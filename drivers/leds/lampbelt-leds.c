// SPDX-License-Identifier: GPL-2.0
/*
 * Unihertz Jelly Star back LED ("lamp belt").
 *
 * The belt is driven by one PWM channel in MediaTek's memory mode: the PWM
 * block streams a DMA buffer bit by bit, repeating it forever, and the share
 * of set bits sets the brightness. GPIOs switch the belt's power and its LED
 * strings on.
 *
 * Written for LineageOS from the device tree node, the mtk-pwm interface and
 * the behaviour of the stock driver observed at runtime (PWM settings, buffer
 * contents and GPIO changes for each brightness).
 *
 * Interface: /sys/bus/platform/drivers/lampbelt_leds/lampbelt_leds_duty takes
 * a brightness from 0 (off) to 255, as the stock driver does.
 */

#include <linux/bits.h>
#include <linux/dma-mapping.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/iio/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

#include <mt-plat/mtk_pwm.h>
#include <mt-plat/mtk_pwm_hal.h>

#define DRV_NAME	"lampbelt_leds"
#define MAX_DUTY	255
#define MAX_EN_GPIOS	4

/* The pattern is 8 words long; BUF0_SIZE holds the index of the last word. */
#define BUF_WORDS	8
#define BUF_BITS	(BUF_WORDS * 32)

struct lampbelt {
	struct device *dev;
	struct mutex lock;
	u32 pwm_no;
	int gpio_pwr;
	int gpio_en[MAX_EN_GPIOS];
	int n_en;
	u32 *buf;
	dma_addr_t buf_dma;
	unsigned int duty;
	bool running;
};

/* Only one belt per phone; the attribute belongs to the driver, not the device. */
static struct lampbelt *the_belt;

/* duty N sets the first N bits of the pattern (255 sets all of them). */
static void lampbelt_fill(struct lampbelt *lb, unsigned int duty)
{
	unsigned int bits = duty >= MAX_DUTY ? BUF_BITS : duty;
	int i;

	for (i = 0; i < BUF_WORDS; i++) {
		if (bits >= 32) {
			lb->buf[i] = 0xffffffff;
			bits -= 32;
		} else {
			lb->buf[i] = bits ? GENMASK(bits - 1, 0) : 0;
			bits = 0;
		}
	}
	/* The buffer is coherent; make sure the stores land before the PWM reads them. */
	wmb();
}

static int lampbelt_start(struct lampbelt *lb)
{
	struct pwm_spec_config conf = {
		.pwm_no = lb->pwm_no,
		.mode = PWM_MODE_MEMORY,
		.clk_div = CLK_DIV128,
		.clk_src = PWM_CLK_NEW_MODE_BLOCK,
		.PWM_MODE_MEMORY_REGS.IDLE_VALUE = 0,
		.PWM_MODE_MEMORY_REGS.GUARD_VALUE = 0,
		.PWM_MODE_MEMORY_REGS.STOP_BITPOS_VALUE = 31,
		.PWM_MODE_MEMORY_REGS.HDURATION = 2,
		.PWM_MODE_MEMORY_REGS.LDURATION = 2,
		.PWM_MODE_MEMORY_REGS.GDURATION = 0,
		.PWM_MODE_MEMORY_REGS.BUF0_BASE_ADDR = lb->buf_dma,
		.PWM_MODE_MEMORY_REGS.BUF0_SIZE = BUF_WORDS - 1,
		.PWM_MODE_MEMORY_REGS.WAVE_NUM = 0,	/* repeat forever */
	};
	int i, ret;

	mt_pwm_disable(lb->pwm_no, 0);
	ret = pwm_set_spec_config(&conf);
	if (ret) {
		dev_err(lb->dev, "pwm_set_spec_config failed: %d\n", ret);
		return -EIO;
	}

	gpio_set_value_cansleep(lb->gpio_pwr, 1);
	for (i = 0; i < lb->n_en; i++)
		gpio_set_value_cansleep(lb->gpio_en[i], 1);

	lb->running = true;
	return 0;
}

static int lampbelt_set(struct lampbelt *lb, unsigned int duty)
{
	int ret = 0;

	mutex_lock(&lb->lock);
	if (duty == 0) {
		/* Like stock: stop the PWM, leave the power and enable lines as they are. */
		if (lb->running)
			mt_pwm_disable(lb->pwm_no, 0);
		lb->running = false;
	} else {
		lampbelt_fill(lb, duty);
		/* While running, the PWM picks up the new pattern on its next pass. */
		if (!lb->running)
			ret = lampbelt_start(lb);
	}
	if (!ret)
		lb->duty = duty;
	mutex_unlock(&lb->lock);
	return ret;
}

static ssize_t lampbelt_leds_duty_store(struct device_driver *drv,
					const char *buf, size_t count)
{
	unsigned int duty;
	int ret;

	if (!the_belt)
		return -ENODEV;

	ret = kstrtouint(buf, 0, &duty);
	if (ret)
		return ret;

	ret = lampbelt_set(the_belt, min_t(unsigned int, duty, MAX_DUTY));
	return ret ? ret : count;
}
static DRIVER_ATTR_WO(lampbelt_leds_duty);

static int lampbelt_get_gpio(struct lampbelt *lb, const char *prop, int index,
			     const char *label)
{
	int gpio = of_get_named_gpio(lb->dev->of_node, prop, index);
	int ret;

	if (gpio < 0)
		return gpio;
	ret = devm_gpio_request_one(lb->dev, gpio, GPIOF_OUT_INIT_LOW, label);
	return ret ? ret : gpio;
}

/* The board ID ADC tells the white belt from the colour one; only log it. */
static void lampbelt_log_variant(struct lampbelt *lb)
{
	struct device_node *np = lb->dev->of_node;
	struct iio_channel *chan;
	u32 white[2], color[2];
	int val, ret;

	chan = iio_channel_get(lb->dev, "lampbelt_id");
	if (IS_ERR(chan))
		return;
	ret = iio_read_channel_processed(chan, &val);
	iio_channel_release(chan);
	if (ret < 0)
		return;

	if (!of_property_read_u32_array(np, "lampbelt_leds,adc_white", white, 2) &&
	    val >= white[0] && val <= white[1])
		dev_info(lb->dev, "white belt (id adc %d)\n", val);
	else if (!of_property_read_u32_array(np, "lampbelt_leds,adc_color", color, 2) &&
		 val >= color[0] && val <= color[1])
		dev_warn(lb->dev, "colour belt (id adc %d): driven like the white one, untested\n",
			 val);
	else
		dev_warn(lb->dev, "unknown belt (id adc %d)\n", val);
}

static int lampbelt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct lampbelt *lb;
	char *label;
	int i, ret;

	if (the_belt)
		return -EBUSY;

	lb = devm_kzalloc(dev, sizeof(*lb), GFP_KERNEL);
	if (!lb)
		return -ENOMEM;
	lb->dev = dev;
	mutex_init(&lb->lock);

	ret = of_property_read_u32(dev->of_node, "lampbelt_leds,pwm_ch", &lb->pwm_no);
	if (ret)
		return dev_err_probe(dev, ret, "no lampbelt_leds,pwm_ch\n");

	/* The stock driver switches only the first power line on. */
	lb->gpio_pwr = lampbelt_get_gpio(lb, "lampbelt_leds,gpio_pwr", 0,
					 "lampbelt_leds,gpio_pwr");
	if (lb->gpio_pwr < 0)
		return dev_err_probe(dev, lb->gpio_pwr, "power gpio\n");

	ret = of_count_phandle_with_args(dev->of_node, "lampbelt_leds,gpio_en", "#gpio-cells");
	lb->n_en = ret > 0 ? min(ret, MAX_EN_GPIOS) : 0;
	for (i = 0; i < lb->n_en; i++) {
		label = devm_kasprintf(dev, GFP_KERNEL, "lampbelt_led%d", i + 1);
		if (!label)
			return -ENOMEM;
		lb->gpio_en[i] = lampbelt_get_gpio(lb, "lampbelt_leds,gpio_en", i, label);
		if (lb->gpio_en[i] < 0)
			return dev_err_probe(dev, lb->gpio_en[i], "enable gpio %d\n", i);
	}

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret)
		return dev_err_probe(dev, ret, "dma mask\n");
	lb->buf = dmam_alloc_coherent(dev, BUF_WORDS * sizeof(u32), &lb->buf_dma, GFP_KERNEL);
	if (!lb->buf)
		return -ENOMEM;

	lampbelt_log_variant(lb);

	platform_set_drvdata(pdev, lb);
	the_belt = lb;
	dev_info(dev, "pwm %u, %d enable lines\n", lb->pwm_no, lb->n_en);
	return 0;
}

static void lampbelt_remove(struct platform_device *pdev)
{
	struct lampbelt *lb = platform_get_drvdata(pdev);

	lampbelt_set(lb, 0);
	the_belt = NULL;
}

static void lampbelt_shutdown(struct platform_device *pdev)
{
	lampbelt_set(platform_get_drvdata(pdev), 0);
}

static const struct of_device_id lampbelt_of_match[] = {
	{ .compatible = "agold,lampbelt_leds" },
	{ }
};
MODULE_DEVICE_TABLE(of, lampbelt_of_match);

static struct platform_driver lampbelt_driver = {
	.probe = lampbelt_probe,
	.remove = lampbelt_remove,
	.shutdown = lampbelt_shutdown,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = lampbelt_of_match,
	},
};

static int __init lampbelt_init(void)
{
	int ret = platform_driver_register(&lampbelt_driver);

	if (ret)
		return ret;
	ret = driver_create_file(&lampbelt_driver.driver, &driver_attr_lampbelt_leds_duty);
	if (ret)
		platform_driver_unregister(&lampbelt_driver);
	return ret;
}
module_init(lampbelt_init);

static void __exit lampbelt_exit(void)
{
	driver_remove_file(&lampbelt_driver.driver, &driver_attr_lampbelt_leds_duty);
	platform_driver_unregister(&lampbelt_driver);
}
module_exit(lampbelt_exit);

MODULE_DESCRIPTION("Unihertz Jelly Star back LED belt");
MODULE_LICENSE("GPL");

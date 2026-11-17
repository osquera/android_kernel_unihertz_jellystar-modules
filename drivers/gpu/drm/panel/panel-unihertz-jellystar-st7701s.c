// SPDX-License-Identifier: GPL-2.0
/*
 * Sitronix ST7701S 480x854 video-mode DSI panel of the Unihertz Jelly Star (MT6789).
 *
 * Written from MediaTek's panel template (panel-boe-jd9365da-vdo.c). The power-on sequence, init
 * commands, display mode and link rate were recorded on a running Jelly Star (kprobes on the DSI
 * write and GPIO calls of Google's GKI kernel); no vendor panel driver code was used.
 */

#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/leds-mtk.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <video/mipi_display.h>

#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"

struct st7701s {
	struct device *dev;
	struct drm_panel panel;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *io_2_8_gpio;
	bool prepared;
	bool enabled;
	int error;
};

static inline struct st7701s *panel_to_st7701s(struct drm_panel *panel)
{
	return container_of(panel, struct st7701s, panel);
}

/* Like MediaTek's panels: DCS commands below 0xB0, manufacturer commands as generic writes. */
static void st7701s_write(struct st7701s *ctx, const u8 *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return;
	if (data[0] < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing %*ph\n", ret, (int)len, data);
		ctx->error = ret;
	}
}

#define st7701s_seq(ctx, seq...)						\
	({									\
		static const u8 d[] = { seq };					\
		st7701s_write(ctx, d, ARRAY_SIZE(d));				\
	})

static void st7701s_init(struct st7701s *ctx)
{
	st7701s_seq(ctx, MIPI_DCS_EXIT_SLEEP_MODE, 0x00);
	msleep(120);
	st7701s_seq(ctx, MIPI_DCS_SET_ADDRESS_MODE, 0x10);

	/* Command2 BK0: display line setting, porch, inversion, gamma */
	st7701s_seq(ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x10);
	st7701s_seq(ctx, 0xC0, 0xE9, 0x03);
	st7701s_seq(ctx, 0xC1, 0x0E, 0x0C);
	st7701s_seq(ctx, 0xC2, 0x07, 0x06);
	st7701s_seq(ctx, 0xC7, 0x00);
	st7701s_seq(ctx, 0xCC, 0x30);
	st7701s_seq(ctx, 0xB0, 0x00, 0x16, 0x1E, 0x0F, 0x13, 0x07, 0x0C, 0x09,
		    0x09, 0x26, 0x04, 0x11, 0x0F, 0x27, 0x2D, 0x13);
	st7701s_seq(ctx, 0xB1, 0x00, 0x16, 0x1E, 0x0F, 0x13, 0x07, 0x0C, 0x09,
		    0x09, 0x26, 0x04, 0x11, 0x0F, 0x27, 0x2D, 0x13);

	/* Command2 BK1: power control */
	st7701s_seq(ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x11);
	st7701s_seq(ctx, 0xB0, 0x7D);
	st7701s_seq(ctx, 0xB1, 0x26);
	st7701s_seq(ctx, 0xB2, 0x81);
	st7701s_seq(ctx, 0xB3, 0x80);
	st7701s_seq(ctx, 0xB5, 0x49);
	st7701s_seq(ctx, 0xB7, 0x87);
	st7701s_seq(ctx, 0xB8, 0x33);
	st7701s_seq(ctx, 0xBA, 0x00);
	st7701s_seq(ctx, 0xBB, 0x00);
	st7701s_seq(ctx, 0xBC, 0x00);
	st7701s_seq(ctx, 0xC1, 0x78);
	st7701s_seq(ctx, 0xC2, 0x78);
	st7701s_seq(ctx, 0xD0, 0x88);
	msleep(10);

	/* GIP (gate driver) timing */
	st7701s_seq(ctx, 0xE0, 0x00, 0x00, 0x02);
	st7701s_seq(ctx, 0xE1, 0x03, 0xA0, 0x05, 0xA0, 0x02, 0xA0, 0x04, 0xA0,
		    0x00, 0x22, 0x22);
	st7701s_seq(ctx, 0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00);
	st7701s_seq(ctx, 0xE3, 0x00, 0x00, 0x33, 0x33);
	st7701s_seq(ctx, 0xE4, 0x44, 0x44);
	st7701s_seq(ctx, 0xE5, 0x06, 0x6A, 0xA0, 0xA0, 0x08, 0x6C, 0xA0, 0xA0,
		    0x0A, 0x6E, 0xA0, 0xA0, 0x0C, 0x70, 0xA0, 0xA0);
	st7701s_seq(ctx, 0xE6, 0x00, 0x00, 0x33, 0x33);
	st7701s_seq(ctx, 0xE7, 0x44, 0x44);
	st7701s_seq(ctx, 0xE8, 0x05, 0x69, 0xA0, 0xA0, 0x07, 0x6B, 0xA0, 0xA0,
		    0x09, 0x6D, 0xA0, 0xA0, 0x0B, 0x6F, 0xA0, 0xA0);
	st7701s_seq(ctx, 0xEB, 0x00, 0x02, 0xE4, 0xE4, 0x11, 0x00, 0x00);
	st7701s_seq(ctx, 0xED, 0xAB, 0xFF, 0x54, 0x76, 0x01, 0xFF, 0xFF, 0xFF,
		    0xFF, 0xFF, 0xFF, 0x10, 0x67, 0x45, 0xFF, 0xBA);
	st7701s_seq(ctx, 0xFF, 0x10, 0x0D, 0x04, 0x08, 0x3F, 0x1F);

	/* Command2 BK3 */
	st7701s_seq(ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x13);
	st7701s_seq(ctx, 0xE5, 0xE4);
	st7701s_seq(ctx, 0xEF, 0x08);

	/* Back to Command1 */
	st7701s_seq(ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x00);
	st7701s_seq(ctx, MIPI_DCS_SET_DISPLAY_ON, 0x00);
	msleep(20);
}

/*
 * The DT has no backlight device for this panel; the backlight is the "lcd-backlight" LED
 * (leds-mtk). Switch it with the panel, so a blanked screen (e.g. in power-off charging) is dark.
 */
static int st7701s_disable(struct drm_panel *panel)
{
	mtk_leds_panel_power(false);
	panel_to_st7701s(panel)->enabled = false;
	return 0;
}

static int st7701s_unprepare(struct drm_panel *panel)
{
	struct st7701s *ctx = panel_to_st7701s(panel);

	if (!ctx->prepared)
		return 0;

	st7701s_seq(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(50);
	st7701s_seq(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(150);
	gpiod_set_value(ctx->reset_gpio, 0);

	ctx->error = 0;
	ctx->prepared = false;
	return 0;
}

static int st7701s_prepare(struct drm_panel *panel)
{
	struct st7701s *ctx = panel_to_st7701s(panel);
	int ret;

	if (ctx->prepared)
		return 0;

	gpiod_set_value(ctx->io_2_8_gpio, 1);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(18);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(16);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(16);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(120);

	st7701s_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		st7701s_unprepare(panel);
	ctx->prepared = true;
	return ret;
}

static int st7701s_enable(struct drm_panel *panel)
{
	panel_to_st7701s(panel)->enabled = true;
	mtk_leds_panel_power(true);
	return 0;
}

/* 28 MHz, 522 x 894 total: 60 Hz. */
static const struct drm_display_mode default_mode = {
	.clock = 28000,
	.hdisplay = 480,
	.hsync_start = 480 + 16,	/* HFP */
	.hsync_end = 480 + 16 + 10,	/* HSA */
	.htotal = 480 + 16 + 10 + 16,	/* HBP */
	.vdisplay = 854,
	.vsync_start = 854 + 18,	/* VFP */
	.vsync_end = 854 + 18 + 2,	/* VSA */
	.vtotal = 854 + 18 + 2 + 20,	/* VBP */
};

static struct mtk_panel_params ext_params = {
	.pll_clk = 348,
	.data_rate = 696,
	.physical_width = 480,
	.physical_height = 854,
};

static int st7701s_ext_reset(struct drm_panel *panel, int on)
{
	gpiod_set_value(panel_to_st7701s(panel)->reset_gpio, on);
	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = st7701s_ext_reset,
};

static int st7701s_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode)
		return -ENOMEM;
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	/* 3.0" panel */
	connector->display_info.width_mm = 37;
	connector->display_info.height_mm = 67;
	return 1;
}

static const struct drm_panel_funcs st7701s_funcs = {
	.disable = st7701s_disable,
	.unprepare = st7701s_unprepare,
	.prepare = st7701s_prepare,
	.enable = st7701s_enable,
	.get_modes = st7701s_get_modes,
};

static int st7701s_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *endpoint, *remote = NULL;
	struct st7701s *ctx;
	int ret;

	/* The DSI node lists several panels; only probe the one its endpoint points at. */
	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote = of_graph_get_remote_port_parent(endpoint);
			of_node_put(endpoint);
		}
		of_node_put(dsi_node);
	}
	of_node_put(remote);
	if (remote != dev->of_node)
		return -ENODEV;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;

	dsi->lanes = 1;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_LPM | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio), "no reset-gpios\n");
	ctx->io_2_8_gpio = devm_gpiod_get(dev, "io_2_8", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->io_2_8_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->io_2_8_gpio), "no io_2_8-gpios\n");

	/* The bootloader has already switched the panel on. */
	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel, dev, &st7701s_funcs, DRM_MODE_CONNECTOR_DSI);
	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0) {
		mipi_dsi_detach(dsi);
		drm_panel_remove(&ctx->panel);
	}
	return ret;
}

static void st7701s_remove(struct mipi_dsi_device *dsi)
{
	struct st7701s *ctx = mipi_dsi_get_drvdata(dsi);
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
}

static const struct of_device_id st7701s_of_match[] = {
	{ .compatible = "zhunyi,st7701,vdo" },
	{}
};
MODULE_DEVICE_TABLE(of, st7701s_of_match);

static struct mipi_dsi_driver st7701s_driver = {
	.probe = st7701s_probe,
	.remove = st7701s_remove,
	.driver = {
		.name = "panel-unihertz-jellystar-st7701s",
		.of_match_table = st7701s_of_match,
	},
};
module_mipi_dsi_driver(st7701s_driver);

MODULE_DESCRIPTION("Unihertz Jelly Star ST7701S video-mode DSI panel");
MODULE_LICENSE("GPL");

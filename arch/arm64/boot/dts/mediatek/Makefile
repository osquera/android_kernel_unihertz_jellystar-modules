# SPDX-License-Identifier: GPL-2.0

# for the include dt-bindings header files in the kernel_device_modules-X.Y
# common, for include/dt-bindings
DTC_INCLUDE += $(DEVICE_MODULES_PATH)/include
# arm64
DTC_INCLUDE += $(DEVICE_MODULES_PATH)/arch/$(SRCARCH)/boot/dts $(DEVICE_MODULES_PATH)/arch/$(SRCARCH)/boot/dts/mediatek

# for cust.dtsi
#DTC_INCLUDE += $(DEVICE_MODULES_REL_PATH)/arch/$(SRCARCH)/boot/dts
DTC_INCLUDE += $(dir $(obj))

# for devicetree overlay
DTC_FLAGS += -@

# Jelly Star: the MT6789 base and its board overlay
dtb-$(CONFIG_ARCH_MEDIATEK) += mt6789.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += mt6789-jellystar-overlay.dtbo

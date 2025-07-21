# firmware/external.mk

# Include all custom packages
include $(sort $(wildcard $(BR2_EXTERNAL_FIRMWARE_PATH)/package/*/*.mk))

# Add custom kernel patch directory
LINUX_PATCHES += $(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-patches/linux-$(BR2_LINUX_KERNEL_VERSION)

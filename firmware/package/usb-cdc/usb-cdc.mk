################################################################################
#
# usb-cdc
#
################################################################################

USB_CDC_VERSION = 1.0
USB_CDC_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/usb-cdc
USB_CDC_SITE_METHOD = local

# Linux is parsed before br2-external package makefiles. Selecting this
# package enables the DWC2 peripheral controller and a CDC ACM data port.
# USART1 remains the kernel console and interactive shell. The USB USER socket
# uses OTG HS with its embedded full-speed PHY.
ifeq ($(BR2_PACKAGE_USB_CDC),y)
LINUX_PATCHES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/package/usb-cdc/0001-usb-dwc2-stm32f4-select-fs-phy-before-reset.patch \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/package/usb-cdc/0002-usb-dwc2-stm32f4-use-pio-for-gadget-transfers.patch
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-usb-cdc.config

endif

define USB_CDC_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) -std=c99 -Wall -Wextra \
		-o $(@D)/usb-cdc $(@D)/usb-cdc.c \
		$(TARGET_LDFLAGS) -Wl,--gc-sections
endef

define USB_CDC_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/usb-cdc \
		$(TARGET_DIR)/usr/bin/usb-cdc
endef

$(eval $(generic-package))

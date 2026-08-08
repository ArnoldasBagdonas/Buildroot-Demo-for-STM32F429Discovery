################################################################################
#
# usbserialdevice
#
################################################################################

USBSERIALDEVICE_VERSION = 1.0
USBSERIALDEVICE_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/usbserialdevice
USBSERIALDEVICE_SITE_METHOD = local

# Linux is parsed before br2-external package makefiles. Selecting this
# package enables the DWC2 peripheral controller and a CDC ACM data port.
# USART1 remains the kernel console and interactive shell. The USB USER socket
# uses OTG HS with its embedded full-speed PHY.
ifeq ($(BR2_PACKAGE_USBSERIALDEVICE),y)
LINUX_PATCHES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/package/usbserialdevice/0001-usb-dwc2-stm32f4-select-fs-phy-before-reset.patch \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/package/usbserialdevice/0002-usb-dwc2-stm32f4-use-pio-for-gadget-transfers.patch
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-usbserialdevice.config

ifeq ($(BR2_PACKAGE_DISPLAYEXAMPLE),y)
USBSERIALDEVICE_DTS = stm32f429disco-usbserialdevice-display
else ifneq ($(filter y,$(BR2_PACKAGE_IOEXAMPLE7) $(BR2_PACKAGE_IOEXAMPLE8)),)
USBSERIALDEVICE_DTS = stm32f429disco-usbserialdevice-usart3
else
USBSERIALDEVICE_DTS = stm32f429disco-usbserialdevice
endif

LINUX_DTS_NAME += $(USBSERIALDEVICE_DTS)

define USBSERIALDEVICE_COPY_DTS
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-usbserialdevice.dtsi \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-usbserialdevice-selected.dtsi
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/$(USBSERIALDEVICE_DTS).dts \
		$(LINUX_ARCH_PATH)/boot/dts/$(USBSERIALDEVICE_DTS).dts
endef
LINUX_PRE_BUILD_HOOKS += USBSERIALDEVICE_COPY_DTS
endif

define USBSERIALDEVICE_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) -std=c99 -Wall -Wextra \
		-o $(@D)/usbserialchat $(@D)/usbserialchat.c \
		$(TARGET_LDFLAGS) -Wl,--gc-sections
endef

define USBSERIALDEVICE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/usbserialchat \
		$(TARGET_DIR)/usr/bin/usbserialchat
endef

$(eval $(generic-package))

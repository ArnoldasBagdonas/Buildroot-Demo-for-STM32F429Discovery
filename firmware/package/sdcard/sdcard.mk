################################################################################
#
# sdcard
#
################################################################################

SDCARD_VERSION = 1.0
SDCARD_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/sdcard
SDCARD_SITE_METHOD = local

ifeq ($(BR2_PACKAGE_DISPLAY),y)
SDCARD_DEPENDENCIES = display
else
define SDCARD_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-o $(@D)/sdcard $(@D)/sdcard.c
endef
endif

ifeq ($(BR2_PACKAGE_SDCARD),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-sdcard.config
ifeq ($(BR2_PACKAGE_USBSERIALDEVICE),y)
ifeq ($(BR2_PACKAGE_DISPLAY),y)
SDCARD_BASE_DTS = stm32f429disco-usbserialdevice-display
else
SDCARD_BASE_DTS = stm32f429disco-usbserialdevice
endif
else ifeq ($(BR2_PACKAGE_DISPLAY),y)
SDCARD_BASE_DTS = stm32f429disco-display
else
SDCARD_BASE_DTS = stm32f429disco-custom
endif

SDCARD_DTS = $(SDCARD_BASE_DTS)-sdcard
LINUX_DTS_NAME += $(SDCARD_DTS)

define SDCARD_COPY_DTS
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-sdcard.dtsi \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-sdcard.dtsi
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/$(SDCARD_DTS).dts \
		$(LINUX_ARCH_PATH)/boot/dts/$(SDCARD_DTS).dts
endef
LINUX_PRE_BUILD_HOOKS += SDCARD_COPY_DTS
endif

ifeq ($(BR2_PACKAGE_DISPLAY),y)
define SDCARD_INSTALL_HELPER
	ln -sf ../bin/display $(TARGET_DIR)/usr/sbin/sdcard
endef
else
define SDCARD_INSTALL_HELPER
	$(INSTALL) -D -m 0755 $(@D)/sdcard \
		$(TARGET_DIR)/usr/sbin/sdcard
endef
endif

define SDCARD_INSTALL_TARGET_CMDS
	$(SDCARD_INSTALL_HELPER)
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/mnt/sdcard
endef

$(eval $(generic-package))

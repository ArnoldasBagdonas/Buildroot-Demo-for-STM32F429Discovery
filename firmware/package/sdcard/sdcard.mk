################################################################################
#
# sdcard
#
################################################################################

SDCARD_VERSION = 1.0
SDCARD_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/sdcard
SDCARD_SITE_METHOD = local

ifeq ($(BR2_PACKAGE_SDCARD_AUTODETECT),y)
SDCARD_CFLAGS += -DSDCARD_AUTOMOUNT_PERIODIC
else ifeq ($(BR2_PACKAGE_SDCARD_AUTOMOUNT),y)
SDCARD_CFLAGS += -DSDCARD_AUTOMOUNT_ONCE
endif

define SDCARD_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		$(SDCARD_CFLAGS) -o $(@D)/sdcard $(@D)/sdcard.c
endef

ifeq ($(BR2_PACKAGE_SDCARD),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-compact.config \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-sdcard.config

ifeq ($(BR2_PACKAGE_FIND_MY_DEVICE),y)
# Package makefiles are included alphabetically, so re-apply Find My Device's
# feature requirements after the size-oriented SD-card fragment.
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-find-my-device.config
else
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-no-network.config
endif

define SDCARD_COPY_DTSI
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-sdcard.dtsi \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-sdcard.dtsi
endef
LINUX_PRE_BUILD_HOOKS += SDCARD_COPY_DTSI

ifneq ($(BR2_PACKAGE_FIND_MY_DEVICE),y)
ifeq ($(BR2_PACKAGE_USBSERIALDEVICE),y)
ifneq ($(filter y,$(BR2_PACKAGE_DISPLAY) $(BR2_PACKAGE_GALLERY) $(BR2_PACKAGE_FIRMWARE_SCREEN)),)
SDCARD_BASE_DTS = stm32f429disco-usbserialdevice-display
else
SDCARD_BASE_DTS = stm32f429disco-usbserialdevice
endif
else
ifneq ($(filter y,$(BR2_PACKAGE_DISPLAY) $(BR2_PACKAGE_GALLERY) $(BR2_PACKAGE_FIRMWARE_SCREEN)),)
SDCARD_BASE_DTS = stm32f429disco-display
else
SDCARD_BASE_DTS = stm32f429disco-custom
endif
endif

SDCARD_DTS = $(SDCARD_BASE_DTS)-sdcard
LINUX_DTS_NAME += $(SDCARD_DTS)

define SDCARD_COPY_DTS
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/$(SDCARD_DTS).dts \
		$(LINUX_ARCH_PATH)/boot/dts/$(SDCARD_DTS).dts
endef
LINUX_PRE_BUILD_HOOKS += SDCARD_COPY_DTS
endif
endif

define SDCARD_INSTALL_HELPER
	$(INSTALL) -D -m 0755 $(@D)/sdcard \
		$(TARGET_DIR)/usr/sbin/sdcard
endef

define SDCARD_INSTALL_TARGET_CMDS
	$(SDCARD_INSTALL_HELPER)
	$(RM) -f $(TARGET_DIR)/usr/sbin/sdcard-auto
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/mnt/sdcard
endef

ifeq ($(BR2_PACKAGE_SDCARD_AUTOMOUNT),y)
define SDCARD_INSTALL_AUTOMOUNT_LINK
	ln -sf sdcard $(TARGET_DIR)/usr/sbin/sdcard-auto
endef
SDCARD_POST_INSTALL_TARGET_HOOKS += SDCARD_INSTALL_AUTOMOUNT_LINK
endif

$(eval $(generic-package))

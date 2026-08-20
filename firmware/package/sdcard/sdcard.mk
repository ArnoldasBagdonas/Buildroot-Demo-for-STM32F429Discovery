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
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-sdcard.config

endif

define SDCARD_INSTALL_HELPER
	$(INSTALL) -D -m 0755 $(@D)/sdcard \
		$(TARGET_DIR)/usr/sbin/sdcard
endef

define SDCARD_INSTALL_TARGET_CMDS
	$(SDCARD_INSTALL_HELPER)
	$(RM) -f $(TARGET_DIR)/usr/sbin/sdcard-auto
	$(RM) -f $(TARGET_DIR)/etc/init.d/S20sdcard
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/mnt/sdcard
endef

ifeq ($(BR2_PACKAGE_SDCARD_AUTOMOUNT),y)
define SDCARD_INSTALL_AUTOMOUNT_LINK
	ln -sf sdcard $(TARGET_DIR)/usr/sbin/sdcard-auto
	$(INSTALL) -D -m 0755 $(@D)/S20sdcard-once \
		$(TARGET_DIR)/etc/init.d/S20sdcard
endef
SDCARD_POST_INSTALL_TARGET_HOOKS += SDCARD_INSTALL_AUTOMOUNT_LINK
endif

ifeq ($(BR2_PACKAGE_SDCARD_AUTODETECT),y)
define SDCARD_INSTALL_AUTODETECT_SERVICE
	$(INSTALL) -D -m 0755 $(@D)/S20sdcard \
		$(TARGET_DIR)/etc/init.d/S20sdcard
endef
SDCARD_POST_INSTALL_TARGET_HOOKS += SDCARD_INSTALL_AUTODETECT_SERVICE
endif

$(eval $(generic-package))

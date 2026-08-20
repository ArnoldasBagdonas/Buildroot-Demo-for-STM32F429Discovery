################################################################################
#
# firmware-screen
#
################################################################################

FIRMWARE_SCREEN_VERSION = 1.0
FIRMWARE_SCREEN_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/screen
FIRMWARE_SCREEN_SITE_METHOD = local
FIRMWARE_SCREEN_LICENSE = MIT
FIRMWARE_SCREEN_LICENSE_FILES = LICENSE
FIRMWARE_SCREEN_FLAT_STACKSIZE = 16384

define FIRMWARE_SCREEN_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) -std=c99 -Wall -Wextra \
		-o $(@D)/screen $(@D)/screen.c \
		$(TARGET_LDFLAGS) -Wl,--gc-sections
endef

ifeq ($(BR2_PACKAGE_FIRMWARE_SCREEN),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-display.config

ifeq ($(BR2_PACKAGE_FIRMWARE_SCREEN_COMPRESS_INITRAMFS),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-initramfs-gzip.config
endif

endif

define FIRMWARE_SCREEN_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/screen $(TARGET_DIR)/usr/bin/screen
	$(RM) -f $(TARGET_DIR)/usr/bin/screen-auto
	$(RM) -f $(TARGET_DIR)/etc/init.d/S30screen
endef

ifeq ($(BR2_PACKAGE_FIRMWARE_SCREEN_AUTOSTART),y)
define FIRMWARE_SCREEN_INSTALL_AUTOSTART
	ln -sf screen $(TARGET_DIR)/usr/bin/screen-auto
	$(INSTALL) -D -m 0755 $(@D)/S30screen \
		$(TARGET_DIR)/etc/init.d/S30screen
endef
FIRMWARE_SCREEN_POST_INSTALL_TARGET_HOOKS += FIRMWARE_SCREEN_INSTALL_AUTOSTART
endif

$(eval $(generic-package))

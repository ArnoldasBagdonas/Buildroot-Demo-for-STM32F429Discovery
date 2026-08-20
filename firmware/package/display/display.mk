################################################################################
#
# display
#
################################################################################

DISPLAY_VERSION = 1.0
DISPLAY_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/display
DISPLAY_SITE_METHOD = local
DISPLAY_DEPENDENCIES = fbv

define DISPLAY_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-o $(@D)/display $(@D)/display.c $(@D)/display-pattern.c
endef

# Linux is parsed before br2-external package makefiles. Appending here makes
# the display kernel configuration follow the package selection.
ifeq ($(BR2_PACKAGE_DISPLAY),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-display.config

ifeq ($(BR2_PACKAGE_DISPLAY_COMPRESS_INITRAMFS),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-initramfs-gzip.config
endif

endif

define DISPLAY_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/display \
		$(TARGET_DIR)/usr/bin/display
	ln -sf display $(TARGET_DIR)/usr/bin/display-pattern
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/usr/share/display
	$(INSTALL) -m 0644 $(@D)/images/*.png $(@D)/images/*.jpg \
		$(@D)/images/*.gif $(TARGET_DIR)/usr/share/display/
	$(RM) -f $(TARGET_DIR)/etc/init.d/S30display
endef

ifeq ($(BR2_PACKAGE_DISPLAY_AUTOSTART),y)
define DISPLAY_INSTALL_AUTOSTART
	ln -sf display $(TARGET_DIR)/usr/bin/display-auto
	$(INSTALL) -D -m 0755 $(@D)/S30display \
		$(TARGET_DIR)/etc/init.d/S30display
endef
DISPLAY_POST_INSTALL_TARGET_HOOKS += DISPLAY_INSTALL_AUTOSTART
endif

$(eval $(generic-package))

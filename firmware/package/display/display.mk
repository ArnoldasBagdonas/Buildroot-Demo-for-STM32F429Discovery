################################################################################
#
# display
#
################################################################################

DISPLAY_VERSION = 1.0
DISPLAY_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/display
DISPLAY_SITE_METHOD = local
DISPLAY_DEPENDENCIES = fbv

ifeq ($(BR2_PACKAGE_SDCARD),y)
DISPLAY_SDCARD_FLAGS = -DWITH_SDCARD -DSDCARD_MULTICALL
DISPLAY_SDCARD_SOURCE = \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/package/sdcard/sdcard.c
endif

define DISPLAY_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		$(DISPLAY_SDCARD_FLAGS) -o $(@D)/display \
		$(@D)/display.c $(@D)/display-pattern.c $(DISPLAY_SDCARD_SOURCE)
endef

# Linux is parsed before br2-external package makefiles. Appending here makes
# the display kernel configuration and DTB follow the package selection while
# leaving the tracked minimal kernel and device tree unchanged.
ifeq ($(BR2_PACKAGE_DISPLAY),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-display.config
LINUX_DTS_NAME += stm32f429disco-display

define DISPLAY_COPY_DTS
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-display.dts \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-display.dts
endef
LINUX_PRE_BUILD_HOOKS += DISPLAY_COPY_DTS
endif

define DISPLAY_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/display \
		$(TARGET_DIR)/usr/bin/display
	ln -sf display $(TARGET_DIR)/usr/bin/display-pattern
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/usr/share/display
	$(INSTALL) -m 0644 $(@D)/images/*.png $(@D)/images/*.jpg \
		$(@D)/images/*.gif $(TARGET_DIR)/usr/share/display/
endef

ifeq ($(BR2_PACKAGE_DISPLAY_AUTOSTART),y)
define DISPLAY_INSTALL_AUTOSTART
	ln -sf display $(TARGET_DIR)/usr/bin/display-auto
endef
DISPLAY_POST_INSTALL_TARGET_HOOKS += DISPLAY_INSTALL_AUTOSTART
endif

$(eval $(generic-package))

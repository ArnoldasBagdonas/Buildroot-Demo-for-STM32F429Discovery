################################################################################
#
# displayexample
#
################################################################################

DISPLAYEXAMPLE_VERSION = 1.0
DISPLAYEXAMPLE_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/displayexample
DISPLAYEXAMPLE_SITE_METHOD = local
DISPLAYEXAMPLE_DEPENDENCIES = fbv

define DISPLAYEXAMPLE_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-o $(@D)/fbpattern $(@D)/fbpattern.c
endef

# Linux is parsed before br2-external package makefiles. Appending here makes
# the display kernel configuration and DTB follow the package selection while
# leaving the tracked minimal kernel and device tree unchanged.
ifeq ($(BR2_PACKAGE_DISPLAYEXAMPLE),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-display.config
LINUX_DTS_NAME += stm32f429disco-display

define DISPLAYEXAMPLE_COPY_DTS
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-display.dts \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-display.dts
endef
LINUX_PRE_BUILD_HOOKS += DISPLAYEXAMPLE_COPY_DTS
endif

define DISPLAYEXAMPLE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/displayexample \
		$(TARGET_DIR)/usr/bin/displayexample
	$(INSTALL) -D -m 0755 $(@D)/fbpattern \
		$(TARGET_DIR)/usr/bin/fbpattern
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/usr/share/displayexample
	$(INSTALL) -m 0644 $(@D)/images/*.png $(@D)/images/*.jpg \
		$(@D)/images/*.gif $(TARGET_DIR)/usr/share/displayexample/
endef

$(eval $(generic-package))

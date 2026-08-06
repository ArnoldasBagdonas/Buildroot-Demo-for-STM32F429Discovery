################################################################################
#
# displaydebug
#
################################################################################

DISPLAYDEBUG_VERSION = 1.0
DISPLAYDEBUG_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/displaydebug
DISPLAYDEBUG_SITE_METHOD = local

# Linux and BusyBox are parsed before br2-external package makefiles. Append
# reusable diagnostics only when this package is selected, keeping every
# normal example image small.
ifeq ($(BR2_PACKAGE_DISPLAYDEBUG),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-displaydebug.config
BUSYBOX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/busybox-displaydebug.config
endif

define DISPLAYDEBUG_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/displaydebug \
		$(TARGET_DIR)/usr/bin/displaydebug
endef

$(eval $(generic-package))

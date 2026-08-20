################################################################################
#
# sysdiag
#
################################################################################

SYSDIAG_VERSION = 1.0
SYSDIAG_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/sysdiag
SYSDIAG_SITE_METHOD = local

# Linux and BusyBox are parsed before br2-external package makefiles. Append
# reusable diagnostics only when this package is selected, keeping every
# normal example image small.
ifeq ($(BR2_PACKAGE_SYSDIAG),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-sysdiag.config
BUSYBOX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/busybox-sysdiag.config
endif

define SYSDIAG_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/sysdiag \
		$(TARGET_DIR)/usr/bin/sysdiag
endef

$(eval $(generic-package))

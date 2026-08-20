################################################################################
#
# spinor
#
################################################################################

SPINOR_VERSION = 1.0
SPINOR_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/spinor
SPINOR_SITE_METHOD = local

ifeq ($(BR2_PACKAGE_SPINOR),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-spinor.config
BUSYBOX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/busybox-spinor.config

ifeq ($(BR2_PACKAGE_SPINOR_COMPRESS_INITRAMFS),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-initramfs-gzip.config
endif

ifeq ($(BR2_PACKAGE_SPINOR_SUMMARY),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-spinor-summary.config
endif

endif

define SPINOR_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/spinor-jffs2 \
		$(TARGET_DIR)/usr/sbin/spinor-jffs2
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/mnt/spinor
	rm -f $(TARGET_DIR)/usr/sbin/spinor-jffs2-auto \
		$(TARGET_DIR)/usr/sbin/spinor-ubi \
		$(TARGET_DIR)/usr/sbin/spinor-ubi-auto \
		$(TARGET_DIR)/etc/spinor-ubi-fastmap \
		$(TARGET_DIR)/etc/init.d/S10spinor
	$(SPINOR_INSTALL_AUTOMOUNT)
endef

ifeq ($(BR2_PACKAGE_SPINOR_AUTOMOUNT),y)
define SPINOR_INSTALL_AUTOMOUNT
	ln -sf spinor-jffs2 $(TARGET_DIR)/usr/sbin/spinor-jffs2-auto
	$(INSTALL) -D -m 0755 $(@D)/S10spinor \
		$(TARGET_DIR)/etc/init.d/S10spinor
endef
endif

$(eval $(generic-package))

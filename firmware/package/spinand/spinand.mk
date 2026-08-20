################################################################################
#
# spinand
#
################################################################################

SPINAND_VERSION = 1.0
SPINAND_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/spinand
SPINAND_SITE_METHOD = local

ifeq ($(BR2_PACKAGE_SPINAND),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-spinand.config

BUSYBOX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/busybox-spinand.config

ifeq ($(BR2_PACKAGE_SPINAND_COMPRESS_INITRAMFS),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-initramfs-gzip.config
endif

ifeq ($(BR2_PACKAGE_SPINAND_FASTMAP),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-spinand-fastmap.config
endif

endif

define SPINAND_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/spinand-ubi \
		$(TARGET_DIR)/usr/sbin/spinand-ubi
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/mnt/spinand
	rm -f $(TARGET_DIR)/usr/sbin/spinand-ubi-auto
	rm -f $(TARGET_DIR)/etc/spinand-ubi-fastmap \
		$(TARGET_DIR)/etc/init.d/S11spinand
	$(SPINAND_INSTALL_AUTOMOUNT)
	$(SPINAND_INSTALL_FASTMAP)
endef

ifeq ($(BR2_PACKAGE_SPINAND_AUTOMOUNT),y)
define SPINAND_INSTALL_AUTOMOUNT
	ln -sf spinand-ubi $(TARGET_DIR)/usr/sbin/spinand-ubi-auto
	$(INSTALL) -D -m 0755 $(@D)/S11spinand \
		$(TARGET_DIR)/etc/init.d/S11spinand
endef
endif

ifeq ($(BR2_PACKAGE_SPINAND_FASTMAP),y)
define SPINAND_INSTALL_FASTMAP
	$(INSTALL) -D -m 0644 $(@D)/spinand-ubi-fastmap \
		$(TARGET_DIR)/etc/spinand-ubi-fastmap
endef
endif

$(eval $(generic-package))

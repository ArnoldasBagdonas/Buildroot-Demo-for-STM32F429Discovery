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

ifeq ($(BR2_PACKAGE_FIND_MY_DEVICE),y)
# SPI-NAND is parsed after Find My Device. Re-apply the network requirements
# after the storage fragment so the two independently owned features compose.
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-find-my-device.config
else
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-no-network.config
endif

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

ifeq ($(BR2_PACKAGE_USBSERIALDEVICE),y)
ifneq ($(filter y,$(BR2_PACKAGE_DISPLAY) $(BR2_PACKAGE_GALLERY)),)
SPINAND_BASE_DTS = stm32f429disco-usbserialdevice-display
else
SPINAND_BASE_DTS = stm32f429disco-usbserialdevice
endif
else ifneq ($(filter y,$(BR2_PACKAGE_DISPLAY) $(BR2_PACKAGE_GALLERY)),)
SPINAND_BASE_DTS = stm32f429disco-display
else
SPINAND_BASE_DTS = stm32f429disco-custom
endif

ifeq ($(BR2_PACKAGE_FIND_MY_DEVICE),y)
SPINAND_DTS = $(SPINAND_BASE_DTS)-w5500-spinand
else ifneq ($(filter y,$(BR2_PACKAGE_SDCARD) $(BR2_PACKAGE_GALLERY_SDCARD)),)
SPINAND_DTS = $(SPINAND_BASE_DTS)-spinand-sdcard
LINUX_DTS_NAME += $(SPINAND_DTS)
else
SPINAND_DTS = $(SPINAND_BASE_DTS)-spinand
LINUX_DTS_NAME += $(SPINAND_DTS)
endif

define SPINAND_COPY_DTSI
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-spinand.dtsi \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-spinand.dtsi
endef
LINUX_PRE_BUILD_HOOKS += SPINAND_COPY_DTSI

ifneq ($(BR2_PACKAGE_FIND_MY_DEVICE),y)
define SPINAND_COPY_STANDALONE_DTS
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/$(SPINAND_DTS).dts \
		$(LINUX_ARCH_PATH)/boot/dts/$(SPINAND_DTS).dts
endef
LINUX_PRE_BUILD_HOOKS += SPINAND_COPY_STANDALONE_DTS
endif
endif

define SPINAND_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/spinand-ubi \
		$(TARGET_DIR)/usr/sbin/spinand-ubi
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/mnt/spinand
	rm -f $(TARGET_DIR)/usr/sbin/spinand-ubi-auto
	rm -f $(TARGET_DIR)/etc/spinand-ubi-fastmap
	$(SPINAND_INSTALL_AUTOMOUNT)
	$(SPINAND_INSTALL_FASTMAP)
endef

ifeq ($(BR2_PACKAGE_SPINAND_AUTOMOUNT),y)
define SPINAND_INSTALL_AUTOMOUNT
	ln -sf spinand-ubi $(TARGET_DIR)/usr/sbin/spinand-ubi-auto
endef
endif

ifeq ($(BR2_PACKAGE_SPINAND_FASTMAP),y)
define SPINAND_INSTALL_FASTMAP
	$(INSTALL) -D -m 0644 $(@D)/spinand-ubi-fastmap \
		$(TARGET_DIR)/etc/spinand-ubi-fastmap
endef
endif

$(eval $(generic-package))

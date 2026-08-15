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

ifeq ($(BR2_PACKAGE_USBSERIALDEVICE),y)
SPINAND_BASE_DTS = stm32f429disco-usbserialdevice
else
SPINAND_BASE_DTS = stm32f429disco-custom
endif

SPINAND_DTS = $(SPINAND_BASE_DTS)-spinand
LINUX_DTS_NAME += $(SPINAND_DTS)

define SPINAND_COPY_DTS
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-spinand.dtsi \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-spinand.dtsi
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/$(SPINAND_DTS).dts \
		$(LINUX_ARCH_PATH)/boot/dts/$(SPINAND_DTS).dts
endef
LINUX_PRE_BUILD_HOOKS += SPINAND_COPY_DTS
endif

define SPINAND_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/spinand-ubi \
		$(TARGET_DIR)/usr/sbin/spinand-ubi
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/mnt/spinand
endef

$(eval $(generic-package))

################################################################################
#
# networking
#
################################################################################

NETWORKING_VERSION = 1.0
NETWORKING_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/networking
NETWORKING_SITE_METHOD = local

ifeq ($(BR2_PACKAGE_NETWORKING),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-networking.config
BUSYBOX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/busybox-networking.config

ifeq ($(BR2_PACKAGE_NETWORKING_W5500),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-networking-w5500.config
endif
endif

$(eval $(generic-package))

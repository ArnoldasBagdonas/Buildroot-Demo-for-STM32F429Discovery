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

SPINOR_DEVICE_DTSI = stm32f429disco-spinor.dtsi

ifeq ($(BR2_PACKAGE_SPINAND),y)
ifneq ($(filter y,$(BR2_PACKAGE_SDCARD) $(BR2_PACKAGE_GALLERY_SDCARD)),)
SPINOR_COMPOSITION_DTSI = stm32f429disco-sdcard-spinand-spinor.dtsi
else
SPINOR_COMPOSITION_DTSI = stm32f429disco-spinand-spinor.dtsi
endif
else ifneq ($(filter y,$(BR2_PACKAGE_SDCARD) $(BR2_PACKAGE_GALLERY_SDCARD)),)
SPINOR_COMPOSITION_DTSI = stm32f429disco-sdcard-spinor.dtsi
else
SPINOR_COMPOSITION_DTSI = $(SPINOR_DEVICE_DTSI)
endif

define SPINOR_COPY_DTSI
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/$(SPINOR_DEVICE_DTSI) \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-spinor-device.dtsi
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/$(SPINOR_COMPOSITION_DTSI) \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-spinor-config.dtsi
endef
LINUX_PRE_BUILD_HOOKS += SPINOR_COPY_DTSI

ifneq ($(BR2_PACKAGE_FIND_MY_DEVICE),y)
ifeq ($(BR2_PACKAGE_USBSERIALDEVICE),y)
ifneq ($(filter y,$(BR2_PACKAGE_DISPLAY) $(BR2_PACKAGE_GALLERY) $(BR2_PACKAGE_FIRMWARE_SCREEN)),)
SPINOR_BASE_DTS = stm32f429disco-usbserialdevice-display
else
SPINOR_BASE_DTS = stm32f429disco-usbserialdevice
endif
else ifneq ($(filter y,$(BR2_PACKAGE_DISPLAY) $(BR2_PACKAGE_GALLERY) $(BR2_PACKAGE_FIRMWARE_SCREEN)),)
SPINOR_BASE_DTS = stm32f429disco-display
else
SPINOR_BASE_DTS = stm32f429disco-custom
endif

SPINOR_DTS = $(SPINOR_BASE_DTS)-spinor
LINUX_DTS_NAME += $(SPINOR_DTS)

define SPINOR_COPY_DTS
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/$(SPINOR_DTS).dts \
		$(LINUX_ARCH_PATH)/boot/dts/$(SPINOR_DTS).dts
endef
LINUX_PRE_BUILD_HOOKS += SPINOR_COPY_DTS
endif
endif

define SPINOR_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/spinor-jffs2 \
		$(TARGET_DIR)/usr/sbin/spinor-jffs2
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/mnt/spinor
	rm -f $(TARGET_DIR)/usr/sbin/spinor-jffs2-auto \
		$(TARGET_DIR)/usr/sbin/spinor-ubi \
		$(TARGET_DIR)/usr/sbin/spinor-ubi-auto \
		$(TARGET_DIR)/etc/spinor-ubi-fastmap
	$(SPINOR_INSTALL_AUTOMOUNT)
endef

ifeq ($(BR2_PACKAGE_SPINOR_AUTOMOUNT),y)
define SPINOR_INSTALL_AUTOMOUNT
	ln -sf spinor-jffs2 $(TARGET_DIR)/usr/sbin/spinor-jffs2-auto
endef
endif

$(eval $(generic-package))

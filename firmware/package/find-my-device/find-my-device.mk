################################################################################
#
# find-my-device
#
################################################################################

FIND_MY_DEVICE_VERSION = 1.0
FIND_MY_DEVICE_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/find-my-device
FIND_MY_DEVICE_SITE_METHOD = local
FIND_MY_DEVICE_LICENSE = MIT
FIND_MY_DEVICE_LICENSE_FILES = LICENSE
# main/application_run plus the HTTP/WebSocket request path requires about
# 12 KiB before libc and signal frames. The bFLT default is only 4 KiB.
FIND_MY_DEVICE_FLAT_STACKSIZE = 32768
FIND_MY_DEVICE_CPPFLAGS = $(TARGET_CPPFLAGS)

ifeq ($(BR2_PACKAGE_FIND_MY_DEVICE_CONSOLE_DEBUG),y)
FIND_MY_DEVICE_CPPFLAGS += -DFMD_CONSOLE_DEBUG

define FIND_MY_DEVICE_INSTALL_CONSOLE_DEBUG
	$(INSTALL) -D -m 0755 $(@D)/find-my-device-debug \
		$(TARGET_DIR)/usr/bin/find-my-device-debug
	$(INSTALL) -D -m 0755 $(@D)/find-my-device-confirm \
		$(TARGET_DIR)/usr/bin/find-my-device-confirm
endef
endif

ifeq ($(BR2_PACKAGE_FIND_MY_DEVICE),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-find-my-device.config
BUSYBOX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/busybox-find-my-device.config

ifeq ($(BR2_PACKAGE_FIND_MY_DEVICE_COMPRESS_INITRAMFS),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-initramfs-gzip.config
endif

ifeq ($(BR2_PACKAGE_USBSERIALDEVICE),y)
ifneq ($(filter y,$(BR2_PACKAGE_DISPLAY) $(BR2_PACKAGE_GALLERY)),)
FIND_MY_DEVICE_BASE_DTS = stm32f429disco-usbserialdevice-display
else
FIND_MY_DEVICE_BASE_DTS = stm32f429disco-usbserialdevice
endif
else ifneq ($(filter y,$(BR2_PACKAGE_DISPLAY) $(BR2_PACKAGE_GALLERY)),)
FIND_MY_DEVICE_BASE_DTS = stm32f429disco-display
else
FIND_MY_DEVICE_BASE_DTS = stm32f429disco-custom
endif

ifeq ($(BR2_PACKAGE_SPINAND),y)
FIND_MY_DEVICE_DTS = $(FIND_MY_DEVICE_BASE_DTS)-w5500-spinand
else ifeq ($(BR2_PACKAGE_SDCARD),y)
FIND_MY_DEVICE_DTS = $(FIND_MY_DEVICE_BASE_DTS)-w5500-sdcard
else
FIND_MY_DEVICE_DTS = $(FIND_MY_DEVICE_BASE_DTS)-w5500
endif

LINUX_DTS_NAME += $(FIND_MY_DEVICE_DTS)

define FIND_MY_DEVICE_COPY_DTS
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-find-my-device.dtsi \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-find-my-device.dtsi
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/$(FIND_MY_DEVICE_DTS).dts \
		$(LINUX_ARCH_PATH)/boot/dts/$(FIND_MY_DEVICE_DTS).dts
endef
LINUX_PRE_BUILD_HOOKS += FIND_MY_DEVICE_COPY_DTS
endif

define FIND_MY_DEVICE_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/src clean
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/src \
		CC="$(TARGET_CC)" \
		CPPFLAGS="$(FIND_MY_DEVICE_CPPFLAGS)" \
		CFLAGS='$(TARGET_CFLAGS) -ffunction-sections -fdata-sections' \
		LDFLAGS='$(TARGET_LDFLAGS) -Wl,--gc-sections'
endef

define FIND_MY_DEVICE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/src/find-my-device \
		$(TARGET_DIR)/usr/bin/find-my-device
	$(INSTALL) -D -m 0755 $(@D)/find-my-device-start \
		$(TARGET_DIR)/usr/bin/find-my-device-start
	$(FIND_MY_DEVICE_INSTALL_CONSOLE_DEBUG)
	$(INSTALL) -D -m 0644 $(@D)/find-my-device.conf \
		$(TARGET_DIR)/etc/find-my-device.conf
	$(INSTALL) -D -m 0755 $(@D)/udhcpc.script \
		$(TARGET_DIR)/usr/share/find-my-device/udhcpc.script
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/var/lib/find-my-device
endef

$(eval $(generic-package))

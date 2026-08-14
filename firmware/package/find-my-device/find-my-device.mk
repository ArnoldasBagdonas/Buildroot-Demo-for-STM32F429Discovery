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

ifeq ($(BR2_PACKAGE_FIND_MY_DEVICE),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-find-my-device.config
BUSYBOX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/busybox-find-my-device.config

ifeq ($(BR2_PACKAGE_USBSERIALDEVICE),y)
ifeq ($(BR2_PACKAGE_DISPLAYEXAMPLE),y)
FIND_MY_DEVICE_DTS = stm32f429disco-usbserialdevice-display-w5500
else
FIND_MY_DEVICE_DTS = stm32f429disco-usbserialdevice-w5500
endif
else ifeq ($(BR2_PACKAGE_DISPLAYEXAMPLE),y)
FIND_MY_DEVICE_DTS = stm32f429disco-display-w5500
else
FIND_MY_DEVICE_DTS = stm32f429disco-custom-w5500
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
		CPPFLAGS="$(TARGET_CPPFLAGS)" \
		CFLAGS='$(TARGET_CFLAGS) -ffunction-sections -fdata-sections' \
		LDFLAGS='$(TARGET_LDFLAGS) -Wl,--gc-sections'
endef

define FIND_MY_DEVICE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/src/find-my-device \
		$(TARGET_DIR)/usr/bin/find-my-device
	$(INSTALL) -D -m 0755 $(@D)/find-my-device-start \
		$(TARGET_DIR)/usr/bin/find-my-device-start
	$(INSTALL) -D -m 0755 $(@D)/find-my-device-confirm \
		$(TARGET_DIR)/usr/bin/find-my-device-confirm
	$(INSTALL) -D -m 0644 $(@D)/find-my-device.conf \
		$(TARGET_DIR)/etc/find-my-device.conf
	$(INSTALL) -D -m 0755 $(@D)/udhcpc.script \
		$(TARGET_DIR)/usr/share/find-my-device/udhcpc.script
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/var/lib/find-my-device
endef

$(eval $(generic-package))

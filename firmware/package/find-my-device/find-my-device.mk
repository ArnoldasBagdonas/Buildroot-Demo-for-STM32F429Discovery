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
FIND_MY_DEVICE_STATE_SOURCES =

ifeq ($(BR2_PACKAGE_FIND_MY_DEVICE_FRAM),y)
FIND_MY_DEVICE_CPPFLAGS += -DFMD_FRAM_STATE
endif

ifeq ($(BR2_PACKAGE_FIND_MY_DEVICE_EEPROM),y)
FIND_MY_DEVICE_CPPFLAGS += -DFMD_EEPROM_STATE
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-find-my-device-eeprom.config

define FIND_MY_DEVICE_INSTALL_EEPROM_MARKER
	$(INSTALL) -D -m 0644 /dev/null \
		$(TARGET_DIR)/etc/find-my-device-eeprom
endef
endif

ifneq ($(filter y,$(BR2_PACKAGE_FIND_MY_DEVICE_FRAM) $(BR2_PACKAGE_FIND_MY_DEVICE_EEPROM)),)
FIND_MY_DEVICE_STATE_SOURCES += nvmem_state.c
endif

ifeq ($(BR2_PACKAGE_FIND_MY_DEVICE_SPINOR_STATE),y)
FIND_MY_DEVICE_CPPFLAGS += -DFMD_SPINOR_STATE
FIND_MY_DEVICE_STATE_SOURCES += spinor_state.c
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-find-my-device-spinor.config

ifneq ($(BR2_PACKAGE_SPINOR),y)
define FIND_MY_DEVICE_COPY_RAW_SPINOR_DTSI
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-spinor-state.dtsi \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-spinor-device.dtsi
endef
LINUX_PRE_BUILD_HOOKS += FIND_MY_DEVICE_COPY_RAW_SPINOR_DTSI
endif

define FIND_MY_DEVICE_INSTALL_SPINOR_STATE_MARKER
	$(INSTALL) -D -m 0644 /dev/null \
		$(TARGET_DIR)/etc/find-my-device-spinor-state
endef
endif

ifneq ($(filter y,$(BR2_PACKAGE_FIND_MY_DEVICE_FRAM) $(BR2_PACKAGE_FIND_MY_DEVICE_EEPROM) $(BR2_PACKAGE_FIND_MY_DEVICE_SPINOR_STATE)),)
FIND_MY_DEVICE_STATE_SOURCES += state_record.c
endif

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

ifeq ($(BR2_PACKAGE_FIND_MY_DEVICE_FRAM),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-find-my-device-fram.config
ifeq ($(BR2_PACKAGE_FIND_MY_DEVICE_EEPROM),y)
FIND_MY_DEVICE_COMPOSITION_DTSI = stm32f429disco-find-my-device-fram-eeprom.dtsi
else
FIND_MY_DEVICE_COMPOSITION_DTSI = stm32f429disco-find-my-device-fram.dtsi
endif

define FIND_MY_DEVICE_INSTALL_FRAM_MARKER
	$(INSTALL) -D -m 0644 /dev/null \
		$(TARGET_DIR)/etc/find-my-device-fram
endef
else ifneq ($(filter y,$(BR2_PACKAGE_FIND_MY_DEVICE_SPINOR_STATE) $(BR2_PACKAGE_SPINOR)),)
ifeq ($(BR2_PACKAGE_FIND_MY_DEVICE_EEPROM),y)
FIND_MY_DEVICE_COMPOSITION_DTSI = stm32f429disco-find-my-device-spinor-eeprom.dtsi
else
FIND_MY_DEVICE_COMPOSITION_DTSI = stm32f429disco-find-my-device-spinor.dtsi
endif
else ifeq ($(BR2_PACKAGE_FIND_MY_DEVICE_EEPROM),y)
FIND_MY_DEVICE_COMPOSITION_DTSI = stm32f429disco-find-my-device-eeprom.dtsi
else
FIND_MY_DEVICE_COMPOSITION_DTSI = stm32f429disco-find-my-device.dtsi
endif

ifeq ($(BR2_PACKAGE_USBSERIALDEVICE),y)
ifneq ($(filter y,$(BR2_PACKAGE_DISPLAY) $(BR2_PACKAGE_GALLERY) $(BR2_PACKAGE_FIRMWARE_SCREEN)),)
FIND_MY_DEVICE_BASE_DTS = stm32f429disco-usbserialdevice-display
else
FIND_MY_DEVICE_BASE_DTS = stm32f429disco-usbserialdevice
endif
else ifneq ($(filter y,$(BR2_PACKAGE_DISPLAY) $(BR2_PACKAGE_GALLERY) $(BR2_PACKAGE_FIRMWARE_SCREEN)),)
FIND_MY_DEVICE_BASE_DTS = stm32f429disco-display
else
FIND_MY_DEVICE_BASE_DTS = stm32f429disco-custom
endif

ifeq ($(BR2_PACKAGE_SPINAND),y)
ifneq ($(filter y,$(BR2_PACKAGE_SDCARD) $(BR2_PACKAGE_GALLERY_SDCARD)),)
FIND_MY_DEVICE_DTS = $(FIND_MY_DEVICE_BASE_DTS)-w5500-spinand-sdcard
else
FIND_MY_DEVICE_DTS = $(FIND_MY_DEVICE_BASE_DTS)-w5500-spinand
endif
else ifneq ($(filter y,$(BR2_PACKAGE_SDCARD) $(BR2_PACKAGE_GALLERY_SDCARD)),)
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
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-find-my-device-eeprom-device.dtsi \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-find-my-device-eeprom-device.dtsi
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-find-my-device-fram.dtsi \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-find-my-device-fram.dtsi
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-find-my-device-spinor.dtsi \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-find-my-device-spinor.dtsi
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/$(FIND_MY_DEVICE_COMPOSITION_DTSI) \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-find-my-device-config.dtsi
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
		STATE_SOURCES="$(FIND_MY_DEVICE_STATE_SOURCES)" \
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
	rm -f $(TARGET_DIR)/etc/find-my-device-fram \
		$(TARGET_DIR)/etc/find-my-device-eeprom \
		$(TARGET_DIR)/etc/find-my-device-spinor \
		$(TARGET_DIR)/etc/find-my-device-spinor-state
	$(FIND_MY_DEVICE_INSTALL_FRAM_MARKER)
	$(FIND_MY_DEVICE_INSTALL_EEPROM_MARKER)
	$(FIND_MY_DEVICE_INSTALL_SPINOR_STATE_MARKER)
	$(INSTALL) -D -m 0755 $(@D)/udhcpc.script \
		$(TARGET_DIR)/usr/share/find-my-device/udhcpc.script
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/var/lib/find-my-device
endef

$(eval $(generic-package))

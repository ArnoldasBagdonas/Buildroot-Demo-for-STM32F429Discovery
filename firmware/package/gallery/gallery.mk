################################################################################
#
# gallery
#
################################################################################

GALLERY_VERSION = 1.0
GALLERY_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/gallery
GALLERY_SITE_METHOD = local
GALLERY_DEPENDENCIES = fbv

GALLERY_DISPLAY_DIR = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/display
GALLERY_SDCARD_DIR = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/sdcard
GALLERY_CFLAGS = -DDISPLAY_MULTICALL
GALLERY_SOURCES = \
	$(@D)/gallery.c \
	$(GALLERY_DISPLAY_DIR)/display.c \
	$(GALLERY_DISPLAY_DIR)/display-pattern.c

ifeq ($(BR2_PACKAGE_GALLERY_SDCARD),y)
GALLERY_CFLAGS += -DWITH_SDCARD -DSDCARD_MULTICALL
GALLERY_SOURCES += $(GALLERY_SDCARD_DIR)/sdcard.c
endif

ifeq ($(BR2_PACKAGE_GALLERY_SDCARD_IMAGES),y)
GALLERY_CFLAGS += -DWITH_SDCARD_IMAGES
endif

define GALLERY_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		$(GALLERY_CFLAGS) \
		-I$(GALLERY_DISPLAY_DIR) -o $(@D)/gallery \
		$(GALLERY_SOURCES)
endef

ifeq ($(BR2_PACKAGE_GALLERY),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-display.config \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-compact.config

ifeq ($(BR2_PACKAGE_FIND_MY_DEVICE),y)
# Gallery is parsed after Find My Device and adds linux-compact.config. Re-apply
# the networking, UID, and timer requirements after that size fragment.
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-find-my-device.config
else
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-no-network.config
endif

ifeq ($(BR2_PACKAGE_USBSERIALDEVICE),y)
ifeq ($(BR2_PACKAGE_GALLERY_SDCARD),y)
GALLERY_DTS = stm32f429disco-usbserialdevice-display-sdcard
else
GALLERY_DTS = stm32f429disco-usbserialdevice-display
endif
else
ifeq ($(BR2_PACKAGE_GALLERY_SDCARD),y)
GALLERY_DTS = stm32f429disco-display-sdcard
else
GALLERY_DTS = stm32f429disco-display
endif
endif

LINUX_DTS_NAME += $(GALLERY_DTS)

ifeq ($(BR2_PACKAGE_GALLERY_SDCARD),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-initramfs-gzip.config \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-sdcard.config
endif

define GALLERY_COPY_DTS
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-display.dts \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-display.dts
endef
LINUX_PRE_BUILD_HOOKS += GALLERY_COPY_DTS

ifeq ($(BR2_PACKAGE_GALLERY_SDCARD),y)
define GALLERY_COPY_SDCARD_DTS
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-sdcard.dtsi \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-sdcard.dtsi
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/$(GALLERY_DTS).dts \
		$(LINUX_ARCH_PATH)/boot/dts/$(GALLERY_DTS).dts
endef
LINUX_PRE_BUILD_HOOKS += GALLERY_COPY_SDCARD_DTS
endif

ifeq ($(BR2_PACKAGE_USBSERIALDEVICE),y)
define GALLERY_COPY_USB_DISPLAY_DTS
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-usbserialdevice-display.dts \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-usbserialdevice-display.dts
endef
LINUX_PRE_BUILD_HOOKS += GALLERY_COPY_USB_DISPLAY_DTS
endif
endif

define GALLERY_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/gallery $(TARGET_DIR)/usr/bin/gallery
	ln -sf gallery $(TARGET_DIR)/usr/bin/display
	ln -sf gallery $(TARGET_DIR)/usr/bin/display-pattern
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/usr/share/display
	$(INSTALL) -m 0644 $(GALLERY_DISPLAY_DIR)/images/*.png \
		$(GALLERY_DISPLAY_DIR)/images/*.jpg \
		$(GALLERY_DISPLAY_DIR)/images/*.gif \
		$(TARGET_DIR)/usr/share/display/
endef

ifeq ($(BR2_PACKAGE_GALLERY_SDCARD),y)
define GALLERY_INSTALL_SDCARD
	ln -sf ../bin/gallery $(TARGET_DIR)/usr/sbin/sdcard
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/mnt/sdcard
endef
GALLERY_POST_INSTALL_TARGET_HOOKS += GALLERY_INSTALL_SDCARD
endif

ifeq ($(BR2_PACKAGE_GALLERY_AUTOSTART),y)
define GALLERY_INSTALL_AUTOSTART
	ln -sf gallery $(TARGET_DIR)/usr/bin/display-auto
	ln -sf gallery $(TARGET_DIR)/usr/bin/gallery-auto
endef
GALLERY_POST_INSTALL_TARGET_HOOKS += GALLERY_INSTALL_AUTOSTART
endif

$(eval $(generic-package))

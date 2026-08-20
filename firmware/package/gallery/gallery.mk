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
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-display.config

ifeq ($(BR2_PACKAGE_GALLERY_SDCARD),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-sdcard.config
endif

ifeq ($(BR2_PACKAGE_GALLERY_COMPRESS_INITRAMFS),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-initramfs-gzip.config
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
	$(RM) -f $(TARGET_DIR)/etc/init.d/S20sdcard \
		$(TARGET_DIR)/etc/init.d/S30gallery
endef

ifeq ($(BR2_PACKAGE_GALLERY_SDCARD),y)
define GALLERY_INSTALL_SDCARD
	ln -sf ../bin/gallery $(TARGET_DIR)/usr/sbin/sdcard
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/mnt/sdcard
	$(INSTALL) -D -m 0755 $(GALLERY_SDCARD_DIR)/S20sdcard \
		$(TARGET_DIR)/etc/init.d/S20sdcard
endef
GALLERY_POST_INSTALL_TARGET_HOOKS += GALLERY_INSTALL_SDCARD
endif

ifeq ($(BR2_PACKAGE_GALLERY_AUTOSTART),y)
define GALLERY_INSTALL_AUTOSTART
	ln -sf gallery $(TARGET_DIR)/usr/bin/display-auto
	ln -sf gallery $(TARGET_DIR)/usr/bin/gallery-auto
	$(INSTALL) -D -m 0755 $(@D)/S30gallery \
		$(TARGET_DIR)/etc/init.d/S30gallery
endef
GALLERY_POST_INSTALL_TARGET_HOOKS += GALLERY_INSTALL_AUTOSTART
endif

$(eval $(generic-package))

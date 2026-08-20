################################################################################
#
# find-me
#
################################################################################

FIND_ME_VERSION = 1.0
FIND_ME_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/find-me
FIND_ME_SITE_METHOD = local
FIND_ME_LICENSE = MIT
FIND_ME_LICENSE_FILES = LICENSE
# main/application_run plus the HTTP/WebSocket request path requires about
# 12 KiB before libc and signal frames. The bFLT default is only 4 KiB.
FIND_ME_FLAT_STACKSIZE = 32768
FIND_ME_CPPFLAGS = $(TARGET_CPPFLAGS)
FIND_ME_STATE_SOURCES =

ifeq ($(BR2_PACKAGE_FIND_ME_FRAM),y)
FIND_ME_CPPFLAGS += -DFMD_FRAM_STATE
endif

ifeq ($(BR2_PACKAGE_FIND_ME_EEPROM),y)
FIND_ME_CPPFLAGS += -DFMD_EEPROM_STATE
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-find-me-eeprom.config

define FIND_ME_INSTALL_EEPROM_MARKER
	$(INSTALL) -D -m 0644 /dev/null \
		$(TARGET_DIR)/etc/find-me-eeprom
endef
endif

ifneq ($(filter y,$(BR2_PACKAGE_FIND_ME_FRAM) $(BR2_PACKAGE_FIND_ME_EEPROM)),)
FIND_ME_STATE_SOURCES += nvmem_state.c
endif

ifeq ($(BR2_PACKAGE_FIND_ME_SPINOR_STATE),y)
FIND_ME_CPPFLAGS += -DFMD_SPINOR_STATE
FIND_ME_STATE_SOURCES += spinor_state.c
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-find-me-spinor.config

define FIND_ME_INSTALL_SPINOR_STATE_MARKER
	$(INSTALL) -D -m 0644 /dev/null \
		$(TARGET_DIR)/etc/find-me-spinor-state
endef
endif

ifneq ($(filter y,$(BR2_PACKAGE_FIND_ME_FRAM) $(BR2_PACKAGE_FIND_ME_EEPROM) $(BR2_PACKAGE_FIND_ME_SPINOR_STATE)),)
FIND_ME_STATE_SOURCES += state_record.c
endif

ifeq ($(BR2_PACKAGE_FIND_ME_CONSOLE_DEBUG),y)
FIND_ME_CPPFLAGS += -DFMD_CONSOLE_DEBUG

define FIND_ME_INSTALL_CONSOLE_DEBUG
	$(INSTALL) -D -m 0755 $(@D)/find-me-debug \
		$(TARGET_DIR)/usr/bin/find-me-debug
	$(INSTALL) -D -m 0755 $(@D)/find-me-confirm \
		$(TARGET_DIR)/usr/bin/find-me-confirm
endef
endif

ifeq ($(BR2_PACKAGE_FIND_ME),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-find-me.config
BUSYBOX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/busybox-find-me.config

ifeq ($(BR2_PACKAGE_FIND_ME_COMPRESS_INITRAMFS),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-initramfs-gzip.config
endif

ifeq ($(BR2_PACKAGE_FIND_ME_FRAM),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-find-me-fram.config

define FIND_ME_INSTALL_FRAM_MARKER
	$(INSTALL) -D -m 0644 /dev/null \
		$(TARGET_DIR)/etc/find-me-fram
endef
endif
endif

define FIND_ME_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/src clean
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/src \
		CC="$(TARGET_CC)" \
		STATE_SOURCES="$(FIND_ME_STATE_SOURCES)" \
		CPPFLAGS="$(FIND_ME_CPPFLAGS)" \
		CFLAGS='$(TARGET_CFLAGS) -ffunction-sections -fdata-sections' \
		LDFLAGS='$(TARGET_LDFLAGS) -Wl,--gc-sections'
endef

define FIND_ME_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/src/find-me \
		$(TARGET_DIR)/usr/bin/find-me
	$(INSTALL) -D -m 0755 $(@D)/find-me-start \
		$(TARGET_DIR)/usr/bin/find-me-start
	$(FIND_ME_INSTALL_CONSOLE_DEBUG)
	$(INSTALL) -D -m 0644 $(@D)/find-me.conf \
		$(TARGET_DIR)/etc/find-me.conf
	rm -f $(TARGET_DIR)/etc/find-me-fram \
		$(TARGET_DIR)/etc/find-me-eeprom \
		$(TARGET_DIR)/etc/find-me-spinor \
		$(TARGET_DIR)/etc/find-me-spinor-state
	$(FIND_ME_INSTALL_FRAM_MARKER)
	$(FIND_ME_INSTALL_EEPROM_MARKER)
	$(FIND_ME_INSTALL_SPINOR_STATE_MARKER)
	$(INSTALL) -D -m 0755 $(@D)/udhcpc.script \
		$(TARGET_DIR)/usr/share/find-me/udhcpc.script
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/var/lib/find-me
	$(RM) -f $(TARGET_DIR)/etc/init.d/S40find-me \
		$(TARGET_DIR)/etc/init.d/find-me \
		$(TARGET_DIR)/etc/init.d/find-me-service \
		$(TARGET_DIR)/usr/bin/find-me-service
endef

ifeq ($(BR2_PACKAGE_FIND_ME_AUTOSTART),y)
define FIND_ME_INSTALL_AUTOSTART
	$(INSTALL) -D -m 0755 $(@D)/S40find-me \
		$(TARGET_DIR)/etc/init.d/S40find-me
	ln -sf ../../etc/init.d/S40find-me \
		$(TARGET_DIR)/usr/bin/find-me-service
endef
FIND_ME_POST_INSTALL_TARGET_HOOKS += FIND_ME_INSTALL_AUTOSTART
endif

$(eval $(generic-package))

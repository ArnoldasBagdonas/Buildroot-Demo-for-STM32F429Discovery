# firmware/external.mk

# Normal example images start from the size-oriented kernel profile. Package
# makefiles are included afterwards and re-enable only their required drivers
# and syscall families. Sysdiag alone retains the broader base configuration;
# when selected with an example, its fragment restores the diagnostic features.
ifneq ($(filter y,$(BR2_PACKAGE_DISPLAY) \
	$(BR2_PACKAGE_FIND_ME) \
	$(BR2_PACKAGE_FIRMWARE_SCREEN) \
	$(BR2_PACKAGE_GALLERY) \
	$(BR2_PACKAGE_HELLO_C) \
	$(BR2_PACKAGE_HELLO_CPP) \
	$(BR2_PACKAGE_HWTOOLS) \
	$(BR2_PACKAGE_NETWORKING) \
	$(BR2_PACKAGE_PERIPHERY) \
	$(BR2_PACKAGE_SDCARD) \
	$(BR2_PACKAGE_SPINAND) \
	$(BR2_PACKAGE_SPINOR) \
	$(BR2_PACKAGE_USB_CDC)),)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-compact.config \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-no-network.config
endif

# Include all custom packages after the common profile so their requirements
# take precedence.
include $(sort $(wildcard $(BR2_EXTERNAL_FIRMWARE_PATH)/package/*/*.mk))

# Every package uses one board description. Drivers remain package-controlled,
# so an unselected peripheral node does not add kernel code or probe hardware.
define FIRMWARE_COPY_UNIFIED_DTS
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-custom.dts \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-custom.dts
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-screen.dts \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-screen.dts
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-usb-cdc.dtsi \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-usb-cdc.dtsi
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-networking-w5500.dtsi \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-networking-w5500.dtsi
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-find-me.dtsi \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-find-me.dtsi
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-find-me-eeprom.dtsi \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-find-me-eeprom.dtsi
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-spinand.dtsi \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-spinand.dtsi
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-sdcard.dtsi \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-sdcard.dtsi
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-spinor.dtsi \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-spinor.dtsi
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-fram.dtsi \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-fram.dtsi
endef
LINUX_PRE_BUILD_HOOKS += FIRMWARE_COPY_UNIFIED_DTS

# Keep service-management code out of images that do not install a background
# service. Synchronous storage mount services do not need start-stop-daemon.
ifneq ($(filter y,$(BR2_PACKAGE_DISPLAY_AUTOSTART) \
	$(BR2_PACKAGE_GALLERY_AUTOSTART) \
	$(BR2_PACKAGE_GALLERY_SDCARD) \
	$(BR2_PACKAGE_FIRMWARE_SCREEN_AUTOSTART) \
	$(BR2_PACKAGE_SDCARD_AUTODETECT) \
	$(BR2_PACKAGE_FIND_ME_AUTOSTART)),)
BUSYBOX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/busybox-services.config
endif

# Storage packages append size-oriented fragments after Screen is parsed.
# Reapply Screen's package-owned requirements last so I2C3, evdev, STMPE811,
# and the PTY used by the child shell survive every supported composition.
ifeq ($(BR2_PACKAGE_FIRMWARE_SCREEN),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-screen.config
endif

ifneq ($(filter y,$(BR2_PACKAGE_DISPLAY) $(BR2_PACKAGE_GALLERY)),)
# giflib's upstream Makefile otherwise replaces Buildroot's no-MMU/static
# target flags with "-O2 -fPIC". Pass CFLAGS on make's command line so the
# library uses the same ABI and FLAT-binary settings as fbv and the rest of
# this target, without modifying either Buildroot or giflib sources.
define GIFLIB_BUILD_CMDS
	$(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D) \
		CFLAGS="$(TARGET_CFLAGS) -std=gnu99 -Wall -Wno-format-truncation" \
		$(GIFLIB_BUILD_LIBS)
endef
endif

# Add custom kernel patch directory
LINUX_PATCHES += $(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-patches/linux-$(BR2_LINUX_KERNEL_VERSION)

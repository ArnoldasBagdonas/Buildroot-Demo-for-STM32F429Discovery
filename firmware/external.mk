# firmware/external.mk

# Include all custom packages
include $(sort $(wildcard $(BR2_EXTERNAL_FIRMWARE_PATH)/package/*/*.mk))

ifeq ($(BR2_PACKAGE_DISPLAYEXAMPLE),y)
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

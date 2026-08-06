###############################################################################
#
# SLEEPEXAMPLE package
#
###############################################################################

# Package version and source location
SLEEPEXAMPLE_VERSION = 1.0
SLEEPEXAMPLE_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/sleepexample/project
SLEEPEXAMPLE_SITE_METHOD = local

# Linux is parsed before br2-external package makefiles. The selected example
# must restore the 32-bit time syscall ABI used by this uClibc toolchain.
ifeq ($(BR2_PACKAGE_SLEEPEXAMPLE),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-sleep.config
endif

# Build commands
define SLEEPEXAMPLE_BUILD_CMDS
	$(MAKE) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS) -Wl,--gc-sections" \
		-C $(@D)
endef

# Install the compiled binary to the target filesystem
define SLEEPEXAMPLE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/sleepexample $(TARGET_DIR)/usr/bin/sleepexample
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

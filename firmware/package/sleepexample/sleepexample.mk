###############################################################################
#
# SLEEPEXAMPLE package
#
###############################################################################

# Package version and source location
SLEEPEXAMPLE_VERSION = 1.0
SLEEPEXAMPLE_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/sleepexample/project
SLEEPEXAMPLE_SITE_METHOD = local

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

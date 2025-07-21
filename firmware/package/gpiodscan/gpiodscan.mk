###############################################################################
#
# GPIODSCAN package
#
###############################################################################

# Package version and source location
GPIODSCAN_VERSION = 1.0
GPIODSCAN_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/gpiodscan/project
GPIODSCAN_SITE_METHOD = local

# Build commands (use the correct target compiler and environment)
define GPIODSCAN_BUILD_CMDS
	$(MAKE) CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" -C $(@D)
endef

# Install the compiled binary to the target filesystem
define GPIODSCAN_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/gpiodscan $(TARGET_DIR)/usr/bin/gpiodscan
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

###############################################################################
#
# LEDBRIGHTNESS package
#
###############################################################################

# Package version and source location
LEDBRIGHTNESS_VERSION = 1.0
LEDBRIGHTNESS_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/ledbrightness/project
LEDBRIGHTNESS_SITE_METHOD = local

# Build commands (use the correct target compiler and environment)
define LEDBRIGHTNESS_BUILD_CMDS
	$(MAKE) CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" -C $(@D)
endef

# Install the compiled binary to the target filesystem
define LEDBRIGHTNESS_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/ledbrightness $(TARGET_DIR)/usr/bin/ledbrightness
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

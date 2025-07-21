###############################################################################
#
# HELLOMK package
#
###############################################################################

# Package version and source location
HELLOMK_VERSION = 1.0
HELLOMK_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/hellomk/project
HELLOMK_SITE_METHOD = local

# Build commands (use the correct target compiler and environment)
define HELLOMK_BUILD_CMDS
	$(MAKE) CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" -C $(@D)
endef

# Install the compiled binary and config file to the target filesystem
define HELLOMK_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/hellomk $(TARGET_DIR)/usr/bin/hellomk
	$(INSTALL) -D -m 0644 $(@D)/bin/hellomk.ini $(TARGET_DIR)/etc/hellomk.ini
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

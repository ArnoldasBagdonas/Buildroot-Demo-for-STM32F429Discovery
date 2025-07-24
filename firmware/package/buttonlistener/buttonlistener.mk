###############################################################################
#
# BUTTONLISTENER package
#
###############################################################################

# Package version and source location
BUTTONLISTENER_VERSION = 1.0
BUTTONLISTENER_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/buttonlistener/project
BUTTONLISTENER_SITE_METHOD = local

# Build commands (use the correct target compiler and environment)
define BUTTONLISTENER_BUILD_CMDS
	$(MAKE) CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" -C $(@D)
endef

# Install the compiled binary to the target filesystem
define BUTTONLISTENER_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/buttonlistener $(TARGET_DIR)/usr/bin/buttonlistener
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

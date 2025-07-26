###############################################################################
#
# IOEXAMPLE1 package
#
###############################################################################

# Package version and source location
IOEXAMPLE1_VERSION = 1.0
IOEXAMPLE1_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/ioexample1/project
IOEXAMPLE1_SITE_METHOD = local

# Build commands (use the correct target compiler and environment)
define IOEXAMPLE1_BUILD_CMDS
	$(MAKE) CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" -C $(@D)
endef

# Install the compiled binary to the target filesystem
define IOEXAMPLE1_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/ioexample1 $(TARGET_DIR)/usr/bin/ioexample1
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

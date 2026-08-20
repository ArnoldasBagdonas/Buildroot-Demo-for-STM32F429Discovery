################################################################################
#
# HELLO_C package
#
################################################################################

# Package version and source location
HELLO_C_VERSION = 1.0
HELLO_C_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/hello-c/project
HELLO_C_SITE_METHOD = local

# Build commands (use the correct target compiler and environment)
define HELLO_C_BUILD_CMDS
	$(MAKE) CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" -C $(@D)
endef

# Install the compiled binary and config file to the target filesystem
define HELLO_C_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/hello-c $(TARGET_DIR)/usr/bin/hello-c
	$(INSTALL) -D -m 0644 $(@D)/bin/hello-c.ini $(TARGET_DIR)/etc/hello-c.ini
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

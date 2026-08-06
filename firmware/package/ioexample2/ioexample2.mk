###############################################################################
#
# IOEXAMPLE2 package
#
###############################################################################

# Package version and source location
IOEXAMPLE2_VERSION = 1.0
IOEXAMPLE2_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/ioexample2/project
IOEXAMPLE2_SITE_METHOD = local



# Build commands
define IOEXAMPLE2_BUILD_CMDS
	$(MAKE) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS) -Wl,--gc-sections" \
		-C $(@D)
endef

# Install the compiled binary to the target filesystem
define IOEXAMPLE2_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/ioexample2 $(TARGET_DIR)/usr/bin/ioexample2
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

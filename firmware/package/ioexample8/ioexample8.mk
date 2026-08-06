###############################################################################
#
# IOEXAMPLE8 package
#
###############################################################################

# Package version and source location
IOEXAMPLE8_VERSION = 1.0
IOEXAMPLE8_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/ioexample8/project
IOEXAMPLE8_SITE_METHOD = local



# Build commands
define IOEXAMPLE8_BUILD_CMDS
	$(MAKE) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS) -Wl,--gc-sections" \
		-C $(@D)
endef

# Install the compiled binary to the target filesystem
define IOEXAMPLE8_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/ioexample8 $(TARGET_DIR)/usr/bin/ioexample8
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

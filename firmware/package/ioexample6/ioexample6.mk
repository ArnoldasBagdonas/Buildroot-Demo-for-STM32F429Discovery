###############################################################################
#
# IOEXAMPLE6 package
#
###############################################################################

# Package version and source location
IOEXAMPLE6_VERSION = 1.0
IOEXAMPLE6_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/ioexample6/project
IOEXAMPLE6_SITE_METHOD = local
IOEXAMPLE6_DEPENDENCIES = periphery



# Build commands
define IOEXAMPLE6_BUILD_CMDS
	$(MAKE) -C $(@D) clean
	$(MAKE) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS) -Wl,--gc-sections" \
		LDLIBS="-lperiphery" \
		-C $(@D)
endef

# Install the compiled binary to the target filesystem
define IOEXAMPLE6_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/ioexample6 $(TARGET_DIR)/usr/bin/ioexample6
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

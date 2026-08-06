###############################################################################
#
# IOEXAMPLE5 package
#
###############################################################################

# Package version and source location
IOEXAMPLE5_VERSION = 1.0
IOEXAMPLE5_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/ioexample5/project
IOEXAMPLE5_SITE_METHOD = local
IOEXAMPLE5_DEPENDENCIES = periphery



# Build commands
define IOEXAMPLE5_BUILD_CMDS
	$(MAKE) -C $(@D) clean
	$(MAKE) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS) -Wl,--gc-sections" \
		LDLIBS="-lperiphery" \
		-C $(@D)
endef

# Install the compiled binary to the target filesystem
define IOEXAMPLE5_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/ioexample5 $(TARGET_DIR)/usr/bin/ioexample5
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

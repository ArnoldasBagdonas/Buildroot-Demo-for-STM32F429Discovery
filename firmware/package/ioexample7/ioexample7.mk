###############################################################################
#
# IOEXAMPLE7 package
#
###############################################################################

# Package version and source location
IOEXAMPLE7_VERSION = 1.0
IOEXAMPLE7_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/ioexample7/project
IOEXAMPLE7_SITE_METHOD = local
IOEXAMPLE7_DEPENDENCIES = periphery



# Build commands
define IOEXAMPLE7_BUILD_CMDS
	$(MAKE) -C $(@D) clean
	$(MAKE) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS) -Wl,--gc-sections" \
		LDLIBS="-lperiphery" \
		-C $(@D)
endef

# Install the compiled binary to the target filesystem
define IOEXAMPLE7_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/ioexample7 $(TARGET_DIR)/usr/bin/ioexample7
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

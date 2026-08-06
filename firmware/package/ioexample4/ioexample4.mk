###############################################################################
#
# IOEXAMPLE4 package
#
###############################################################################

# Package version and source location
IOEXAMPLE4_VERSION = 1.0
IOEXAMPLE4_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/ioexample4/project
IOEXAMPLE4_SITE_METHOD = local
IOEXAMPLE4_DEPENDENCIES = periphery



# Build commands
define IOEXAMPLE4_BUILD_CMDS
	$(MAKE) -C $(@D) clean
	$(MAKE) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS) -Wl,--gc-sections" \
		LDLIBS="-lperiphery" \
		-C $(@D)
endef

# Install the compiled binary to the target filesystem
define IOEXAMPLE4_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/ioexample4 $(TARGET_DIR)/usr/bin/ioexample4
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

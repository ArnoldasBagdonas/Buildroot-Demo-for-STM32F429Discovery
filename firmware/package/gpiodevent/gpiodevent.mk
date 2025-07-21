###############################################################################
#
# GPIODEVENT package
#
###############################################################################

# Package version and source location
GPIODEVENT_VERSION = 1.0
GPIODEVENT_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/gpiodevent/project
GPIODEVENT_SITE_METHOD = local

# Build commands (use the correct target compiler and environment)
define GPIODEVENT_BUILD_CMDS
	$(MAKE) CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" -C $(@D)
endef

# Install the compiled binary to the target filesystem
define GPIODEVENT_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/gpiodevent $(TARGET_DIR)/usr/bin/gpiodevent
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

###############################################################################
#
# GPIODIO package
#
###############################################################################

# Package version and source location
GPIODIO_VERSION = 1.0
GPIODIO_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/gpiodio/project
GPIODIO_SITE_METHOD = local

# Build commands (use the correct target compiler and environment)
define GPIODIO_BUILD_CMDS
	$(MAKE) CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" -C $(@D)
endef

# Install the compiled binary to the target filesystem
define GPIODIO_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/gpiodio $(TARGET_DIR)/usr/bin/gpiodio
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

###############################################################################
#
# IOEXAMPLE3 package
#
###############################################################################

# Package version and source location
IOEXAMPLE3_VERSION = 1.0
IOEXAMPLE3_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/ioexample3/project
IOEXAMPLE3_SITE_METHOD = local

# Use cross-compiler for detection
IOEXAMPLE3_NULL := $(if $(filter Windows_NT,$(OS)),NUL,/dev/null)
IOEXAMPLE3_GPIO_CDEV_V1_SUPPORT := $(shell ! env printf "\x23include <linux/gpio.h>\n\x23ifndef GPIO_GET_LINEEVENT_IOCTL\n\x23error\n\x23endif" | $(TARGET_CC) -E - >$(IOEXAMPLE3_NULL) 2>&1; echo $$?)
IOEXAMPLE3_GPIO_CDEV_V2_SUPPORT := $(shell ! env printf "\x23include <linux/gpio.h>\nint main(void) { GPIO_V2_LINE_FLAG_EVENT_CLOCK_REALTIME; return 0; }" | $(TARGET_CC) -x c - >$(IOEXAMPLE3_NULL) 2>&1; echo $$?)
IOEXAMPLE3_GPIO_CDEV_SUPPORT = $(if $(filter 1,$(IOEXAMPLE3_GPIO_CDEV_V2_SUPPORT)),2,$(if $(filter 1,$(IOEXAMPLE3_GPIO_CDEV_V1_SUPPORT)),1,0))

IOEXAMPLE3_DEFINES += -DPERIPHERY_GPIO_CDEV_SUPPORT=$(IOEXAMPLE3_GPIO_CDEV_SUPPORT)


# Build commands
define IOEXAMPLE3_BUILD_CMDS
	$(MAKE) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS) $(IOEXAMPLE3_DEFINES)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		-C $(@D)
endef

# Install the compiled binary to the target filesystem
define IOEXAMPLE3_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/ioexample3 $(TARGET_DIR)/usr/bin/ioexample3
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

###############################################################################
#
# SLEEPEXAMPLE package
#
###############################################################################

# Package version and source location
SLEEPEXAMPLE_VERSION = 1.0
SLEEPEXAMPLE_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/sleepexample/project
SLEEPEXAMPLE_SITE_METHOD = local

# Use cross-compiler for detection
SLEEPEXAMPLE_NULL := $(if $(filter Windows_NT,$(OS)),NUL,/dev/null)
SLEEPEXAMPLE_GPIO_CDEV_V1_SUPPORT := $(shell ! env printf "\x23include <linux/gpio.h>\n\x23ifndef GPIO_GET_LINEEVENT_IOCTL\n\x23error\n\x23endif" | $(TARGET_CC) -E - >$(SLEEPEXAMPLE_NULL) 2>&1; echo $$?)
SLEEPEXAMPLE_GPIO_CDEV_V2_SUPPORT := $(shell ! env printf "\x23include <linux/gpio.h>\nint main(void) { GPIO_V2_LINE_FLAG_EVENT_CLOCK_REALTIME; return 0; }" | $(TARGET_CC) -x c - >$(SLEEPEXAMPLE_NULL) 2>&1; echo $$?)
SLEEPEXAMPLE_GPIO_CDEV_SUPPORT = $(if $(filter 1,$(SLEEPEXAMPLE_GPIO_CDEV_V2_SUPPORT)),2,$(if $(filter 1,$(SLEEPEXAMPLE_GPIO_CDEV_V1_SUPPORT)),1,0))

SLEEPEXAMPLE_DEFINES += -DPERIPHERY_GPIO_CDEV_SUPPORT=$(SLEEPEXAMPLE_GPIO_CDEV_SUPPORT)


# Build commands
define SLEEPEXAMPLE_BUILD_CMDS
	$(MAKE) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS) $(SLEEPEXAMPLE_DEFINES)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		-C $(@D)
endef

# Install the compiled binary to the target filesystem
define SLEEPEXAMPLE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/sleepexample $(TARGET_DIR)/usr/bin/sleepexample
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

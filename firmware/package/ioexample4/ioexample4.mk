###############################################################################
#
# IOEXAMPLE4 package
#
###############################################################################

# Package version and source location
IOEXAMPLE4_VERSION = 1.0
IOEXAMPLE4_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/ioexample4/project
IOEXAMPLE4_SITE_METHOD = local

# Use cross-compiler for detection
IOEXAMPLE4_NULL := $(if $(filter Windows_NT,$(OS)),NUL,/dev/null)
IOEXAMPLE4_GPIO_CDEV_V1_SUPPORT := $(shell ! env printf "\x23include <linux/gpio.h>\n\x23ifndef GPIO_GET_LINEEVENT_IOCTL\n\x23error\n\x23endif" | $(TARGET_CC) -E - >$(IOEXAMPLE4_NULL) 2>&1; echo $$?)
IOEXAMPLE4_GPIO_CDEV_V2_SUPPORT := $(shell ! env printf "\x23include <linux/gpio.h>\nint main(void) { GPIO_V2_LINE_FLAG_EVENT_CLOCK_REALTIME; return 0; }" | $(TARGET_CC) -x c - >$(IOEXAMPLE4_NULL) 2>&1; echo $$?)
IOEXAMPLE4_GPIO_CDEV_SUPPORT = $(if $(filter 1,$(IOEXAMPLE4_GPIO_CDEV_V2_SUPPORT)),2,$(if $(filter 1,$(IOEXAMPLE4_GPIO_CDEV_V1_SUPPORT)),1,0))

IOEXAMPLE4_DEFINES += -DPERIPHERY_GPIO_CDEV_SUPPORT=$(IOEXAMPLE4_GPIO_CDEV_SUPPORT)


# Build commands
define IOEXAMPLE4_BUILD_CMDS
	$(MAKE) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS) $(IOEXAMPLE4_DEFINES)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		-C $(@D)
endef

# Install the compiled binary to the target filesystem
define IOEXAMPLE4_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/ioexample4 $(TARGET_DIR)/usr/bin/ioexample4
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

###############################################################################
#
# IOEXAMPLE8 package
#
###############################################################################

# Package version and source location
IOEXAMPLE8_VERSION = 1.0
IOEXAMPLE8_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/ioexample8/project
IOEXAMPLE8_SITE_METHOD = local

# Use cross-compiler for detection
IOEXAMPLE8_NULL := $(if $(filter Windows_NT,$(OS)),NUL,/dev/null)
IOEXAMPLE8_GPIO_CDEV_V1_SUPPORT := $(shell ! env printf "\x23include <linux/gpio.h>\n\x23ifndef GPIO_GET_LINEEVENT_IOCTL\n\x23error\n\x23endif" | $(TARGET_CC) -E - >$(IOEXAMPLE8_NULL) 2>&1; echo $$?)
IOEXAMPLE8_GPIO_CDEV_V2_SUPPORT := $(shell ! env printf "\x23include <linux/gpio.h>\nint main(void) { GPIO_V2_LINE_FLAG_EVENT_CLOCK_REALTIME; return 0; }" | $(TARGET_CC) -x c - >$(IOEXAMPLE8_NULL) 2>&1; echo $$?)
IOEXAMPLE8_GPIO_CDEV_SUPPORT = $(if $(filter 1,$(IOEXAMPLE8_GPIO_CDEV_V2_SUPPORT)),2,$(if $(filter 1,$(IOEXAMPLE8_GPIO_CDEV_V1_SUPPORT)),1,0))

IOEXAMPLE8_DEFINES += -DPERIPHERY_GPIO_CDEV_SUPPORT=$(IOEXAMPLE8_GPIO_CDEV_SUPPORT)


# Build commands
define IOEXAMPLE8_BUILD_CMDS
	$(MAKE) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS) $(IOEXAMPLE8_DEFINES)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		-C $(@D)
endef

# Install the compiled binary to the target filesystem
define IOEXAMPLE8_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/ioexample8 $(TARGET_DIR)/usr/bin/ioexample8
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

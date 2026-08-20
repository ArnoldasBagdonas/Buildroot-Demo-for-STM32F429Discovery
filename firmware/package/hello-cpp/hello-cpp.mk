################################################################################
#
# HELLO_CPP package
#
################################################################################

# Package version and source location
HELLO_CPP_VERSION = 1.0
HELLO_CPP_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/hello-cpp/project
HELLO_CPP_SITE_METHOD = local

# Build dependencies (ensure g++ is available for the build)
# Run make menuconfig in your buildroot system. Under the toolchain heading select the g++ option.
# Then flag is set in configiguration file BR2_TOOLCHAIN_BUILDROOT_CXX=y

# Build commands (use the correct target C++ compiler and environment)
define HELLO_CPP_BUILD_CMDS
	$(MAKE) CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" CXX="$(TARGET_CXX)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" -C $(@D)
endef

# Install the compiled binary and config file to the target filesystem
define HELLO_CPP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/hello-cpp $(TARGET_DIR)/usr/bin/hello-cpp
	$(INSTALL) -D -m 0644 $(@D)/bin/hello-cpp.ini $(TARGET_DIR)/etc/hello-cpp.ini
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

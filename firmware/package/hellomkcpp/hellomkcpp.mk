###############################################################################
#
# HELLOMKCPP package
#
###############################################################################

# Package version and source location
HELLOMKCPP_VERSION = 1.0
HELLOMKCPP_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/hellomkcpp/project
HELLOMKCPP_SITE_METHOD = local

# Build dependencies (ensure g++ is available for the build)
# Run make menuconfig in your buildroot system. Under the toolchain heading select the g++ option.
# Then flag is set in configiguration file BR2_TOOLCHAIN_BUILDROOT_CXX=y

# Build commands (use the correct target C++ compiler and environment)
define HELLOMKCPP_BUILD_CMDS
	$(MAKE) CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" CXX="$(TARGET_CXX)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" -C $(@D)
endef

# Install the compiled binary and config file to the target filesystem
define HELLOMKCPP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/hellomkcpp $(TARGET_DIR)/usr/bin/hellomkcpp
	$(INSTALL) -D -m 0644 $(@D)/bin/hellomkcpp.ini $(TARGET_DIR)/etc/hellomkcpp.ini
endef

# Evaluate the generic package infrastructure
$(eval $(generic-package))

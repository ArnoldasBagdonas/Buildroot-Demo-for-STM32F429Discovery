################################################################################
#
# periphery
#
################################################################################

PERIPHERY_VERSION = 2.4.3-stm32
PERIPHERY_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/periphery/project
PERIPHERY_SITE_METHOD = local
PERIPHERY_INSTALL_STAGING = YES
PERIPHERY_LICENSE = MIT

define PERIPHERY_BUILD_CMDS
	$(MAKE) -C $(@D) clean
	$(TARGET_MAKE_ENV) $(MAKE) \
		CC="$(TARGET_CC)" \
		AR="$(TARGET_AR)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		CPPFLAGS="$(TARGET_CPPFLAGS)" \
		-C $(@D)
endef

define PERIPHERY_INSTALL_STAGING_CMDS
	$(INSTALL) -D -m 0644 $(@D)/bin/libperiphery.a \
		$(STAGING_DIR)/usr/lib/libperiphery.a
	$(INSTALL) -d -m 0755 $(STAGING_DIR)/usr/include/periphery
	$(INSTALL) -m 0644 $(@D)/include/periphery/*.h \
		$(STAGING_DIR)/usr/include/periphery/
endef

$(eval $(generic-package))

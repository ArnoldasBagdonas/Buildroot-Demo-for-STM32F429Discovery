################################################################################
#
# pn7150
#
################################################################################

PN7150_VERSION = 1.6-linux1
PN7150_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/pn7150
PN7150_SITE_METHOD = local
PN7150_LICENSE = MIT (Linux port), NXP NFC Infrastructure Software License (NXP library)
PN7150_LICENSE_FILES = LICENSE
PN7150_REDISTRIBUTE = NO
PN7150_FLAT_STACKSIZE = 32768
PN7150_NXP_ARCHIVE = $(BR2_EXTERNAL_FIRMWARE_PATH)/../SW4325.zip
PN7150_NXP_ROOT = NXP-NCI_LPC11U6x_example

ifeq ($(BR2_PACKAGE_PN7150_IRQ_POLLING_FALLBACK),y)
PN7150_CFLAGS += -DPN7150_IRQ_POLLING_FALLBACK
endif

ifeq ($(BR2_PACKAGE_PN7150),y)
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-pn7150.config
endif

define PN7150_EXTRACT_NXP_LIBRARY
	test -f $(PN7150_NXP_ARCHIVE) || { \
		echo "ERROR: Download NXP SW4325.zip and place it at $(PN7150_NXP_ARCHIVE)"; \
		exit 1; \
	}
	$(RM) -r $(@D)/nxp $(@D)/nxp-extract $(@D)/NXP-NCI_MCUXpressoExample_V1.6.zip
	unzip -p $(PN7150_NXP_ARCHIVE) \
		NXP-NCI_MCUXpressoExample_V1.6.zip \
		> $(@D)/NXP-NCI_MCUXpressoExample_V1.6.zip
	unzip -q $(@D)/NXP-NCI_MCUXpressoExample_V1.6.zip \
		'$(PN7150_NXP_ROOT)/NfcLibrary/*' -d $(@D)/nxp-extract
	mv $(@D)/nxp-extract/$(PN7150_NXP_ROOT)/NfcLibrary $(@D)/nxp
endef
PN7150_PRE_BUILD_HOOKS += PN7150_EXTRACT_NXP_LIBRARY

PN7150_VENDOR_SOURCES = \
	$(@D)/nxp/NxpNci/src/NxpNci.c \
	$(@D)/nxp/NdefLibrary/src/RW_NDEF.c \
	$(@D)/nxp/NdefLibrary/src/RW_NDEF_T1T.c \
	$(@D)/nxp/NdefLibrary/src/RW_NDEF_T2T.c \
	$(@D)/nxp/NdefLibrary/src/RW_NDEF_T3T.c \
	$(@D)/nxp/NdefLibrary/src/RW_NDEF_T4T.c \
	$(@D)/nxp/NdefLibrary/src/RW_NDEF_T5T.c \
	$(@D)/nxp/NdefLibrary/src/RW_NDEF_MIFARE.c

define PN7150_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-std=gnu99 -Wall -Wextra -DRW_SUPPORT -DNCI_DEBUG \
		$(PN7150_CFLAGS) \
		-I$(@D) -I$(@D)/nxp/inc -I$(@D)/nxp/NxpNci/inc \
		-I$(@D)/nxp/NdefLibrary/inc \
		-o $(@D)/pn7150-demo \
		$(@D)/pn7150-demo.c $(@D)/tml-linux.c $(@D)/tool-linux.c \
		$(PN7150_VENDOR_SOURCES)
endef

define PN7150_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/pn7150-demo \
		$(TARGET_DIR)/usr/bin/pn7150-demo
endef

$(eval $(generic-package))

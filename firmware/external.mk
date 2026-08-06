# firmware/external.mk

# Include all custom packages
include $(sort $(wildcard $(BR2_EXTERNAL_FIRMWARE_PATH)/package/*/*.mk))

# ioexample7 and ioexample8 share one USART3-enabled board variant. Keeping
# this logic here avoids adding the same DTB and hook twice if both UART
# examples are selected.
ifneq ($(filter y,$(BR2_PACKAGE_IOEXAMPLE7) $(BR2_PACKAGE_IOEXAMPLE8)),)
LINUX_DTS_NAME += stm32f429disco-usart3

define STM32F429DISCO_COPY_USART3_DTS
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-usart3.dts \
		$(LINUX_ARCH_PATH)/boot/dts/stm32f429disco-usart3.dts
endef
LINUX_PRE_BUILD_HOOKS += STM32F429DISCO_COPY_USART3_DTS
endif

# PB10/PB11 are USART3 TX/RX and also LTDC G4/G5. Reject a direct Buildroot
# build instead of producing firmware in which one of the drivers cannot bind.
ifeq ($(BR2_PACKAGE_DISPLAYEXAMPLE),y)
# giflib's upstream Makefile otherwise replaces Buildroot's no-MMU/static
# target flags with "-O2 -fPIC". Pass CFLAGS on make's command line so the
# library uses the same ABI and FLAT-binary settings as fbv and the rest of
# this target, without modifying either Buildroot or giflib sources.
define GIFLIB_BUILD_CMDS
	$(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D) \
		CFLAGS="$(TARGET_CFLAGS) -std=gnu99 -Wall -Wno-format-truncation" \
		$(GIFLIB_BUILD_LIBS)
endef

ifneq ($(filter y,$(BR2_PACKAGE_IOEXAMPLE7) $(BR2_PACKAGE_IOEXAMPLE8)),)
define STM32F429DISCO_REJECT_DISPLAY_USART3_CONFLICT
	echo "ERROR: displayexample conflicts with ioexample7/ioexample8: PB10/PB11 are shared by LTDC and USART3." >&2
	false
endef
LINUX_PRE_BUILD_HOOKS += STM32F429DISCO_REJECT_DISPLAY_USART3_CONFLICT
endif
endif

# Add custom kernel patch directory
LINUX_PATCHES += $(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-patches/linux-$(BR2_LINUX_KERNEL_VERSION)

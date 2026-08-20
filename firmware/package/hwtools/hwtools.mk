################################################################################
#
# hwtools
#
################################################################################

HWTOOLS_VERSION = 1.0
HWTOOLS_SITE = $(BR2_EXTERNAL_FIRMWARE_PATH)/package/hwtools
HWTOOLS_SITE_METHOD = local
HWTOOLS_SOURCES = $(@D)/hw.c
HWTOOLS_APPLETS =

ifeq ($(BR2_PACKAGE_HWTOOLS_GPIO_LED),y)
HWTOOLS_SOURCES += $(@D)/gpio_led.c
HWTOOLS_CFLAGS += -DHWTOOLS_GPIO_LED
HWTOOLS_APPLETS += gpio-led
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-hwtools-gpio.config
endif
ifeq ($(BR2_PACKAGE_HWTOOLS_BUTTON_LED),y)
HWTOOLS_SOURCES += $(@D)/button_led.c
HWTOOLS_CFLAGS += -DHWTOOLS_BUTTON_LED
HWTOOLS_APPLETS += button-led
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-hwtools-gpio.config
endif
ifeq ($(BR2_PACKAGE_HWTOOLS_BOOT_BUTTON),y)
HWTOOLS_SOURCES += $(@D)/boot_button.c
HWTOOLS_CFLAGS += -DHWTOOLS_BOOT_BUTTON
HWTOOLS_APPLETS += boot-button
endif
ifeq ($(BR2_PACKAGE_HWTOOLS_ADC_READ),y)
HWTOOLS_SOURCES += $(@D)/adc_read.c
HWTOOLS_CFLAGS += -DHWTOOLS_ADC_READ
HWTOOLS_APPLETS += adc-read
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-hwtools-adc.config
endif
ifeq ($(BR2_PACKAGE_HWTOOLS_I2C_SCAN),y)
HWTOOLS_SOURCES += $(@D)/i2c_scan.c
HWTOOLS_CFLAGS += -DHWTOOLS_I2C_SCAN
HWTOOLS_NEEDS_PERIPHERY = y
HWTOOLS_APPLETS += i2c-scan
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-hwtools-i2c.config
endif
ifeq ($(BR2_PACKAGE_HWTOOLS_SPI_GYRO),y)
HWTOOLS_SOURCES += $(@D)/spi_gyro.c
HWTOOLS_CFLAGS += -DHWTOOLS_SPI_GYRO
HWTOOLS_NEEDS_PERIPHERY = y
HWTOOLS_APPLETS += spi-gyro
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-hwtools-spi.config
endif
ifeq ($(BR2_PACKAGE_HWTOOLS_RCC_CLOCK),y)
HWTOOLS_SOURCES += $(@D)/rcc_clock.c
HWTOOLS_CFLAGS += -DHWTOOLS_RCC_CLOCK
HWTOOLS_NEEDS_PERIPHERY = y
HWTOOLS_APPLETS += rcc-clock
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-hwtools-rcc.config
endif
ifeq ($(BR2_PACKAGE_HWTOOLS_PWM_LED),y)
HWTOOLS_SOURCES += $(@D)/pwm_led.c
HWTOOLS_CFLAGS += -DHWTOOLS_PWM_LED
HWTOOLS_NEEDS_PERIPHERY = y
HWTOOLS_APPLETS += pwm-led
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-hwtools-pwm.config
endif
ifeq ($(BR2_PACKAGE_HWTOOLS_UART_SEND),y)
HWTOOLS_SOURCES += $(@D)/uart_send.c
HWTOOLS_CFLAGS += -DHWTOOLS_UART_SEND
HWTOOLS_NEEDS_PERIPHERY = y
HWTOOLS_APPLETS += uart-send
endif
ifeq ($(BR2_PACKAGE_HWTOOLS_RS485_CHAT),y)
HWTOOLS_SOURCES += $(@D)/rs485_chat.c
HWTOOLS_CFLAGS += -DHWTOOLS_RS485_CHAT
HWTOOLS_APPLETS += rs485-chat
endif
ifeq ($(BR2_PACKAGE_HWTOOLS_TIMERS),y)
HWTOOLS_SOURCES += $(@D)/timers.c
HWTOOLS_CFLAGS += -DHWTOOLS_TIMERS
HWTOOLS_APPLETS += timers
LINUX_KCONFIG_FRAGMENT_FILES += \
	$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-sleep.config
endif

ifeq ($(HWTOOLS_NEEDS_PERIPHERY),y)
HWTOOLS_DEPENDENCIES += periphery
HWTOOLS_LIBS += -lperiphery
endif

define HWTOOLS_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		$(HWTOOLS_CFLAGS) -o $(@D)/hw $(HWTOOLS_SOURCES) $(HWTOOLS_LIBS)
endef

define HWTOOLS_INSTALL_TARGET_CMDS
	$(RM) -f $(addprefix $(TARGET_DIR)/usr/bin/,gpio-led button-led \
		boot-button adc-read i2c-scan spi-gyro rcc-clock pwm-led uart-send \
		rs485-chat timers)
	$(INSTALL) -D -m 0755 $(@D)/hw $(TARGET_DIR)/usr/bin/hw
	$(foreach applet,$(HWTOOLS_APPLETS),ln -sf hw $(TARGET_DIR)/usr/bin/$(applet);)
endef

$(eval $(generic-package))

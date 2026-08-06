LIBRARY := bin/libperiphery.a
SRC_DIR := src
OBJ_DIR := bin/obj

SOURCES := \
	$(SRC_DIR)/periphery/gpio.c \
	$(SRC_DIR)/periphery/gpio_cdev_v1.c \
	$(SRC_DIR)/periphery/gpio_cdev_v2.c \
	$(SRC_DIR)/periphery/gpio_sysfs.c \
	$(SRC_DIR)/periphery/i2c.c \
	$(SRC_DIR)/periphery/led.c \
	$(SRC_DIR)/periphery/mmio.c \
	$(SRC_DIR)/periphery/pwm.c \
	$(SRC_DIR)/periphery/serial.c \
	$(SRC_DIR)/periphery/spi.c \
	$(SRC_DIR)/periphery/version.c
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))

CC ?= gcc
AR ?= ar
CFLAGS ?= -Os -fdata-sections -ffunction-sections
CPPFLAGS ?=
INCLUDES := -Iinclude

# Build every GPIO character-device API exposed by the toolchain's Linux UAPI
# headers. When both are present, gpio.c tries v2 first and falls back to v1 at
# runtime. Very old headers use the legacy GPIO sysfs implementation.
GPIO_CDEV_V2_SUPPORTED := $(shell \
	printf '%b' '\043include <linux/gpio.h>\nint main(void) { struct gpio_v2_line_request request = {0}; request.config.flags = GPIO_V2_LINE_FLAG_EVENT_CLOCK_REALTIME; return GPIO_V2_GET_LINE_IOCTL == 0; }\n' | \
	$(CC) $(CPPFLAGS) -x c -fsyntax-only - >/dev/null 2>&1 && echo 1 || echo 0)
GPIO_CDEV_V1_SUPPORTED := $(shell \
	printf '%b' '\043include <linux/gpio.h>\nint main(void) { struct gpiohandle_request handle = {0}; struct gpioevent_request event = {0}; handle.flags = GPIOHANDLE_REQUEST_OUTPUT; return (GPIO_GET_LINEHANDLE_IOCTL == 0) || (GPIO_GET_LINEEVENT_IOCTL == 0) || event.fd; }\n' | \
	$(CC) $(CPPFLAGS) -x c -fsyntax-only - >/dev/null 2>&1 && echo 1 || echo 0)
GPIO_CDEV_SUPPORT ?= $(if $(filter 1,$(GPIO_CDEV_V2_SUPPORTED)),$(if $(filter 1,$(GPIO_CDEV_V1_SUPPORTED)),3,2),$(if $(filter 1,$(GPIO_CDEV_V1_SUPPORTED)),1,0))
DEFINES := -DPERIPHERY_GPIO_CDEV_SUPPORT=$(GPIO_CDEV_SUPPORT)

all: $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(DEFINES) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf bin

.PHONY: all clean

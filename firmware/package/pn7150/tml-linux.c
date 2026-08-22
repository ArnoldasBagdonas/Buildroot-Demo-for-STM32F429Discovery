// SPDX-License-Identifier: MIT
/* Linux I2C/GPIO transport mapping for NXP's portable NCI library. */

#include <errno.h>
#include <fcntl.h>
#include <linux/gpio.h>
#include <linux/i2c-dev.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "tml.h"
#include "tool.h"

#define PN7150_I2C_DEVICE "/dev/i2c-0"
#define PN7150_I2C_ADDRESS 0x28
#define PN7150_VEN_GPIO_CHIP "/dev/gpiochip2"
#define PN7150_VEN_LINE 8
#define PN7150_IRQ_GPIO_CHIP "/dev/gpiochip1"
#define PN7150_IRQ_LINE 7

static int i2c_fd = -1;
static int ven_fd = -1;
static int irq_fd = -1;

static int request_gpio(const char *chip_path, unsigned int offset,
			uint64_t flags, const char *consumer)
{
	struct gpio_v2_line_request request;
	int chip_fd;

	memset(&request, 0, sizeof(request));
	request.offsets[0] = offset;
	request.num_lines = 1;
	request.config.flags = flags;
	strncpy(request.consumer, consumer, sizeof(request.consumer) - 1);
	chip_fd = open(chip_path, O_RDONLY);
	if (chip_fd < 0) {
		fprintf(stderr, "TML: cannot open %s: %s\n", chip_path,
			strerror(errno));
		return -1;
	}
	if (ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &request) < 0) {
		fprintf(stderr, "TML: cannot request GPIO line %u: %s\n", offset,
			strerror(errno));
		close(chip_fd);
		return -1;
	}
	close(chip_fd);
	return request.fd;
}

static int gpio_set(int fd, bool high)
{
	struct gpio_v2_line_values values;

	memset(&values, 0, sizeof(values));
	values.mask = 1;
	values.bits = high ? 1 : 0;
	return ioctl(fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values);
}

static int gpio_get(int fd, bool *high)
{
	struct gpio_v2_line_values values;

	memset(&values, 0, sizeof(values));
	values.mask = 1;
	if (ioctl(fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &values) < 0)
		return -1;
	*high = (values.bits & 1) != 0;
	return 0;
}

static int drain_irq_events(void)
{
	struct gpio_v2_line_event event;
	struct pollfd descriptor;
	int result;

	descriptor.fd = irq_fd;
	descriptor.events = POLLIN;
	descriptor.revents = 0;
	for (;;) {
		do {
			result = poll(&descriptor, 1, 0);
		} while (result < 0 && errno == EINTR);
		if (result == 0)
			return 0;
		if (result < 0 || (descriptor.revents & POLLIN) == 0)
			return -1;
		if (read(irq_fd, &event, sizeof(event)) !=
		    (ssize_t)sizeof(event))
			return -1;
		descriptor.revents = 0;
	}
}

static int wait_for_irq(uint16_t timeout_ms)
{
	struct pollfd descriptor;
	bool high;
	int result;
#ifdef PN7150_IRQ_POLLING_FALLBACK
	uint16_t remaining = timeout_ms;
#endif

	if (irq_fd < 0)
		return -1;
	descriptor.fd = irq_fd;
	descriptor.events = POLLIN;
	descriptor.revents = 0;
	for (;;) {
		if (gpio_get(irq_fd, &high) < 0)
			return -1;
		if (high)
			return drain_irq_events();
#ifdef PN7150_IRQ_POLLING_FALLBACK
		int interval = remaining == 0 || remaining > 10 ? 10 : remaining;

		descriptor.revents = 0;
		do {
			result = poll(&descriptor, 1, interval);
		} while (result < 0 && errno == EINTR);
		if (result < 0)
			return -1;
		if (result == 0 && timeout_ms != 0) {
			remaining -= (uint16_t)interval;
			if (remaining == 0)
				return -1;
		}
#else
		do {
			result = poll(&descriptor, 1,
				timeout_ms == 0 ? -1 : timeout_ms);
		} while (result < 0 && errno == EINTR);
		if (result <= 0)
			return -1;
#endif
		if (drain_irq_events() < 0)
			return -1;
	}
}

static int read_exact(uint8_t *buffer, uint16_t length)
{
	ssize_t count;

	do {
		count = read(i2c_fd, buffer, length);
	} while (count < 0 && errno == EINTR);
	if (count != length) {
		if (count < 0)
			fprintf(stderr, "TML: I2C read failed: %s\n", strerror(errno));
		else
			fprintf(stderr, "TML: short I2C read (%d/%u)\n", (int)count,
				length);
		return -1;
	}
	return 0;
}

void tml_Connect(void)
{
	tml_Disconnect();
	printf("TML: %s address 0x%02X, VEN=%s:%u, IRQ=%s:%u\n",
	       PN7150_I2C_DEVICE, PN7150_I2C_ADDRESS,
	       PN7150_VEN_GPIO_CHIP, PN7150_VEN_LINE,
	       PN7150_IRQ_GPIO_CHIP, PN7150_IRQ_LINE);
	irq_fd = request_gpio(PN7150_IRQ_GPIO_CHIP, PN7150_IRQ_LINE,
		GPIO_V2_LINE_FLAG_INPUT | GPIO_V2_LINE_FLAG_EDGE_RISING,
		"pn7150-irq");
	ven_fd = request_gpio(PN7150_VEN_GPIO_CHIP, PN7150_VEN_LINE,
		GPIO_V2_LINE_FLAG_OUTPUT, "pn7150-ven");
	i2c_fd = open(PN7150_I2C_DEVICE, O_RDWR);
	if (i2c_fd < 0)
		fprintf(stderr, "TML: cannot open %s: %s\n", PN7150_I2C_DEVICE,
			strerror(errno));
	else if (ioctl(i2c_fd, I2C_SLAVE, PN7150_I2C_ADDRESS) < 0) {
		fprintf(stderr, "TML: cannot select I2C address 0x%02X: %s\n",
			PN7150_I2C_ADDRESS, strerror(errno));
		close(i2c_fd);
		i2c_fd = -1;
	}
	if (irq_fd < 0 || ven_fd < 0 || i2c_fd < 0)
		return;

	/* This is the reset sequence used by NXP's Cortex-M TML. */
	printf("TML: VEN high\n");
	if (gpio_set(ven_fd, true) < 0)
		goto gpio_error;
	Sleep(10);
	printf("TML: VEN low\n");
	if (gpio_set(ven_fd, false) < 0)
		goto gpio_error;
	Sleep(10);
	printf("TML: VEN high (reset released)\n");
	if (gpio_set(ven_fd, true) < 0)
		goto gpio_error;
	Sleep(10);
	return;

gpio_error:
	fprintf(stderr, "TML: cannot drive VEN: %s\n", strerror(errno));
}

void tml_Disconnect(void)
{
	if (i2c_fd >= 0)
		close(i2c_fd);
	if (ven_fd >= 0)
		close(ven_fd);
	if (irq_fd >= 0)
		close(irq_fd);
	i2c_fd = -1;
	ven_fd = -1;
	irq_fd = -1;
}

void tml_Send(uint8_t *buffer, uint16_t length, uint16_t *bytes_sent)
{
	ssize_t count;

	*bytes_sent = 0;
	if (i2c_fd < 0)
		return;
	do {
		count = write(i2c_fd, buffer, length);
	} while (count < 0 && errno == EINTR);
	if (count != length) {
		if (count < 0)
			fprintf(stderr, "TML: I2C write failed: %s\n", strerror(errno));
		else
			fprintf(stderr, "TML: short I2C write (%d/%u)\n", (int)count,
				length);
		Sleep(10);
		do {
			count = write(i2c_fd, buffer, length);
		} while (count < 0 && errno == EINTR);
	}
	if (count == length)
		*bytes_sent = length;
}

void tml_Receive(uint8_t *buffer, uint16_t capacity, uint16_t *bytes_read,
		 uint16_t timeout_ms)
{
	uint16_t frame_length;

	*bytes_read = 0;
	if (i2c_fd < 0 || wait_for_irq(timeout_ms) < 0)
		return;
	if (capacity < 3 || read_exact(buffer, 3) < 0)
		return;
	frame_length = (uint16_t)buffer[2] + 3;
	if (frame_length > capacity) {
		fprintf(stderr, "TML: NCI frame needs %u bytes, buffer has %u\n",
			frame_length, capacity);
		return;
	}
	if (buffer[2] != 0 && read_exact(buffer + 3, buffer[2]) < 0)
		return;
	*bytes_read = frame_length;
}

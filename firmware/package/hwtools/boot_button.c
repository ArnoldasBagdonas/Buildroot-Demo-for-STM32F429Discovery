#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BUTTON_PROPERTY \
	"/sys/firmware/devicetree/base/chosen/bootloader,user-button"

static void usage(FILE *stream)
{
	fputs("Usage: boot-button\n"
	      "Report the user-button state sampled by AFBOOT.\n",
	      stream);
}

int boot_button_main(int argc, char **argv)
{
	uint8_t data[4];
	ssize_t length;
	uint32_t value;
	int fd;

	if (argc == 2 && !strcmp(argv[1], "--help")) {
		usage(stdout);
		return 0;
	}
	if (argc != 1) {
		usage(stderr);
		return 2;
	}

	fd = open(BUTTON_PROPERTY, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "boot-button: cannot open %s: %s\n",
			BUTTON_PROPERTY, strerror(errno));
		return 1;
	}
	length = read(fd, data, sizeof(data));
	if (length < 0) {
		fprintf(stderr, "boot-button: cannot read %s: %s\n",
			BUTTON_PROPERTY, strerror(errno));
		close(fd);
		return 1;
	}
	close(fd);
	if (length != sizeof(data)) {
		fprintf(stderr, "boot-button: expected a 4-byte device-tree value\n");
		return 1;
	}

	value = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
		((uint32_t)data[2] << 8) | data[3];
	if (value > 1) {
		fprintf(stderr, "boot-button: invalid value %lu\n",
			(unsigned long)value);
		return 1;
	}
	printf("bootloader user button: %s\n", value ? "pressed" : "not pressed");
	return 0;
}

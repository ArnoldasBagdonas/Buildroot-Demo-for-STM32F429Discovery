#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ADC_CHANNEL 13
#define ADC_MAX_RAW 4095
#define ADC_REFERENCE_MV 3300
#define MAX_IIO_DEVICES 32

static void usage(FILE *stream)
{
	fputs("Usage: adc-read [sample-count [interval-ms]]\n"
	      "Read ADC1_IN13 on PC3 (P2 pin 13).\n"
	      "Defaults: sample-count=1, interval-ms=1000.\n",
	      stream);
}

static int parse_number(const char *text, unsigned long minimum,
			unsigned long maximum, unsigned long *value)
{
	char *end;
	unsigned long parsed;

	errno = 0;
	parsed = strtoul(text, &end, 10);
	if (errno || *text == '\0' || *end != '\0' ||
	    parsed < minimum || parsed > maximum)
		return -1;
	*value = parsed;
	return 0;
}

static int read_text(const char *path, char *buffer, size_t size)
{
	ssize_t length;
	int fd;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;
	length = read(fd, buffer, size - 1);
	if (length < 0) {
		int saved_errno = errno;

		close(fd);
		errno = saved_errno;
		return -1;
	}
	close(fd);
	buffer[length] = '\0';
	while (length > 0 &&
	       (buffer[length - 1] == '\n' || buffer[length - 1] == '\r'))
		buffer[--length] = '\0';
	return 0;
}

static int find_adc_device(char *path, size_t size)
{
	char raw_path[PATH_MAX];
	int index;

	for (index = 0; index < MAX_IIO_DEVICES; ++index) {
		if (snprintf(path, size, "/sys/bus/iio/devices/iio:device%d",
			     index) >= (int)size)
			return -1;
		if (snprintf(raw_path, sizeof(raw_path), "%s/in_voltage%d_raw",
			     path, ADC_CHANNEL) >= (int)sizeof(raw_path))
			return -1;
		if (access(raw_path, R_OK) == 0)
			return 0;
	}
	errno = ENOENT;
	return -1;
}

static int sleep_milliseconds(unsigned long milliseconds)
{
	struct timespec delay = {
		.tv_sec = (time_t)(milliseconds / 1000),
		.tv_nsec = (long)(milliseconds % 1000) * 1000000L,
	};

	while (nanosleep(&delay, &delay) < 0)
		if (errno != EINTR)
			return -1;
	return 0;
}

int adc_read_main(int argc, char **argv)
{
	unsigned long sample_count = 1;
	unsigned long interval_ms = 1000;
	char device_path[PATH_MAX];
	char raw_path[PATH_MAX];
	char scale_path[PATH_MAX];
	char buffer[64];
	char scale[64];
	unsigned long sample;
	int have_scale;

	if (argc > 1 && !strcmp(argv[1], "--help")) {
		usage(stdout);
		return 0;
	}
	if (argc > 3 ||
	    (argc > 1 && parse_number(argv[1], 1, 1000000, &sample_count)) ||
	    (argc > 2 && parse_number(argv[2], 0, 3600000, &interval_ms))) {
		usage(stderr);
		return 2;
	}

	if (find_adc_device(device_path, sizeof(device_path)) < 0) {
		fprintf(stderr,
			"adc-read: ADC1_IN13 is not available in Linux IIO sysfs\n");
		return 1;
	}
	if (snprintf(raw_path, sizeof(raw_path), "%s/in_voltage%d_raw",
		     device_path, ADC_CHANNEL) >= (int)sizeof(raw_path) ||
	    snprintf(scale_path, sizeof(scale_path), "%s/in_voltage%d_scale",
		     device_path, ADC_CHANNEL) >= (int)sizeof(scale_path)) {
		fputs("adc-read: IIO path is too long\n", stderr);
		return 1;
	}
	have_scale = read_text(scale_path, scale, sizeof(scale)) == 0;

	for (sample = 0; sample < sample_count; ++sample) {
		char *end;
		long raw;
		long millivolts;

		if (read_text(raw_path, buffer, sizeof(buffer)) < 0) {
			fprintf(stderr, "adc-read: cannot read %s: %s\n",
				raw_path, strerror(errno));
			return 1;
		}
		errno = 0;
		raw = strtol(buffer, &end, 10);
		if (errno || end == buffer || *end != '\0' ||
		    raw < 0 || raw > ADC_MAX_RAW) {
			fprintf(stderr, "adc-read: invalid raw value: %s\n", buffer);
			return 1;
		}
		millivolts = (raw * ADC_REFERENCE_MV + ADC_MAX_RAW / 2) /
			ADC_MAX_RAW;
		printf("ADC1_IN13 PC3 (P2 pin 13): raw=%ld/%d, estimated=%ld mV",
		       raw, ADC_MAX_RAW, millivolts);
		if (have_scale)
			printf(", scale=%s mV/LSB", scale);
		putchar('\n');
		fflush(stdout);

		if (sample + 1 < sample_count && sleep_milliseconds(interval_ms)) {
			fprintf(stderr, "adc-read: sleep failed: %s\n",
				strerror(errno));
			return 1;
		}
	}
	return 0;
}

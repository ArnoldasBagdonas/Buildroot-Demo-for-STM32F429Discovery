#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested;

static void request_stop(int signal_number)
{
	(void)signal_number;
	stop_requested = 1;
}

static int write_all(int fd, const unsigned char *buffer, size_t length)
{
	while (length > 0) {
		ssize_t written = write(fd, buffer, length);

		if (written < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (written == 0) {
			errno = EIO;
			return -1;
		}

		buffer += written;
		length -= (size_t)written;
	}

	return 0;
}

static void make_raw(struct termios *settings)
{
	settings->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP |
				 INLCR | IGNCR | ICRNL | IXON);
	settings->c_oflag &= ~OPOST;
	settings->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	settings->c_cflag &= ~(CSIZE | PARENB);
	settings->c_cflag |= CS8 | CLOCAL | CREAD;
	settings->c_cc[VMIN] = 1;
	settings->c_cc[VTIME] = 0;
}

static void print_received(const unsigned char *buffer, size_t length)
{
	size_t index;

	printf("USB RX %lu byte%s: \"", (unsigned long)length,
	       length == 1 ? "" : "s");
	for (index = 0; index < length; index++) {
		unsigned char value = buffer[index];

		switch (value) {
		case '\\':
			fputs("\\\\", stdout);
			break;
		case '"':
			fputs("\\\"", stdout);
			break;
		case '\r':
			fputs("\\r", stdout);
			break;
		case '\n':
			fputs("\\n", stdout);
			break;
		case '\t':
			fputs("\\t", stdout);
			break;
		default:
			if (value >= 0x20 && value <= 0x7e)
				putchar(value);
			else
				printf("\\x%02x", value);
			break;
		}
	}
	puts("\"");
	fflush(stdout);
}

int main(int argc, char **argv)
{
	const char *device;
	unsigned char buffer[256];
	struct termios saved_settings;
	struct termios raw_settings;
	struct sigaction action;
	struct pollfd descriptors[2];
	int fd;
	int result = EXIT_SUCCESS;

	if (argc > 2) {
		fprintf(stderr, "Usage: %s [device]\n", argv[0]);
		return EXIT_FAILURE;
	}
	if (argc == 2 && strcmp(argv[1], "--help") == 0) {
		printf("Usage: %s [device]\n", argv[0]);
		return EXIT_SUCCESS;
	}
	device = argc == 2 ? argv[1] : "/dev/ttyGS0";

	fd = open(device, O_RDWR | O_NOCTTY);
	if (fd < 0) {
		fprintf(stderr, "usbserialchat: cannot open %s: %s\n",
			device, strerror(errno));
		return EXIT_FAILURE;
	}

	if (tcgetattr(fd, &saved_settings) < 0) {
		fprintf(stderr, "usbserialchat: tcgetattr(%s): %s\n",
			device, strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}

	raw_settings = saved_settings;
	make_raw(&raw_settings);
	if (tcsetattr(fd, TCSANOW, &raw_settings) < 0) {
		fprintf(stderr, "usbserialchat: tcsetattr(%s): %s\n",
			device, strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}

	memset(&action, 0, sizeof(action));
	action.sa_handler = request_stop;
	sigemptyset(&action.sa_mask);
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGHUP, &action, NULL);

	printf("USB serial loopback active on %s.\n", device);
	printf("Received bytes are printed here; press Ctrl-C, or type q then Enter, to stop.\n");
	fflush(stdout);

	descriptors[0].fd = fd;
	descriptors[0].events = POLLIN;
	descriptors[1].fd = STDIN_FILENO;
	descriptors[1].events = POLLIN;

	while (!stop_requested) {
		int ready = poll(descriptors, 2, -1);
		ssize_t received;

		if (ready < 0) {
			if (errno == EINTR)
				continue;
			fprintf(stderr, "usbserialchat: poll: %s\n",
				strerror(errno));
			result = EXIT_FAILURE;
			break;
		}

		if (descriptors[1].revents & (POLLIN | POLLHUP)) {
			unsigned char console_input[32];
			ssize_t console_length;
			size_t index;

			console_length = read(STDIN_FILENO, console_input,
					      sizeof(console_input));
			if (console_length <= 0) {
				stop_requested = 1;
			} else {
				for (index = 0; index < (size_t)console_length;
				     index++) {
					if (console_input[index] == 'q' ||
					    console_input[index] == 'Q') {
						stop_requested = 1;
						break;
					}
				}
			}
		}
		if (stop_requested)
			break;

		if (!(descriptors[0].revents & (POLLIN | POLLHUP | POLLERR)))
			continue;

		received = read(fd, buffer, sizeof(buffer));

		if (received < 0) {
			if (errno == EINTR)
				continue;
			fprintf(stderr, "usbserialchat: read(%s): %s\n",
				device, strerror(errno));
			result = EXIT_FAILURE;
			break;
		}
		if (received == 0) {
			fprintf(stderr, "usbserialchat: host disconnected\n");
			break;
		}

		if (write_all(fd, buffer, (size_t)received) < 0) {
			fprintf(stderr, "usbserialchat: write(%s): %s\n",
				device, strerror(errno));
			result = EXIT_FAILURE;
			break;
		}
		print_received(buffer, (size_t)received);
	}

	printf("usbserialchat: stopped\n");
	fflush(stdout);

	if (tcsetattr(fd, TCSANOW, &saved_settings) < 0) {
		fprintf(stderr, "usbserialchat: cannot restore %s settings: %s\n",
			device, strerror(errno));
		result = EXIT_FAILURE;
	}
	close(fd);
	return result;
}

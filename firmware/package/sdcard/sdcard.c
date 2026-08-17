#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static const char mount_point[] = "/mnt/sdcard";
static const char automount_state[] = "/run/sdcard-automount";
static volatile sig_atomic_t watch_stop;

struct card_device {
	char card[64];
	char mount[64];
	char name[32];
};

#if defined(SDCARD_AUTOMOUNT_PERIODIC) || defined(SDCARD_AUTOMOUNT_ONCE)
static const char *program_name(const char *path)
{
	const char *slash = strrchr(path, '/');

	return slash ? slash + 1 : path;
}
#endif

static void watch_signal(int signal_number)
{
	(void)signal_number;
	watch_stop = 1;
}

static int set_automount(const char *state)
{
	FILE *file = fopen(automount_state, "w");

	if (!file)
		return -1;
	if (fprintf(file, "%s\n", state) < 0 || fclose(file) < 0)
		return -1;
	return 0;
}

static bool automount_enabled(void)
{
	char state[32];
	FILE *file = fopen(automount_state, "r");

	if (!file)
		return true;
	if (!fgets(state, sizeof(state), file)) {
		fclose(file);
		return true;
	}
	fclose(file);
	state[strcspn(state, "\r\n")] = '\0';
	return !strcmp(state, "enabled");
}

static int find_card(struct card_device *device)
{
	struct stat status;
	int index;

	for (index = 0; index < 10; ++index) {
		char sys_path[64];

		snprintf(device->name, sizeof(device->name), "mmcblk%d", index);
		snprintf(sys_path, sizeof(sys_path), "/sys/class/block/%s",
		         device->name);
		if (stat(sys_path, &status) < 0)
			continue;
		snprintf(device->card, sizeof(device->card), "/dev/%s",
		         device->name);
		snprintf(device->mount, sizeof(device->mount), "/dev/mmcblk%dp1",
		         index);
		if (stat(device->mount, &status) < 0 || !S_ISBLK(status.st_mode))
			strcpy(device->mount, device->card);
		return 0;
	}
	return -1;
}

static bool is_mounted(void)
{
	char source[PATH_MAX];
	char target[PATH_MAX];
	FILE *mounts = fopen("/proc/mounts", "r");
	bool mounted = false;

	if (!mounts)
		return false;
	while (fscanf(mounts, "%1023s %1023s %*s %*s %*d %*d",
	              source, target) == 2) {
		if (!strcmp(target, mount_point)) {
			mounted = true;
			break;
		}
	}
	fclose(mounts);
	return mounted;
}

static int mount_card(bool report)
{
	struct card_device device;

	set_automount("enabled");
	if (find_card(&device) < 0) {
		if (report) {
			fputs("No SPI SD card was detected.\n", stderr);
			fputs("Check the card, wiring, and flashed -sdcard DTB.\n",
			      stderr);
		}
		return 1;
	}
	if (mkdir(mount_point, 0755) < 0 && errno != EEXIST) {
		if (report)
			perror("sdcard: create mount point");
		return 1;
	}
	if (!is_mounted() && mount(device.mount, mount_point, "vfat", 0, NULL) < 0) {
		if (report)
			perror("sdcard: mount vfat");
		return 1;
	}
	if (report)
		printf("SD card mounted at %s\n", mount_point);
	return 0;
}

static int show_status(void)
{
	struct card_device device;

	printf("Automount: %s\n", automount_enabled() ? "enabled" :
	       "disabled until the card is removed");
	if (find_card(&device) < 0) {
		puts("SD card: not detected");
		return 1;
	}
	printf("Card device:  %s\nMount device: %s\n", device.card, device.mount);
	{
		char size_path[96];
		char sectors[64];
		FILE *file;

		snprintf(size_path, sizeof(size_path), "/sys/class/block/%s/size",
		         device.name);
		file = fopen(size_path, "r");
		if (file && fgets(sectors, sizeof(sectors), file)) {
			sectors[strcspn(sectors, "\r\n")] = '\0';
			printf("Size sectors: %s (512 bytes each)\n", sectors);
		}
		if (file)
			fclose(file);
	}
	printf("FAT filesystem: %s\n", is_mounted() ?
	       "mounted at /mnt/sdcard" : "not mounted");
	return 0;
}

static int unmount_card(void)
{
	set_automount("disabled");
	if (!is_mounted())
		return 0;
	sync();
	if (umount2(mount_point, 0) < 0) {
		perror("sdcard: unmount");
		return 1;
	}
	return 0;
}

static int watch_card(void)
{
	struct sigaction action;
	struct timespec delay = { .tv_sec = 1, .tv_nsec = 0 };

	memset(&action, 0, sizeof(action));
	action.sa_handler = watch_signal;
	sigemptyset(&action.sa_mask);
	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGINT, &action, NULL);
	if (access(automount_state, F_OK) < 0)
		set_automount("enabled");
	while (!watch_stop) {
		struct card_device device;

		if (find_card(&device) == 0) {
			if (automount_enabled() && !is_mounted())
				mount_card(false);
		} else {
			if (is_mounted())
				umount2(mount_point, MNT_DETACH);
			set_automount("enabled");
		}
		nanosleep(&delay, NULL);
	}
	return 0;
}

static void usage(void)
{
	fputs("Usage: sdcard {status|mount|unmount|watch}\n", stderr);
}

int sdcard_main(int argc, char **argv)
{
	const char *command = argc > 1 ? argv[1] : "status";

#if defined(SDCARD_AUTOMOUNT_PERIODIC) || defined(SDCARD_AUTOMOUNT_ONCE)
	const char *name = program_name(argv[0]);

	if (!strcmp(name, "sdcard-auto")) {
		if (argc != 1) {
			fputs("Usage: sdcard-auto\n", stderr);
			return 2;
		}
#if defined(SDCARD_AUTOMOUNT_PERIODIC)
		return watch_card();
#else
		return mount_card(false);
#endif
	}
#endif

	if (argc > 2) {
		usage();
		return 2;
	}
	if (!strcmp(command, "status"))
		return show_status();
	if (!strcmp(command, "mount"))
		return mount_card(true);
	if (!strcmp(command, "unmount"))
		return unmount_card();
	if (!strcmp(command, "watch"))
		return watch_card();
	usage();
	return 2;
}

#ifndef SDCARD_MULTICALL
int main(int argc, char **argv)
{
	return sdcard_main(argc, argv);
}
#endif

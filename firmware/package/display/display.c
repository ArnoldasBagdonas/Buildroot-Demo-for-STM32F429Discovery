#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

static const char pid_file[] = "/run/display-auto.pid";
static const char source_file[] = "/run/display-source";
static const char builtin_directory[] = "/usr/share/display";
static const char sdcard_directory[] = "/mnt/sdcard";

static volatile sig_atomic_t stop_requested;
static volatile sig_atomic_t slideshow_pid;

int display_pattern_main(int argc, char **argv);
#ifdef WITH_SDCARD
int sdcard_main(int argc, char **argv);
#endif

struct image_list {
	char **names;
	size_t count;
};

static const char *program_name(const char *path)
{
	const char *slash = strrchr(path, '/');

	return slash ? slash + 1 : path;
}

static void sleep_milliseconds(long milliseconds)
{
	struct timespec remaining;

	remaining.tv_sec = milliseconds / 1000;
	remaining.tv_nsec = (milliseconds % 1000) * 1000000L;
	while (nanosleep(&remaining, &remaining) < 0 && errno == EINTR) {
		if (stop_requested)
			break;
	}
}

static int write_state(const char *path, const char *value)
{
	FILE *file = fopen(path, "w");

	if (!file)
		return -1;
	if (fprintf(file, "%s\n", value) < 0 || fclose(file) < 0)
		return -1;
	return 0;
}

static void clear_state(void)
{
	unlink(pid_file);
	unlink(source_file);
}

static pid_t read_service_pid(void)
{
	char text[32];
	char *end;
	long value;
	FILE *file = fopen(pid_file, "r");

	if (!file)
		return -1;
	if (!fgets(text, sizeof(text), file)) {
		fclose(file);
		return -1;
	}
	fclose(file);
	errno = 0;
	value = strtol(text, &end, 10);
	if (errno || end == text || (*end && *end != '\n') || value <= 1 ||
	    value > INT_MAX)
		return -1;
	return (pid_t)value;
}

static bool process_is_running(pid_t pid)
{
	return pid > 1 && (kill(pid, 0) == 0 || errno == EPERM);
}

static bool sdcard_is_mounted(void)
{
	char source[PATH_MAX];
	char target[PATH_MAX];
	FILE *mounts = fopen("/proc/mounts", "r");
	bool mounted = false;

	if (!mounts)
		return false;
	while (fscanf(mounts, "%1023s %1023s %*s %*s %*d %*d",
	              source, target) == 2) {
		if (!strcmp(target, sdcard_directory)) {
			mounted = true;
			break;
		}
	}
	fclose(mounts);
	return mounted;
}

static bool supported_image(const char *name)
{
	static const char *const extensions[] = {
		".png", ".jpg", ".jpeg", ".gif", ".bmp"
	};
	const char *dot = strrchr(name, '.');
	size_t index;

	if (!dot)
		return false;
	for (index = 0; index < ARRAY_SIZE(extensions); ++index) {
		if (!strcasecmp(dot, extensions[index]))
			return true;
	}
	return false;
}

static int compare_names(const void *left, const void *right)
{
	const char *const *a = left;
	const char *const *b = right;

	return strcmp(*a, *b);
}

static void free_images(struct image_list *images)
{
	size_t index;

	for (index = 0; index < images->count; ++index)
		free(images->names[index]);
	free(images->names);
	images->names = NULL;
	images->count = 0;
}

static int collect_images(const char *directory, struct image_list *images)
{
	struct dirent *entry;
	DIR *stream;

	memset(images, 0, sizeof(*images));
	stream = opendir(directory);
	if (!stream)
		return -1;
	while ((entry = readdir(stream))) {
		char path[PATH_MAX];
		char **names;
		struct stat status;

		if (!supported_image(entry->d_name))
			continue;
		if (snprintf(path, sizeof(path), "%s/%s", directory,
		             entry->d_name) >= (int)sizeof(path))
			continue;
		if (stat(path, &status) < 0 || !S_ISREG(status.st_mode))
			continue;
		names = realloc(images->names,
		                (images->count + 1) * sizeof(*images->names));
		if (!names) {
			closedir(stream);
			free_images(images);
			errno = ENOMEM;
			return -1;
		}
		images->names = names;
		images->names[images->count] = strdup(path);
		if (!images->names[images->count]) {
			closedir(stream);
			free_images(images);
			errno = ENOMEM;
			return -1;
		}
		++images->count;
	}
	closedir(stream);
	qsort(images->names, images->count, sizeof(*images->names), compare_names);
	return 0;
}

static int activate_display(void)
{
	static const char mode_path[] = "/sys/class/graphics/fb0/mode";
	static const char modes_path[] = "/sys/class/graphics/fb0/modes";
	char mode[64];
	FILE *file = fopen(mode_path, "r");

	if (!file) {
		perror("display: open framebuffer mode");
		return -1;
	}
	if (!fgets(mode, sizeof(mode), file)) {
		fclose(file);
		file = fopen(modes_path, "r");
		if (!file || !fgets(mode, sizeof(mode), file)) {
			if (file)
				fclose(file);
			fprintf(stderr, "display: framebuffer has no display mode\n");
			return -1;
		}
	}
	fclose(file);
	file = fopen(mode_path, "w");
	if (!file) {
		perror("display: activate framebuffer mode");
		return -1;
	}
	if (fputs(mode, file) == EOF || fclose(file) < 0) {
		perror("display: activate framebuffer mode");
		return -1;
	}
	return 0;
}

static int stop_service(bool quiet)
{
	pid_t pid = read_service_pid();
	int count;

	if (!process_is_running(pid)) {
		clear_state();
		if (!quiet)
			puts("display-auto: not running");
		return 1;
	}
	if (kill(pid, SIGTERM) < 0) {
		if (!quiet)
			perror("display-auto: stop");
		return 1;
	}
	for (count = 0; count < 20 && process_is_running(pid); ++count)
		sleep_milliseconds(100);
	clear_state();
	if (!quiet)
		puts("display-auto: stopped");
	return 0;
}

static int parse_delay(const char *text, unsigned int *delay, bool autoplay)
{
	char *end;
	unsigned long value;

	errno = 0;
	value = strtoul(text, &end, 10);
	if (errno || end == text || *end || value > UINT_MAX / 10 ||
	    (autoplay && value == 0))
		return -1;
	*delay = (unsigned int)value;
	return 0;
}

static int run_fbv(const char *directory, unsigned int delay,
	           bool autoplay, bool replace_process)
{
	struct image_list images;
	char delay_tenths[16];
	char **arguments;
	size_t fixed = autoplay ? 10 : 9;
	size_t index;
	pid_t pid;
	int status = 0;
	int waited = 0;

	if (collect_images(directory, &images) < 0 || !images.count) {
		free_images(&images);
		return 1;
	}
	if (activate_display() < 0) {
		free_images(&images);
		return 1;
	}
	snprintf(delay_tenths, sizeof(delay_tenths), "%u", delay * 10);
	arguments = calloc(fixed + images.count, sizeof(*arguments));
	if (!arguments) {
		free_images(&images);
		return 1;
	}
	index = 0;
	arguments[index++] = (char *)"fbv";
	if (autoplay)
		arguments[index++] = (char *)"--noinput";
	arguments[index++] = (char *)"-c";
	arguments[index++] = (char *)"-u";
	arguments[index++] = (char *)"-i";
	arguments[index++] = (char *)"-k";
	arguments[index++] = (char *)"-e";
	arguments[index++] = (char *)"-s";
	arguments[index++] = delay_tenths;
	memcpy(arguments + index, images.names, images.count * sizeof(*arguments));
	index += images.count;
	arguments[index] = NULL;

	if (replace_process) {
		execv("/usr/bin/fbv", arguments);
		perror("display: execute fbv");
		free(arguments);
		free_images(&images);
		return 1;
	}

	pid = vfork();
	if (pid == 0) {
		execv("/usr/bin/fbv", arguments);
		_exit(127);
	}
	if (pid < 0) {
		free(arguments);
		free_images(&images);
		return 1;
	}
	slideshow_pid = pid;
	while (!(waited = waitpid(pid, &status, 0)))
		;
	while (waited < 0) {
		if (errno != EINTR)
			break;
		if (stop_requested)
			kill(pid, SIGTERM);
		waited = waitpid(pid, &status, 0);
	}
	slideshow_pid = 0;
	free(arguments);
	free_images(&images);
	return waited > 0 && WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

static const char *automatic_source(void)
{
	struct image_list images = { 0 };

	if (sdcard_is_mounted() && collect_images(sdcard_directory, &images) == 0 &&
	    images.count) {
		free_images(&images);
		return sdcard_directory;
	}
	free_images(&images);
	return builtin_directory;
}

static void worker_signal(int signal_number)
{
	(void)signal_number;
	stop_requested = 1;
	if (slideshow_pid > 1)
		kill(slideshow_pid, SIGTERM);
}

static int run_worker(unsigned int delay)
{
	struct sigaction action;
	pid_t saved_pid;
	int count;

	memset(&action, 0, sizeof(action));
	action.sa_handler = worker_signal;
	sigemptyset(&action.sa_mask);
	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGHUP, &action, NULL);

	for (count = 0; count < 10 && access("/dev/fb0", F_OK) < 0; ++count)
		sleep_milliseconds(1000);
	if (access("/usr/sbin/sdcard", X_OK) == 0) {
		for (count = 0; count < 5 && !sdcard_is_mounted(); ++count)
			sleep_milliseconds(1000);
	}
	while (!stop_requested) {
		const char *source = automatic_source();

		write_state(source_file, source);
		run_fbv(source, delay, true, false);
		sleep_milliseconds(1000);
	}
	saved_pid = read_service_pid();
	if (saved_pid == getpid())
		clear_state();
	return 0;
}

static void display_usage(void)
{
	fputs("Usage: display [--autoplay] [delay-seconds] [image-directory]\n",
	      stderr);
}

static int display_main(int argc, char **argv)
{
	const char *directory = NULL;
	const char *delay_text = "3";
	bool autoplay = false;
	bool explicit_directory = false;
	unsigned int delay;
	struct image_list images;

	if (argc > 1 && !strcmp(argv[1], "--autoplay")) {
		autoplay = true;
		--argc;
		++argv;
	} else if (argc > 1 && (!strcmp(argv[1], "-h") ||
	                       !strcmp(argv[1], "--help"))) {
		display_usage();
		return 0;
	}
	if (argc > 1)
		delay_text = argv[1];
	if (argc > 2) {
		directory = argv[2];
		explicit_directory = true;
	}
	if (argc > 3 || parse_delay(delay_text, &delay, autoplay) < 0) {
		display_usage();
		return 2;
	}
	if (!directory)
		directory = automatic_source();
	if (!autoplay && access("/usr/bin/display-auto", X_OK) == 0)
		stop_service(true);
	if (access("/dev/fb0", F_OK) < 0) {
		fputs("display: /dev/fb0 is missing; select Display, rebuild, and flash\n",
		      stderr);
		return 1;
	}
	if (collect_images(directory, &images) < 0 || !images.count) {
		if (explicit_directory)
			fprintf(stderr, "display: no supported images in %s\n", directory);
		else
			fputs("display: no usable SD-card or built-in images were found\n",
			      stderr);
		free_images(&images);
		return 1;
	}
	printf("Showing %lu image(s) from %s; delay %us.\n",
	       (unsigned long)images.count, directory, delay);
	if (!autoplay)
		puts("Press q to quit, Space/Enter for next, or </> to navigate.");
	free_images(&images);
	if (autoplay)
		write_state(source_file, directory);
	return run_fbv(directory, delay, autoplay, true);
}

static void auto_usage(void)
{
	fputs("Usage: display-auto {start [delay-seconds]|stop|restart "
	      "[delay-seconds]|status}\n", stderr);
}

static int auto_main(int argc, char **argv)
{
	const char *command = argc > 1 ? argv[1] : "status";
	const char *delay_text = argc > 2 ? argv[2] : "3";
	unsigned int delay;
	pid_t pid;

	if (!strcmp(command, "run")) {
		if (parse_delay(delay_text, &delay, true) < 0)
			return 2;
		return run_worker(delay);
	}
	if (!strcmp(command, "status")) {
		char source[PATH_MAX];
		FILE *file;

		pid = read_service_pid();
		if (!process_is_running(pid)) {
			puts("display-auto: stopped");
			return 1;
		}
		printf("display-auto: running (PID %ld)\n", (long)pid);
		file = fopen(source_file, "r");
		if (file && fgets(source, sizeof(source), file)) {
			source[strcspn(source, "\r\n")] = '\0';
			printf("Image source: %s\n", source);
		} else {
			puts("Image source: waiting for display or automount");
		}
		if (file)
			fclose(file);
		return 0;
	}
	if (!strcmp(command, "stop"))
		return stop_service(false);
	if (!strcmp(command, "restart"))
		stop_service(true);
	else if (strcmp(command, "start")) {
		auto_usage();
		return 2;
	}
	if (argc > 3 || parse_delay(delay_text, &delay, true) < 0) {
		auto_usage();
		return 2;
	}
	pid = read_service_pid();
	if (process_is_running(pid)) {
		printf("display-auto: already running (PID %ld)\n", (long)pid);
		return 0;
	}
	clear_state();
	pid = vfork();
	if (pid == 0) {
		char *const arguments[] = {
			(char *)"display-auto", (char *)"run", (char *)delay_text, NULL
		};

		execv("/usr/bin/display-auto", arguments);
		_exit(127);
	}
	if (pid < 0) {
		perror("display-auto: start");
		return 1;
	}
	{
		char text[32];

		snprintf(text, sizeof(text), "%ld", (long)pid);
		if (write_state(pid_file, text) < 0) {
			kill(pid, SIGTERM);
			perror("display-auto: write PID file");
			return 1;
		}
	}
	printf("display-auto: started (PID %ld)\n", (long)pid);
	return 0;
}

int main(int argc, char **argv)
{
	const char *name = program_name(argv[0]);

	if (!strcmp(name, "display-pattern"))
		return display_pattern_main(argc, argv);
	if (!strcmp(name, "display-auto"))
		return auto_main(argc, argv);
#ifdef WITH_SDCARD
	if (!strcmp(name, "sdcard"))
		return sdcard_main(argc, argv);
#endif
	return display_main(argc, argv);
}

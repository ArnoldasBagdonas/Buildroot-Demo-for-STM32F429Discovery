#include <stdio.h>
#include <string.h>

#include "display.h"

static const char *program_name(const char *path)
{
	const char *slash = strrchr(path, '/');

	return slash ? slash + 1 : path;
}

#ifdef WITH_SDCARD
int sdcard_main(int argc, char **argv);
#endif

#ifdef WITH_SDCARD_IMAGES
#include <limits.h>

static const char sdcard_directory[] = "/mnt/sdcard";

static int sdcard_is_mounted(void)
{
	char source[PATH_MAX];
	char target[PATH_MAX];
	FILE *mounts = fopen("/proc/mounts", "r");
	int mounted = 0;

	if (!mounts)
		return 0;
	while (fscanf(mounts, "%1023s %1023s %*s %*s %*d %*d",
	              source, target) == 2) {
		if (!strcmp(target, sdcard_directory)) {
			mounted = 1;
			break;
		}
	}
	fclose(mounts);
	return mounted;
}

static const char *gallery_source(void)
{
	if (sdcard_is_mounted())
		return sdcard_directory;
	return NULL;
}
#endif

static void usage(void)
{
	fputs("Gallery multicall applets: gallery, display, display-pattern, "
	      "display-auto, gallery-auto"
#ifdef WITH_SDCARD
	      ", sdcard"
#endif
	      "\n", stderr);
}

int main(int argc, char **argv)
{
	const char *name = program_name(argv[0]);

#ifdef WITH_SDCARD_IMAGES
	display_set_source_provider(gallery_source);
#endif
#ifdef WITH_SDCARD
	if (!strcmp(name, "sdcard"))
		return sdcard_main(argc, argv);
#endif
	if (!strcmp(name, "display-pattern"))
		return display_pattern_main(argc, argv);
	if (!strcmp(name, "display-auto") || !strcmp(name, "gallery-auto"))
		return display_auto_applet_main(argc, argv);
	if (!strcmp(name, "display") || !strcmp(name, "gallery"))
		return display_applet_main(argc, argv);
	usage();
	return 2;
}

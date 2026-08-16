#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static ssize_t read_display_mode(const char *path, char *mode, size_t size)
{
	ssize_t count;
	int descriptor;

	descriptor = open(path, O_RDONLY);
	if (descriptor < 0) {
		perror("display-pattern: open framebuffer mode list");
		return -1;
	}
	count = read(descriptor, mode, size);
	close(descriptor);
	if (count < 0) {
		perror("display-pattern: read framebuffer mode list");
		return -1;
	}
	return count;
}

static int activate_display(void)
{
	static const char current_mode_path[] = "/sys/class/graphics/fb0/mode";
	static const char available_modes_path[] = "/sys/class/graphics/fb0/modes";
	char mode[64];
	ssize_t count;
	ssize_t offset;
	int descriptor;

	count = read_display_mode(current_mode_path, mode, sizeof(mode));
	if (count < 0)
		return -1;
	/* Before the first DRM modeset, "mode" is empty. Select the first mode
	 * advertised by "modes" so the panel and LTDC are actually enabled. */
	if (!count) {
		count = read_display_mode(available_modes_path, mode, sizeof(mode));
		if (count < 0)
			return -1;
		if (!count) {
			fprintf(stderr, "display-pattern: framebuffer has no display mode\n");
			return -1;
		}
	}

	descriptor = open(current_mode_path, O_WRONLY);
	if (descriptor < 0) {
		perror("display-pattern: open framebuffer mode for activation");
		return -1;
	}
	for (offset = 0; offset < count;) {
		ssize_t result = write(descriptor, mode + offset, count - offset);

		if (result < 0 && errno == EINTR)
			continue;
		if (result <= 0) {
			perror("display-pattern: activate framebuffer mode");
			close(descriptor);
			return -1;
		}
		offset += result;
	}
	close(descriptor);
	printf("display-pattern: activated display mode %.*s", (int)count, mode);
	if (mode[count - 1] != '\n')
		putchar('\n');
	return 0;
}

static uint32_t scale_component(uint8_t value, struct fb_bitfield field)
{
	uint32_t maximum;

	if (!field.length)
		return 0;
	maximum = field.length == 32 ? UINT32_MAX : (1U << field.length) - 1U;
	return ((uint32_t)value * maximum / 255U) << field.offset;
}

static uint32_t make_pixel(const struct fb_var_screeninfo *var,
			   uint8_t red, uint8_t green, uint8_t blue)
{
	uint32_t pixel;

	pixel = scale_component(red, var->red);
	pixel |= scale_component(green, var->green);
	pixel |= scale_component(blue, var->blue);
	if (var->transp.length)
		pixel |= scale_component(255, var->transp);
	return pixel;
}

static void store_pixel(unsigned char *destination, uint32_t pixel,
			unsigned int bytes_per_pixel)
{
	unsigned int byte;

	for (byte = 0; byte < bytes_per_pixel; ++byte)
		destination[byte] = (pixel >> (byte * 8)) & 0xff;
}

static void put_pixel(unsigned char *frame, size_t frame_size,
		      const struct fb_fix_screeninfo *fix,
		      const struct fb_var_screeninfo *var,
		      int x, int y, uint32_t pixel)
{
	unsigned int bytes_per_pixel = (var->bits_per_pixel + 7U) / 8U;
	unsigned char *destination;

	if (x < 0 || y < 0 || (unsigned int)x >= var->xres ||
	    (unsigned int)y >= var->yres)
		return;
	x += var->xoffset;
	y += var->yoffset;
	destination = frame + (size_t)y * fix->line_length
		      + (size_t)x * bytes_per_pixel;
	if (destination + bytes_per_pixel <= frame + frame_size)
		store_pixel(destination, pixel, bytes_per_pixel);
}

/* Integer Bresenham algorithm: no floating point or graphics library. */
static void draw_line(unsigned char *frame, size_t frame_size,
		      const struct fb_fix_screeninfo *fix,
		      const struct fb_var_screeninfo *var,
		      int x0, int y0, int x1, int y1, uint32_t pixel)
{
	int dx = abs(x1 - x0);
	int sx = x0 < x1 ? 1 : -1;
	int dy = -abs(y1 - y0);
	int sy = y0 < y1 ? 1 : -1;
	int error = dx + dy;

	for (;;) {
		put_pixel(frame, frame_size, fix, var, x0, y0, pixel);
		if (x0 == x1 && y0 == y1)
			break;
		if (2 * error >= dy) {
			error += dy;
			x0 += sx;
		}
		if (2 * error <= dx) {
			error += dx;
			y0 += sy;
		}
	}
}

static void circle_points(unsigned char *frame, size_t frame_size,
			  const struct fb_fix_screeninfo *fix,
			  const struct fb_var_screeninfo *var,
			  int cx, int cy, int x, int y, uint32_t pixel)
{
	put_pixel(frame, frame_size, fix, var, cx + x, cy + y, pixel);
	put_pixel(frame, frame_size, fix, var, cx - x, cy + y, pixel);
	put_pixel(frame, frame_size, fix, var, cx + x, cy - y, pixel);
	put_pixel(frame, frame_size, fix, var, cx - x, cy - y, pixel);
	put_pixel(frame, frame_size, fix, var, cx + y, cy + x, pixel);
	put_pixel(frame, frame_size, fix, var, cx - y, cy + x, pixel);
	put_pixel(frame, frame_size, fix, var, cx + y, cy - x, pixel);
	put_pixel(frame, frame_size, fix, var, cx - y, cy - x, pixel);
}

/* Midpoint circle algorithm, also entirely integer based. */
static void draw_circle(unsigned char *frame, size_t frame_size,
			const struct fb_fix_screeninfo *fix,
			const struct fb_var_screeninfo *var,
			int cx, int cy, int radius, uint32_t pixel)
{
	int x = radius;
	int y = 0;
	int error = 1 - radius;

	while (x >= y) {
		circle_points(frame, frame_size, fix, var, cx, cy, x, y, pixel);
		++y;
		if (error < 0)
			error += 2 * y + 1;
		else {
			--x;
			error += 2 * (y - x) + 1;
		}
	}
}

static void draw_shapes(unsigned char *frame, size_t frame_size,
			const struct fb_fix_screeninfo *fix,
			const struct fb_var_screeninfo *var)
{
	int width = var->xres;
	int height = var->yres;
	int cx = width / 2;
	int cy = height / 2;
	int radius = (width < height ? width : height) / 3;
	uint32_t white = make_pixel(var, 255, 255, 255);

	draw_line(frame, frame_size, fix, var, 0, 0, width - 1, 0, white);
	draw_line(frame, frame_size, fix, var, width - 1, 0,
		  width - 1, height - 1, white);
	draw_line(frame, frame_size, fix, var, width - 1, height - 1,
		  0, height - 1, white);
	draw_line(frame, frame_size, fix, var, 0, height - 1, 0, 0, white);
	draw_line(frame, frame_size, fix, var, 0, 0, width - 1, height - 1,
		  make_pixel(var, 255, 0, 0));
	draw_line(frame, frame_size, fix, var, width - 1, 0, 0, height - 1,
		  make_pixel(var, 0, 255, 0));
	draw_line(frame, frame_size, fix, var, 0, cy, width - 1, cy,
		  make_pixel(var, 0, 255, 255));
	draw_line(frame, frame_size, fix, var, cx, 0, cx, height - 1,
		  make_pixel(var, 255, 255, 0));
	draw_circle(frame, frame_size, fix, var, cx, cy, radius,
		    make_pixel(var, 255, 0, 255));
	draw_circle(frame, frame_size, fix, var, cx, cy, radius / 2, white);
}

static uint32_t pattern_pixel(const char *pattern,
			      const struct fb_var_screeninfo *var,
			      unsigned int x, unsigned int y)
{
	static const uint8_t bars[][3] = {
		{255, 255, 255}, {255, 255, 0}, {0, 255, 255}, {0, 255, 0},
		{255, 0, 255}, {255, 0, 0}, {0, 0, 255}, {0, 0, 0}
	};
	uint8_t red = 0;
	uint8_t green = 0;
	uint8_t blue = 0;
	unsigned int index;

	if (!strcmp(pattern, "white"))
		red = green = blue = 255;
	else if (!strcmp(pattern, "red"))
		red = 255;
	else if (!strcmp(pattern, "green"))
		green = 255;
	else if (!strcmp(pattern, "blue"))
		blue = 255;
	else if (!strcmp(pattern, "checker")) {
		if (((x / 20U) + (y / 20U)) & 1U)
			red = green = blue = 255;
	} else if (!strcmp(pattern, "random")) {
		uint32_t value = (x + 1U) * 1103515245U + (y + 1U) * 12345U;

		red = value >> 24;
		green = value >> 16;
		blue = value >> 8;
	} else if (!strcmp(pattern, "shapes")) {
		blue = 32;
	} else {
		index = (x * 8U) / (var->xres ? var->xres : 1U);
		if (index > 7U)
			index = 7U;
		red = bars[index][0];
		green = bars[index][1];
		blue = bars[index][2];
	}
	return make_pixel(var, red, green, blue);
}

static int write_frame(int descriptor, const unsigned char *frame, size_t size)
{
	size_t offset = 0;

	if (lseek(descriptor, 0, SEEK_SET) < 0) {
		perror("display-pattern: lseek");
		return -1;
	}
	while (offset < size) {
		ssize_t result = write(descriptor, frame + offset, size - offset);

		if (result < 0 && errno == EINTR)
			continue;
		if (result <= 0) {
			perror("display-pattern: write");
			return -1;
		}
		offset += (size_t)result;
	}
	return 0;
}

static int map_frame(int descriptor, const unsigned char *frame, size_t size)
{
	void *mapping;

	mapping = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
		       descriptor, 0);
	if (mapping == MAP_FAILED) {
		perror("display-pattern: mmap");
		return -1;
	}
	printf("display-pattern: mmap address %p\n", mapping);
	memcpy(mapping, frame, size);
	if (msync(mapping, size, MS_SYNC) < 0)
		perror("display-pattern: msync");
	if (munmap(mapping, size) < 0) {
		perror("display-pattern: munmap");
		return -1;
	}
	return 0;
}

static void usage(const char *program)
{
	fprintf(stderr,
		"Usage: %s [bars|shapes|checker|random|white|red|green|blue|black] [write|mmap]\n",
		program);
}

int display_pattern_main(int argc, char **argv)
{
	const char *pattern = argc > 1 ? argv[1] : "bars";
	const char *backend = argc > 2 ? argv[2] : "write";
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;
	unsigned char *frame;
	unsigned int bytes_per_pixel;
	unsigned int x;
	unsigned int y;
	size_t frame_size;
	int descriptor;
	int result;

	if (argc > 3 || (strcmp(backend, "write") && strcmp(backend, "mmap"))) {
		usage(argv[0]);
		return 2;
	}

	if (activate_display() < 0)
		return 1;
	descriptor = open("/dev/fb0", O_RDWR);
	if (descriptor < 0) {
		perror("display-pattern: open /dev/fb0");
		return 1;
	}
	if (ioctl(descriptor, FBIOGET_VSCREENINFO, &var) < 0) {
		perror("display-pattern: FBIOGET_VSCREENINFO");
		close(descriptor);
		return 1;
	}
	if (ioctl(descriptor, FBIOGET_FSCREENINFO, &fix) < 0) {
		perror("display-pattern: FBIOGET_FSCREENINFO");
		close(descriptor);
		return 1;
	}

	bytes_per_pixel = (var.bits_per_pixel + 7U) / 8U;
	frame_size = (size_t)fix.line_length * var.yres_virtual;
	if (!bytes_per_pixel || bytes_per_pixel > 4 || !frame_size) {
		fprintf(stderr, "display-pattern: unsupported framebuffer layout\n");
		close(descriptor);
		return 1;
	}

	printf("display-pattern: %ux%u, virtual %ux%u, %u bpp, stride %u, %lu bytes\n",
	       var.xres, var.yres, var.xres_virtual, var.yres_virtual,
	       var.bits_per_pixel, fix.line_length, (unsigned long)frame_size);
	printf("display-pattern: R%u/%u G%u/%u B%u/%u A%u/%u, visual %u, backend %s\n",
	       var.red.offset, var.red.length, var.green.offset, var.green.length,
	       var.blue.offset, var.blue.length, var.transp.offset,
	       var.transp.length, fix.visual, backend);

	frame = calloc(1, frame_size);
	if (!frame) {
		perror("display-pattern: calloc");
		close(descriptor);
		return 1;
	}
	for (y = 0; y < var.yres_virtual; ++y) {
		for (x = 0; x < var.xres_virtual; ++x) {
			unsigned char *destination;
			uint32_t pixel = pattern_pixel(pattern, &var, x, y);

			destination = frame + (size_t)y * fix.line_length
				      + (size_t)x * bytes_per_pixel;
			if (destination + bytes_per_pixel <= frame + frame_size)
				store_pixel(destination, pixel, bytes_per_pixel);
		}
	}
	if (!strcmp(pattern, "shapes"))
		draw_shapes(frame, frame_size, &fix, &var);

	result = !strcmp(backend, "mmap")
		? map_frame(descriptor, frame, frame_size)
		: write_frame(descriptor, frame, frame_size);
	free(frame);
	close(descriptor);
	if (!result)
		printf("display-pattern: displayed %s using %s\n", pattern, backend);
	return result ? 1 : 0;
}

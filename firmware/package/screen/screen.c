#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/magic.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "font5x7.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define BITS_PER_LONG (sizeof(unsigned long) * 8U)
#define BITS_TO_LONGS(n) (((n) + BITS_PER_LONG - 1U) / BITS_PER_LONG)
#define TEST_BIT(n, a) (((a)[(n) / BITS_PER_LONG] >> ((n) % BITS_PER_LONG)) & 1UL)
#define MAX_COLS 80
#define MAX_ROWS 32
#define CELL_W 8
#define CELL_H 8
#define KEY_SMALL_GLYPH_W 6
#define KEY_SMALL_GLYPH_H 8
#define KEY_SMALL_ADVANCE 7
#define KEY_LARGE_GLYPH_W 10
#define KEY_LARGE_GLYPH_H 14
#define KEY_LARGE_ADVANCE 11
#define WAIT_SECONDS 60

static const char pid_path[] = "/run/screen.pid";
static volatile sig_atomic_t stop_requested;

struct framebuffer {
	int fd;
	unsigned char *memory;
	size_t length;
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;
	uint32_t palette[8];
};

struct cell {
	unsigned char character;
	unsigned char attributes;
};

struct terminal {
	struct framebuffer *fb;
	struct cell cells[MAX_ROWS][MAX_COLS];
	int columns;
	int rows;
	int cursor_x;
	int cursor_y;
	int saved_x;
	int saved_y;
	int foreground;
	int background;
	int bold;
	int parser_state;
	int parameters[8];
	int parameter_count;
	bool have_parameter;
	bool cursor_drawn;
	int console_height;
};

struct touchscreen {
	int fd;
	char path[64];
	char name[128];
	struct input_absinfo abs_x;
	struct input_absinfo abs_y;
	int raw_x;
	int raw_y;
	bool pressed;
	bool tap_pending;
	bool swap_xy;
	bool invert_x;
	bool invert_y;
};

enum key_action {
	ACTION_CHARACTER,
	ACTION_SHIFT,
	ACTION_SPACE,
	ACTION_ENTER,
	ACTION_BACKSPACE,
	ACTION_TAB,
	ACTION_CTRL_C
};

struct screen_key {
	const char *normal_label;
	const char *shift_label;
	unsigned char normal_character;
	unsigned char shift_character;
	unsigned char row;
	unsigned char units;
	enum key_action action;
	int x;
	int y;
	int width;
	int height;
};

#define CHAR_KEY(nl, sl, nc, sc, r) \
	{ nl, sl, (unsigned char)(nc), (unsigned char)(sc), r, 1, \
	  ACTION_CHARACTER, 0, 0, 0, 0 }

static struct screen_key keys[] = {
	CHAR_KEY("1","!",'1','!',0), CHAR_KEY("2","@",'2','@',0),
	CHAR_KEY("3","#",'3','#',0), CHAR_KEY("4","$",'4','$',0),
	CHAR_KEY("5","%",'5','%',0), CHAR_KEY("6","^",'6','^',0),
	CHAR_KEY("7","&",'7','&',0), CHAR_KEY("8","*",'8','*',0),
	CHAR_KEY("9","(",'9','(',0), CHAR_KEY("0",")",'0',')',0),
	CHAR_KEY("-","_",'-','_',0), CHAR_KEY("=","+",'=','+',0),
	CHAR_KEY("q","Q",'q','Q',1), CHAR_KEY("w","W",'w','W',1),
	CHAR_KEY("e","E",'e','E',1), CHAR_KEY("r","R",'r','R',1),
	CHAR_KEY("t","T",'t','T',1), CHAR_KEY("y","Y",'y','Y',1),
	CHAR_KEY("u","U",'u','U',1), CHAR_KEY("i","I",'i','I',1),
	CHAR_KEY("o","O",'o','O',1), CHAR_KEY("p","P",'p','P',1),
	CHAR_KEY("[","{",'[','{',1), CHAR_KEY("]","}",']','}',1),
	CHAR_KEY("a","A",'a','A',2), CHAR_KEY("s","S",'s','S',2),
	CHAR_KEY("d","D",'d','D',2), CHAR_KEY("f","F",'f','F',2),
	CHAR_KEY("g","G",'g','G',2), CHAR_KEY("h","H",'h','H',2),
	CHAR_KEY("j","J",'j','J',2), CHAR_KEY("k","K",'k','K',2),
	CHAR_KEY("l","L",'l','L',2), CHAR_KEY(";",":",';',':',2),
	CHAR_KEY("'","\"",'\'','\"',2), CHAR_KEY("`","~",'`','~',2),
	{"Shift","Shift",0,0,3,2,ACTION_SHIFT,0,0,0,0},
	CHAR_KEY("z","Z",'z','Z',3), CHAR_KEY("x","X",'x','X',3),
	CHAR_KEY("c","C",'c','C',3), CHAR_KEY("v","V",'v','V',3),
	CHAR_KEY("b","B",'b','B',3), CHAR_KEY("n","N",'n','N',3),
	CHAR_KEY("m","M",'m','M',3), CHAR_KEY(",","<",',','<',3),
	CHAR_KEY(".",">",'.','>',3), CHAR_KEY("/","?",'/','?',3),
	CHAR_KEY("\\","|",'\\','|',3),
	{"Tab","Tab",0,0,4,2,ACTION_TAB,0,0,0,0},
	{"C-C","C-C",0,0,4,2,ACTION_CTRL_C,0,0,0,0},
	{"Space","Space",0,0,4,5,ACTION_SPACE,0,0,0,0},
	{"BS","BS",0,0,4,2,ACTION_BACKSPACE,0,0,0,0},
	{"Ent","Ent",0,0,4,3,ACTION_ENTER,0,0,0,0}
};

static bool shift_active;

static void signal_handler(int signal_number)
{
	(void)signal_number;
	stop_requested = 1;
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

static uint32_t channel_value(unsigned int value, const struct fb_bitfield *field)
{
	uint32_t maximum;

	if (!field->length)
		return 0;
	maximum = (1U << field->length) - 1U;
	return ((value * maximum + 127U) / 255U) << field->offset;
}

static uint32_t make_color(const struct framebuffer *fb,
			   unsigned int red, unsigned int green, unsigned int blue)
{
	return channel_value(red, &fb->var.red) |
	       channel_value(green, &fb->var.green) |
	       channel_value(blue, &fb->var.blue);
}

static void put_pixel(struct framebuffer *fb, int x, int y, uint32_t color)
{
	unsigned char *pixel;
	unsigned int bytes;

	if (x < 0 || y < 0 || x >= (int)fb->var.xres || y >= (int)fb->var.yres)
		return;
	bytes = (fb->var.bits_per_pixel + 7U) / 8U;
	pixel = fb->memory + (size_t)y * fb->fix.line_length + (size_t)x * bytes;
	if (bytes == 2) {
		uint16_t value = (uint16_t)color;
		memcpy(pixel, &value, sizeof(value));
	} else if (bytes == 3) {
		pixel[0] = (unsigned char)color;
		pixel[1] = (unsigned char)(color >> 8);
		pixel[2] = (unsigned char)(color >> 16);
	} else if (bytes == 4) {
		memcpy(pixel, &color, sizeof(color));
	}
}

static void fill_rectangle(struct framebuffer *fb, int x, int y, int width,
			   int height, uint32_t color)
{
	int px;
	int py;

	for (py = y; py < y + height; ++py)
		for (px = x; px < x + width; ++px)
			put_pixel(fb, px, py, color);
}

static void draw_glyph(struct framebuffer *fb, int x, int y,
		       unsigned char character, uint32_t foreground,
		       uint32_t background)
{
	const unsigned char *glyph;
	int column;
	int row;

	if (character < 32 || character > 127)
		character = '?';
	glyph = font5x7[character - 32];
	fill_rectangle(fb, x, y, CELL_W, CELL_H, background);
	for (column = 0; column < 5; ++column)
		for (row = 0; row < 7; ++row)
			if (glyph[column] & (1U << row))
				put_pixel(fb, x + column + 1, y + row, foreground);
}

static int activate_framebuffer_mode(void)
{
	char mode[80];
	ssize_t count;
	int source;
	int destination;

	source = open("/sys/class/graphics/fb0/mode", O_RDONLY);
	if (source < 0)
		return -1;
	count = read(source, mode, sizeof(mode));
	close(source);
	if (count == 0) {
		source = open("/sys/class/graphics/fb0/modes", O_RDONLY);
		if (source < 0)
			return -1;
		count = read(source, mode, sizeof(mode));
		close(source);
	}
	if (count <= 0)
		return -1;
	destination = open("/sys/class/graphics/fb0/mode", O_WRONLY);
	if (destination < 0)
		return -1;
	if (write(destination, mode, (size_t)count) != count) {
		close(destination);
		return -1;
	}
	close(destination);
	return 0;
}

static int wait_for_framebuffer(struct framebuffer *fb)
{
	int attempts;

	puts("screen: waiting for framebuffer /dev/fb0");
	fflush(stdout);
	for (attempts = 0; attempts < WAIT_SECONDS * 10 && !stop_requested; ++attempts) {
		fb->fd = open("/dev/fb0", O_RDWR);
		if (fb->fd >= 0)
			break;
		sleep_milliseconds(100);
	}
	if (fb->fd < 0) {
		fprintf(stderr, "screen: failed: /dev/fb0 did not appear within %d seconds\n",
			WAIT_SECONDS);
		return -1;
	}
	if (activate_framebuffer_mode() < 0)
		fprintf(stderr, "screen: warning: could not select framebuffer mode: %s\n",
			strerror(errno));
	if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &fb->fix) < 0 ||
	    ioctl(fb->fd, FBIOGET_VSCREENINFO, &fb->var) < 0) {
		perror("screen: failed to query framebuffer");
		close(fb->fd);
		return -1;
	}
	if (fb->var.bits_per_pixel != 16 && fb->var.bits_per_pixel != 24 &&
	    fb->var.bits_per_pixel != 32) {
		fprintf(stderr, "screen: unsupported framebuffer depth: %u bpp\n",
			fb->var.bits_per_pixel);
		close(fb->fd);
		return -1;
	}
	fb->length = fb->fix.smem_len;
	fb->memory = mmap(NULL, fb->length, PROT_READ | PROT_WRITE, MAP_SHARED,
			  fb->fd, 0);
	if (fb->memory == MAP_FAILED) {
		fb->memory = NULL;
		perror("screen: failed to map framebuffer");
		close(fb->fd);
		return -1;
	}
	fb->palette[0] = make_color(fb, 0, 0, 0);
	fb->palette[1] = make_color(fb, 205, 49, 49);
	fb->palette[2] = make_color(fb, 13, 188, 121);
	fb->palette[3] = make_color(fb, 229, 229, 16);
	fb->palette[4] = make_color(fb, 36, 114, 200);
	fb->palette[5] = make_color(fb, 188, 63, 188);
	fb->palette[6] = make_color(fb, 17, 168, 205);
	fb->palette[7] = make_color(fb, 229, 229, 229);
	printf("screen: framebuffer detected: %ux%u, %u bpp\n", fb->var.xres,
	       fb->var.yres, fb->var.bits_per_pixel);
	fflush(stdout);
	return 0;
}

static void close_framebuffer(struct framebuffer *fb)
{
	if (fb->memory)
		munmap(fb->memory, fb->length);
	if (fb->fd >= 0)
		close(fb->fd);
}

static bool event_device_is_stmpe(int descriptor, char *name, size_t name_size,
				  struct input_absinfo *abs_x,
				  struct input_absinfo *abs_y)
{
	unsigned long event_bits[BITS_TO_LONGS(EV_MAX + 1)] = { 0 };
	unsigned long absolute_bits[BITS_TO_LONGS(ABS_MAX + 1)] = { 0 };
	unsigned long key_bits[BITS_TO_LONGS(KEY_MAX + 1)] = { 0 };
	struct input_id identity;

	if (ioctl(descriptor, EVIOCGNAME(name_size), name) < 0 ||
	    ioctl(descriptor, EVIOCGID, &identity) < 0 ||
	    ioctl(descriptor, EVIOCGBIT(0, sizeof(event_bits)), event_bits) < 0)
		return false;
	if (strcmp(name, "stmpe-ts") != 0 || identity.bustype != BUS_I2C ||
	    !TEST_BIT(EV_ABS, event_bits) || !TEST_BIT(EV_KEY, event_bits))
		return false;
	if (ioctl(descriptor, EVIOCGBIT(EV_ABS, sizeof(absolute_bits)), absolute_bits) < 0 ||
	    ioctl(descriptor, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0 ||
	    !TEST_BIT(ABS_X, absolute_bits) || !TEST_BIT(ABS_Y, absolute_bits) ||
	    !TEST_BIT(BTN_TOUCH, key_bits))
		return false;
	return ioctl(descriptor, EVIOCGABS(ABS_X), abs_x) == 0 &&
	       ioctl(descriptor, EVIOCGABS(ABS_Y), abs_y) == 0;
}

static bool environment_flag(const char *name, bool default_value)
{
	const char *value = getenv(name);

	if (!value || !*value)
		return default_value;
	return strcmp(value, "0") != 0 && strcmp(value, "no") != 0 &&
	       strcmp(value, "false") != 0;
}

static int find_touchscreen(struct touchscreen *touch)
{
	DIR *directory;
	struct dirent *entry;

	directory = opendir("/dev/input");
	if (!directory)
		return -1;
	while ((entry = readdir(directory))) {
		int descriptor;

		if (strncmp(entry->d_name, "event", 5) != 0)
			continue;
		if (snprintf(touch->path, sizeof(touch->path), "/dev/input/%s",
			     entry->d_name) >= (int)sizeof(touch->path))
			continue;
		descriptor = open(touch->path, O_RDONLY | O_NONBLOCK);
		if (descriptor < 0)
			continue;
		if (event_device_is_stmpe(descriptor, touch->name, sizeof(touch->name),
					  &touch->abs_x, &touch->abs_y)) {
			touch->fd = descriptor;
			closedir(directory);
			return 0;
		}
		close(descriptor);
	}
	closedir(directory);
	return -1;
}

static int wait_for_touchscreen(struct touchscreen *touch)
{
	int attempts;

	puts("screen: waiting for STMPE811 input device");
	fflush(stdout);
	for (attempts = 0; attempts < WAIT_SECONDS * 10 && !stop_requested; ++attempts) {
		if (find_touchscreen(touch) == 0)
			break;
		sleep_milliseconds(100);
	}
	if (touch->fd < 0) {
		fprintf(stderr,
			"screen: failed: STMPE811 evdev device did not appear within %d seconds\n",
			WAIT_SECONDS);
		return -1;
	}
	touch->swap_xy = environment_flag("SCREEN_SWAP_XY", false);
	touch->invert_x = environment_flag("SCREEN_INVERT_X", true);
	touch->invert_y = environment_flag("SCREEN_INVERT_Y", false);
	printf("screen: touchscreen detected: %s (%s), X=%d..%d Y=%d..%d\n",
	       touch->name, touch->path, touch->abs_x.minimum, touch->abs_x.maximum,
	       touch->abs_y.minimum, touch->abs_y.maximum);
	fflush(stdout);
	return 0;
}

static void render_cell(struct terminal *terminal, int x, int y, bool cursor)
{
	struct cell *cell = &terminal->cells[y][x];
	int foreground = cell->attributes & 7;
	int background = (cell->attributes >> 3) & 7;

	if (cursor) {
		int temporary = foreground;
		foreground = background;
		background = temporary;
		if (foreground == background)
			foreground = 7;
	}
	draw_glyph(terminal->fb, x * CELL_W, y * CELL_H, cell->character,
		   terminal->fb->palette[foreground], terminal->fb->palette[background]);
}

static void hide_cursor(struct terminal *terminal)
{
	if (terminal->cursor_drawn) {
		render_cell(terminal, terminal->cursor_x, terminal->cursor_y, false);
		terminal->cursor_drawn = false;
	}
}

static void show_cursor(struct terminal *terminal)
{
	render_cell(terminal, terminal->cursor_x, terminal->cursor_y, true);
	terminal->cursor_drawn = true;
}

static void clear_cell(struct terminal *terminal, int x, int y)
{
	terminal->cells[y][x].character = ' ';
	terminal->cells[y][x].attributes = (unsigned char)(terminal->foreground |
						       (terminal->background << 3));
	render_cell(terminal, x, y, false);
}

static void scroll_terminal(struct terminal *terminal)
{
	int x;
	int y;

	memmove(terminal->cells[0], terminal->cells[1],
		(size_t)(terminal->rows - 1) * sizeof(terminal->cells[0]));
	for (x = 0; x < terminal->columns; ++x) {
		terminal->cells[terminal->rows - 1][x].character = ' ';
		terminal->cells[terminal->rows - 1][x].attributes =
			(unsigned char)(terminal->foreground | (terminal->background << 3));
	}
	for (y = 0; y < terminal->rows; ++y)
		for (x = 0; x < terminal->columns; ++x)
			render_cell(terminal, x, y, false);
}

static void line_feed(struct terminal *terminal)
{
	++terminal->cursor_y;
	if (terminal->cursor_y >= terminal->rows) {
		scroll_terminal(terminal);
		terminal->cursor_y = terminal->rows - 1;
	}
}

static void put_character(struct terminal *terminal, unsigned char character)
{
	struct cell *cell = &terminal->cells[terminal->cursor_y][terminal->cursor_x];

	cell->character = character;
	cell->attributes = (unsigned char)(terminal->foreground |
					   (terminal->background << 3));
	render_cell(terminal, terminal->cursor_x, terminal->cursor_y, false);
	++terminal->cursor_x;
	if (terminal->cursor_x >= terminal->columns) {
		terminal->cursor_x = 0;
		line_feed(terminal);
	}
}

static int ansi_parameter(const struct terminal *terminal, int index, int fallback)
{
	if (index >= terminal->parameter_count || terminal->parameters[index] < 0)
		return fallback;
	return terminal->parameters[index];
}

static void erase_display(struct terminal *terminal, int mode)
{
	int x;
	int y;

	for (y = 0; y < terminal->rows; ++y) {
		for (x = 0; x < terminal->columns; ++x) {
			if ((mode == 0 && (y < terminal->cursor_y ||
			    (y == terminal->cursor_y && x < terminal->cursor_x))) ||
			    (mode == 1 && (y > terminal->cursor_y ||
			    (y == terminal->cursor_y && x > terminal->cursor_x))))
				continue;
			clear_cell(terminal, x, y);
		}
	}
	if (mode == 2)
		terminal->cursor_x = terminal->cursor_y = 0;
}

static void erase_line(struct terminal *terminal, int mode)
{
	int x;

	for (x = 0; x < terminal->columns; ++x) {
		if ((mode == 0 && x < terminal->cursor_x) ||
		    (mode == 1 && x > terminal->cursor_x))
			continue;
		clear_cell(terminal, x, terminal->cursor_y);
	}
}

static void apply_sgr(struct terminal *terminal)
{
	int index;

	if (!terminal->parameter_count) {
		terminal->foreground = 7;
		terminal->background = 0;
		terminal->bold = 0;
		return;
	}
	for (index = 0; index < terminal->parameter_count; ++index) {
		int value = ansi_parameter(terminal, index, 0);

		if (value == 0) {
			terminal->foreground = 7;
			terminal->background = 0;
			terminal->bold = 0;
		} else if (value == 1) {
			terminal->bold = 1;
		} else if (value >= 30 && value <= 37) {
			terminal->foreground = value - 30;
		} else if (value == 39) {
			terminal->foreground = 7;
		} else if (value >= 40 && value <= 47) {
			terminal->background = value - 40;
		} else if (value == 49) {
			terminal->background = 0;
		}
	}
}

static void execute_csi(struct terminal *terminal, unsigned char command)
{
	int amount = ansi_parameter(terminal, 0, 1);

	switch (command) {
	case 'A': terminal->cursor_y -= amount; break;
	case 'B': terminal->cursor_y += amount; break;
	case 'C': terminal->cursor_x += amount; break;
	case 'D': terminal->cursor_x -= amount; break;
	case 'G': terminal->cursor_x = amount - 1; break;
	case 'd': terminal->cursor_y = amount - 1; break;
	case 'H':
	case 'f':
		terminal->cursor_y = ansi_parameter(terminal, 0, 1) - 1;
		terminal->cursor_x = ansi_parameter(terminal, 1, 1) - 1;
		break;
	case 'J': erase_display(terminal, ansi_parameter(terminal, 0, 0)); break;
	case 'K': erase_line(terminal, ansi_parameter(terminal, 0, 0)); break;
	case 'm': apply_sgr(terminal); break;
	case 's': terminal->saved_x = terminal->cursor_x; terminal->saved_y = terminal->cursor_y; break;
	case 'u': terminal->cursor_x = terminal->saved_x; terminal->cursor_y = terminal->saved_y; break;
	default: break;
	}
	if (terminal->cursor_x < 0)
		terminal->cursor_x = 0;
	if (terminal->cursor_y < 0)
		terminal->cursor_y = 0;
	if (terminal->cursor_x >= terminal->columns)
		terminal->cursor_x = terminal->columns - 1;
	if (terminal->cursor_y >= terminal->rows)
		terminal->cursor_y = terminal->rows - 1;
}

static void terminal_output(struct terminal *terminal, const unsigned char *data,
			    size_t length)
{
	size_t index;

	hide_cursor(terminal);
	for (index = 0; index < length; ++index) {
		unsigned char character = data[index];

		if (terminal->parser_state == 1) {
			if (character == '[') {
				terminal->parser_state = 2;
				terminal->parameter_count = 0;
				terminal->have_parameter = false;
			} else {
				if (character == '7') {
					terminal->saved_x = terminal->cursor_x;
					terminal->saved_y = terminal->cursor_y;
				} else if (character == '8') {
					terminal->cursor_x = terminal->saved_x;
					terminal->cursor_y = terminal->saved_y;
				}
				terminal->parser_state = 0;
			}
			continue;
		}
		if (terminal->parser_state == 2) {
			if (character >= '0' && character <= '9') {
				if (!terminal->have_parameter) {
					if (terminal->parameter_count < (int)ARRAY_SIZE(terminal->parameters))
						terminal->parameters[terminal->parameter_count++] = 0;
					terminal->have_parameter = true;
				}
				if (terminal->parameter_count)
					terminal->parameters[terminal->parameter_count - 1] =
						terminal->parameters[terminal->parameter_count - 1] * 10 +
						(character - '0');
				continue;
			}
			if (character == ';') {
				if (!terminal->have_parameter &&
				    terminal->parameter_count < (int)ARRAY_SIZE(terminal->parameters))
					terminal->parameters[terminal->parameter_count++] = -1;
				terminal->have_parameter = false;
				continue;
			}
			if (character == '?' || character == '>')
				continue;
			if (!terminal->have_parameter && terminal->parameter_count &&
			    terminal->parameter_count < (int)ARRAY_SIZE(terminal->parameters))
				terminal->parameters[terminal->parameter_count++] = -1;
			execute_csi(terminal, character);
			terminal->parser_state = 0;
			continue;
		}
		if (character == 0x1b) {
			terminal->parser_state = 1;
		} else if (character == '\r') {
			terminal->cursor_x = 0;
		} else if (character == '\n') {
			line_feed(terminal);
		} else if (character == '\b' || character == 0x7f) {
			if (terminal->cursor_x > 0)
				--terminal->cursor_x;
		} else if (character == '\t') {
			do {
				put_character(terminal, ' ');
			} while (terminal->cursor_x % 4);
		} else if (character >= 32 && character != 0x7f) {
			put_character(terminal, character);
		}
	}
	show_cursor(terminal);
}

static void terminal_initialize(struct terminal *terminal, struct framebuffer *fb)
{
	int x;
	int y;

	memset(terminal, 0, sizeof(*terminal));
	terminal->fb = fb;
	terminal->console_height = ((int)fb->var.yres * 3 / 5 / CELL_H) * CELL_H;
	terminal->columns = (int)fb->var.xres / CELL_W;
	terminal->rows = terminal->console_height / CELL_H;
	if (terminal->columns > MAX_COLS)
		terminal->columns = MAX_COLS;
	if (terminal->rows > MAX_ROWS)
		terminal->rows = MAX_ROWS;
	terminal->foreground = 7;
	terminal->background = 0;
	fill_rectangle(fb, 0, 0, (int)fb->var.xres, (int)fb->var.yres,
		       fb->palette[0]);
	for (y = 0; y < terminal->rows; ++y)
		for (x = 0; x < terminal->columns; ++x)
			clear_cell(terminal, x, y);
	show_cursor(terminal);
}

static void draw_centered_text(struct framebuffer *fb, const char *text,
			       int x, int y, int width, int height,
			       uint32_t foreground, uint32_t background)
{
	size_t text_length = strlen(text);

	if (!text_length)
		return;

	bool large = text_length == 1;
	int glyph_width = large ? KEY_LARGE_GLYPH_W : KEY_SMALL_GLYPH_W;
	int glyph_height = large ? KEY_LARGE_GLYPH_H : KEY_SMALL_GLYPH_H;
	int advance = large ? KEY_LARGE_ADVANCE : KEY_SMALL_ADVANCE;
	int text_width = (int)(text_length - 1) * advance + glyph_width;
	int position_x = x + (width - text_width) / 2;
	int position_y = y + (height - glyph_height) / 2;

	while (*text) {
		const unsigned char *glyph;
		unsigned char character = (unsigned char)*text;
		int column;
		int row;

		if (character < 32 || character > 127)
			character = '?';
		glyph = font5x7[character - 32];
		for (column = 0; column < glyph_width; ++column) {
			int source_column = column * 5 / glyph_width;

			for (row = 0; row < glyph_height; ++row) {
				int source_row = row * 7 / glyph_height;
				uint32_t color = glyph[source_column] & (1U << source_row) ?
					foreground : background;

				put_pixel(fb, position_x + column, position_y + row, color);
			}
		}
		position_x += advance;
		++text;
	}
}

static void render_keyboard(struct framebuffer *fb, int keyboard_top)
{
	uint32_t border = make_color(fb, 95, 95, 95);
	uint32_t normal = make_color(fb, 45, 45, 48);
	uint32_t active = make_color(fb, 42, 105, 160);
	int keyboard_height = (int)fb->var.yres - keyboard_top;
	int row;
	size_t index;

	fill_rectangle(fb, 0, keyboard_top, (int)fb->var.xres, keyboard_height,
		       fb->palette[0]);
	for (row = 0; row < 5; ++row) {
		int total_units = 0;
		int used_units = 0;
		int row_y = keyboard_top + row * keyboard_height / 5;
		int row_bottom = keyboard_top + (row + 1) * keyboard_height / 5;

		for (index = 0; index < ARRAY_SIZE(keys); ++index)
			if (keys[index].row == row)
				total_units += keys[index].units;
		for (index = 0; index < ARRAY_SIZE(keys); ++index) {
			struct screen_key *key = &keys[index];
			int right;
			uint32_t background;
			const char *label;

			if (key->row != row)
				continue;
			key->x = used_units * (int)fb->var.xres / total_units;
			used_units += key->units;
			right = used_units * (int)fb->var.xres / total_units;
			key->y = row_y;
			key->width = right - key->x;
			key->height = row_bottom - row_y;
			background = key->action == ACTION_SHIFT && shift_active ? active : normal;
			fill_rectangle(fb, key->x, key->y, key->width, key->height, border);
			fill_rectangle(fb, key->x + 1, key->y + 1, key->width - 2,
				       key->height - 2, background);
			label = shift_active ? key->shift_label : key->normal_label;
			draw_centered_text(fb, label, key->x + 1, key->y + 1,
					   key->width - 2, key->height - 2,
					   fb->palette[7], background);
		}
	}
}

static int scale_coordinate(int value, const struct input_absinfo *range, int extent)
{
	long long numerator;
	int denominator = range->maximum - range->minimum;

	if (denominator <= 0 || extent <= 1)
		return 0;
	if (value < range->minimum)
		value = range->minimum;
	if (value > range->maximum)
		value = range->maximum;
	numerator = (long long)(value - range->minimum) * (extent - 1);
	return (int)(numerator / denominator);
}

static void transformed_touch(const struct touchscreen *touch,
			      const struct framebuffer *fb, int *x, int *y)
{
	if (touch->swap_xy) {
		*x = scale_coordinate(touch->raw_y, &touch->abs_y, (int)fb->var.xres);
		*y = scale_coordinate(touch->raw_x, &touch->abs_x, (int)fb->var.yres);
	} else {
		*x = scale_coordinate(touch->raw_x, &touch->abs_x, (int)fb->var.xres);
		*y = scale_coordinate(touch->raw_y, &touch->abs_y, (int)fb->var.yres);
	}
	if (touch->invert_x)
		*x = (int)fb->var.xres - 1 - *x;
	if (touch->invert_y)
		*y = (int)fb->var.yres - 1 - *y;
}

static int read_touch_events(struct touchscreen *touch)
{
	struct input_event events[16];
	ssize_t count;
	size_t index;

	count = read(touch->fd, events, sizeof(events));
	if (count < 0)
		return errno == EAGAIN || errno == EINTR ? 0 : -1;
	for (index = 0; index < (size_t)count / sizeof(events[0]); ++index) {
		struct input_event *event = &events[index];

		if (event->type == EV_ABS && event->code == ABS_X)
			touch->raw_x = event->value;
		else if (event->type == EV_ABS && event->code == ABS_Y)
			touch->raw_y = event->value;
		else if (event->type == EV_KEY && event->code == BTN_TOUCH) {
			if (event->value && !touch->pressed)
				touch->tap_pending = true;
			touch->pressed = event->value != 0;
		}
	}
	return 0;
}

static int ensure_devpts(void)
{
	struct statfs filesystem;

	if (mkdir("/dev/pts", 0755) < 0 && errno != EEXIST) {
		perror("screen: create /dev/pts");
		return -1;
	}
	if (statfs("/dev/pts", &filesystem) == 0 &&
	    (unsigned long)filesystem.f_type == (unsigned long)DEVPTS_SUPER_MAGIC)
		return 0;
	if (mount("devpts", "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC,
		  "mode=620,ptmxmode=666") < 0) {
		perror("screen: mount devpts");
		return -1;
	}
	return 0;
}

static int open_pty_pair(int *master, int *slave, int columns, int rows)
{
	unsigned int number;
	int unlocked = 0;
	char path[64];
	struct winsize window;
	struct termios settings;
	int flags;

	if (ensure_devpts() < 0)
		return -1;
	*master = open("/dev/ptmx", O_RDWR | O_NOCTTY);
	if (*master < 0 || ioctl(*master, TIOCSPTLCK, &unlocked) < 0 ||
	    ioctl(*master, TIOCGPTN, &number) < 0) {
		perror("screen: allocate PTY");
		if (*master >= 0)
			close(*master);
		return -1;
	}
	snprintf(path, sizeof(path), "/dev/pts/%u", number);
	*slave = open(path, O_RDWR | O_NOCTTY);
	if (*slave < 0) {
		perror("screen: open PTY slave");
		close(*master);
		return -1;
	}
	if (tcgetattr(*slave, &settings) == 0) {
		settings.c_iflag |= ICRNL;
		settings.c_oflag |= OPOST | ONLCR;
		settings.c_lflag |= ECHO | ECHOE | ECHOK | ICANON | ISIG;
		tcsetattr(*slave, TCSANOW, &settings);
	}
	memset(&window, 0, sizeof(window));
	window.ws_col = (unsigned short)columns;
	window.ws_row = (unsigned short)rows;
	ioctl(*slave, TIOCSWINSZ, &window);
	flags = fcntl(*master, F_GETFL);
	if (flags >= 0)
		fcntl(*master, F_SETFL, flags | O_NONBLOCK);
	return 0;
}

static pid_t start_shell(int slave, int master)
{
	pid_t pid = vfork();

	if (pid == 0) {
		char *const arguments[] = { (char *)"sh", (char *)"-i", NULL };

		setsid();
		ioctl(slave, TIOCSCTTY, 0);
		dup2(slave, STDIN_FILENO);
		dup2(slave, STDOUT_FILENO);
		dup2(slave, STDERR_FILENO);
		if (slave > STDERR_FILENO)
			close(slave);
		close(master);
		execv("/bin/sh", arguments);
		_exit(127);
	}
	return pid;
}

static int write_pty(int descriptor, const void *data, size_t length)
{
	const unsigned char *bytes = data;

	while (length) {
		ssize_t count = write(descriptor, bytes, length);

		if (count > 0) {
			bytes += count;
			length -= (size_t)count;
		} else if (count < 0 && errno == EINTR) {
			continue;
		} else if (count < 0 && errno == EAGAIN) {
			sleep_milliseconds(10);
		} else {
			return -1;
		}
	}
	return 0;
}

static void handle_key_tap(struct framebuffer *fb, int master, int x, int y,
			   int keyboard_top)
{
	size_t index;

	if (y < keyboard_top)
		return;
	for (index = 0; index < ARRAY_SIZE(keys); ++index) {
		struct screen_key *key = &keys[index];
		unsigned char character;

		if (x < key->x || x >= key->x + key->width ||
		    y < key->y || y >= key->y + key->height)
			continue;
		switch (key->action) {
		case ACTION_SHIFT:
			shift_active = !shift_active;
			render_keyboard(fb, keyboard_top);
			return;
		case ACTION_SPACE: character = ' '; break;
		case ACTION_ENTER: character = '\n'; break;
		case ACTION_BACKSPACE: character = 0x7f; break;
		case ACTION_TAB: character = '\t'; break;
		case ACTION_CTRL_C: character = 0x03; break;
		case ACTION_CHARACTER:
		default:
			character = shift_active ? key->shift_character : key->normal_character;
			break;
		}
		write_pty(master, &character, 1);
		return;
	}
}

static int read_pid_file(void)
{
	char buffer[32];
	char *end;
	long value;
	int descriptor;
	ssize_t count;

	descriptor = open(pid_path, O_RDONLY);
	if (descriptor < 0)
		return -1;
	count = read(descriptor, buffer, sizeof(buffer) - 1);
	close(descriptor);
	if (count <= 0)
		return -1;
	buffer[count] = '\0';
	errno = 0;
	value = strtol(buffer, &end, 10);
	if (errno || end == buffer || value <= 1 || value > 0x7fffffffL)
		return -1;
	return (int)value;
}

static bool process_running(pid_t pid)
{
	return pid > 1 && (kill(pid, 0) == 0 || errno == EPERM);
}

static int write_pid_file(pid_t pid)
{
	char buffer[32];
	int descriptor;
	int length;

	length = snprintf(buffer, sizeof(buffer), "%ld\n", (long)pid);
	descriptor = open(pid_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (descriptor < 0)
		return -1;
	if (write(descriptor, buffer, (size_t)length) != length) {
		close(descriptor);
		return -1;
	}
	return close(descriptor);
}

static int claim_screen(bool service_worker)
{
	pid_t existing = (pid_t)read_pid_file();

	if (process_running(existing) && existing != getpid()) {
		fprintf(stderr, "screen: already running (PID %ld)\n", (long)existing);
		return -1;
	}
	if (!service_worker && write_pid_file(getpid()) < 0) {
		perror("screen: write PID file");
		return -1;
	}
	if (service_worker && existing != getpid() && write_pid_file(getpid()) < 0) {
		perror("screen: write PID file");
		return -1;
	}
	return 0;
}

static int run_console(bool service_worker)
{
	struct framebuffer fb;
	struct touchscreen touch;
	struct terminal terminal;
	pid_t shell_pid = -1;
	int master = -1;
	int slave = -1;
	int result = 1;

	memset(&fb, 0, sizeof(fb));
	memset(&touch, 0, sizeof(touch));
	fb.fd = -1;
	touch.fd = -1;
	if (claim_screen(service_worker) < 0)
		return 1;
	if (wait_for_framebuffer(&fb) < 0 || wait_for_touchscreen(&touch) < 0)
		goto cleanup;
	terminal_initialize(&terminal, &fb);
	render_keyboard(&fb, terminal.console_height);
	if (open_pty_pair(&master, &slave, terminal.columns, terminal.rows) < 0)
		goto cleanup;
	shell_pid = start_shell(slave, master);
	if (shell_pid < 0) {
		perror("screen: start shell");
		goto cleanup;
	}
	close(slave);
	slave = -1;
	printf("screen: shell started: /bin/sh -i (PID %ld)\n", (long)shell_pid);
	fflush(stdout);
	result = 0;
	while (!stop_requested) {
		struct pollfd descriptors[2];
		unsigned char output[256];
		int status;
		int ready;

		descriptors[0].fd = master;
		descriptors[0].events = POLLIN;
		descriptors[0].revents = 0;
		descriptors[1].fd = touch.fd;
		descriptors[1].events = POLLIN;
		descriptors[1].revents = 0;
		ready = poll(descriptors, ARRAY_SIZE(descriptors), 250);
		if (ready < 0 && errno != EINTR) {
			perror("screen: poll");
			result = 1;
			break;
		}
		if (descriptors[0].revents & POLLIN) {
			ssize_t count = read(master, output, sizeof(output));
			if (count > 0)
				terminal_output(&terminal, output, (size_t)count);
		}
		if (descriptors[1].revents & POLLIN) {
			int x;
			int y;

			if (read_touch_events(&touch) < 0) {
				perror("screen: touchscreen read");
				result = 1;
				break;
			}
			if (touch.tap_pending) {
				touch.tap_pending = false;
				transformed_touch(&touch, &fb, &x, &y);
				handle_key_tap(&fb, master, x, y, terminal.console_height);
			}
		}
		if (waitpid(shell_pid, &status, WNOHANG) == shell_pid) {
			shell_pid = -1;
			puts("screen: shell exited");
			break;
		}
	}

cleanup:
	if (shell_pid > 0) {
		kill(-shell_pid, SIGHUP);
		kill(shell_pid, SIGHUP);
		waitpid(shell_pid, NULL, 0);
	}
	if (slave >= 0)
		close(slave);
	if (master >= 0)
		close(master);
	if (touch.fd >= 0)
		close(touch.fd);
	close_framebuffer(&fb);
	if (read_pid_file() == getpid())
		unlink(pid_path);
	return result;
}

static void service_usage(void)
{
	fputs("Usage: screen-auto {start|stop|restart|status}\n", stderr);
}

static int stop_service(bool quiet)
{
	pid_t pid = (pid_t)read_pid_file();
	int attempts;

	if (!process_running(pid)) {
		unlink(pid_path);
		if (!quiet)
			puts("screen-auto: stopped");
		return 1;
	}
	if (kill(pid, SIGTERM) < 0) {
		perror("screen-auto: stop");
		return 1;
	}
	for (attempts = 0; attempts < 30 && process_running(pid); ++attempts)
		sleep_milliseconds(100);
	if (process_running(pid)) {
		fprintf(stderr, "screen-auto: PID %ld did not stop\n", (long)pid);
		return 1;
	}
	unlink(pid_path);
	if (!quiet)
		puts("screen-auto: stopped");
	return 0;
}

static int service_main(int argc, char **argv)
{
	const char *command = argc > 1 ? argv[1] : "status";
	pid_t pid;

	if (!strcmp(command, "run"))
		return run_console(true);
	if (!strcmp(command, "status")) {
		pid = (pid_t)read_pid_file();
		if (!process_running(pid)) {
			unlink(pid_path);
			puts("screen-auto: stopped");
			return 1;
		}
		printf("screen-auto: running (PID %ld)\n", (long)pid);
		return 0;
	}
	if (!strcmp(command, "stop"))
		return stop_service(false);
	if (!strcmp(command, "restart"))
		stop_service(true);
	else if (strcmp(command, "start")) {
		service_usage();
		return 2;
	}
	pid = (pid_t)read_pid_file();
	if (process_running(pid)) {
		printf("screen-auto: already running (PID %ld)\n", (long)pid);
		return 0;
	}
	unlink(pid_path);
	pid = vfork();
	if (pid == 0) {
		char *const arguments[] = { (char *)"screen-auto", (char *)"run", NULL };

		execv("/usr/bin/screen-auto", arguments);
		_exit(127);
	}
	if (pid < 0) {
		perror("screen-auto: start");
		return 1;
	}
	if (write_pid_file(pid) < 0) {
		kill(pid, SIGTERM);
		perror("screen-auto: write PID file");
		return 1;
	}
	printf("screen-auto: started (PID %ld)\n", (long)pid);
	return 0;
}

static const char *program_name(const char *path)
{
	const char *slash = strrchr(path, '/');

	return slash ? slash + 1 : path;
}

int main(int argc, char **argv)
{
	struct sigaction action;

	memset(&action, 0, sizeof(action));
	action.sa_handler = signal_handler;
	sigemptyset(&action.sa_mask);
	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGHUP, &action, NULL);
	signal(SIGPIPE, SIG_IGN);
	if (!strcmp(program_name(argv[0]), "screen-auto"))
		return service_main(argc, argv);
	if (argc > 1 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
		puts("Usage: screen\nRun the STM32 framebuffer touch console in the foreground.");
		return 0;
	}
	if (argc != 1) {
		fputs("Usage: screen\n", stderr);
		return 2;
	}
	return run_console(false);
}

#include <stdio.h>
#include <string.h>

struct applet {
	const char *name;
	int (*main)(int argc, char **argv);
};

#ifdef HWTOOLS_GPIO_LED
int gpio_led_main(int argc, char **argv);
#endif
#ifdef HWTOOLS_BUTTON_LED
int button_led_main(int argc, char **argv);
#endif
#ifdef HWTOOLS_BOOT_BUTTON
int boot_button_main(int argc, char **argv);
#endif
#ifdef HWTOOLS_ADC_READ
int adc_read_main(int argc, char **argv);
#endif
#ifdef HWTOOLS_I2C_SCAN
int i2c_scan_main(int argc, char **argv);
#endif
#ifdef HWTOOLS_SPI_GYRO
int spi_gyro_main(int argc, char **argv);
#endif
#ifdef HWTOOLS_RCC_CLOCK
int rcc_clock_main(int argc, char **argv);
#endif
#ifdef HWTOOLS_PWM_LED
int pwm_led_main(int argc, char **argv);
#endif
#ifdef HWTOOLS_UART_SEND
int uart_send_main(int argc, char **argv);
#endif
#ifdef HWTOOLS_RS485_CHAT
int rs485_chat_main(int argc, char **argv);
#endif
#ifdef HWTOOLS_TIMERS
int timers_main(int argc, char **argv);
#endif

static const struct applet applets[] = {
#ifdef HWTOOLS_GPIO_LED
	{ "gpio-led", gpio_led_main },
#endif
#ifdef HWTOOLS_BUTTON_LED
	{ "button-led", button_led_main },
#endif
#ifdef HWTOOLS_BOOT_BUTTON
	{ "boot-button", boot_button_main },
#endif
#ifdef HWTOOLS_ADC_READ
	{ "adc-read", adc_read_main },
#endif
#ifdef HWTOOLS_I2C_SCAN
	{ "i2c-scan", i2c_scan_main },
#endif
#ifdef HWTOOLS_SPI_GYRO
	{ "spi-gyro", spi_gyro_main },
#endif
#ifdef HWTOOLS_RCC_CLOCK
	{ "rcc-clock", rcc_clock_main },
#endif
#ifdef HWTOOLS_PWM_LED
	{ "pwm-led", pwm_led_main },
#endif
#ifdef HWTOOLS_UART_SEND
	{ "uart-send", uart_send_main },
#endif
#ifdef HWTOOLS_RS485_CHAT
	{ "rs485-chat", rs485_chat_main },
#endif
#ifdef HWTOOLS_TIMERS
	{ "timers", timers_main },
#endif
	{ NULL, NULL }
};

static const char *program_name(const char *path)
{
	const char *slash = strrchr(path, '/');

	return slash ? slash + 1 : path;
}

static const struct applet *find_applet(const char *name)
{
	const struct applet *applet;

	for (applet = applets; applet->name; ++applet)
		if (!strcmp(applet->name, name))
			return applet;
	return NULL;
}

static void usage(FILE *stream)
{
	const struct applet *applet;

	fputs("Usage: hw <applet> [arguments]\nAvailable applets:", stream);
	for (applet = applets; applet->name; ++applet)
		fprintf(stream, " %s", applet->name);
	fputc('\n', stream);
}

int main(int argc, char **argv)
{
	const char *name = program_name(argv[0]);
	const struct applet *applet;

	if (strcmp(name, "hw"))
		applet = find_applet(name);
	else if (argc > 1) {
		applet = find_applet(argv[1]);
		if (applet) {
			--argc;
			++argv;
		}
	} else {
		usage(stdout);
		return 0;
	}
	if (!applet) {
		usage(stderr);
		return 2;
	}
	return applet->main(argc, argv);
}

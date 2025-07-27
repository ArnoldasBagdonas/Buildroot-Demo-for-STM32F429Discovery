#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

#include "periphery/gpio.h"

#define GREEN_LED_GPIO "G13"
#define RED_LED_GPIO "G14"
#define BUTTON_GPIO "A0"

void GPIO_CHIP(const char *gpio_str, char *buf, size_t bufsize)
{
    if (gpio_str == NULL || buf == NULL || bufsize == 0)
    {
        if (buf && bufsize > 0)
            buf[0] = '\0';
        return;
    }
    if (gpio_str[0] < 'A' || gpio_str[0] > 'Z')
    {
        buf[0] = '\0';
        return;
    }
    int chip_num = gpio_str[0] - 'A';
    snprintf(buf, bufsize, "/dev/gpiochip%d", chip_num);
}

int GPIO_LINE(const char *gpio_str)
{
    if (gpio_str == NULL || gpio_str[1] == '\0')
    {
        return -1; // Invalid input
    }
    return atoi(&gpio_str[1]);
}

// Non-blocking terminal key check
int kbhit(void)
{
    struct termios oldt, newt;
    int ch;
    int oldf;

    // Save terminal settings
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    // Disable canonical mode and echo
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // Set non-blocking input
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF)
    {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

// Helper macros for stringifying macro values
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

int main(void)
{
#ifdef PERIPHERY_GPIO_CDEV_SUPPORT
    printf("periphery PERIPHERY_GPIO_CDEV_SUPPORT is defined as: %s\n", TOSTRING(PERIPHERY_GPIO_CDEV_SUPPORT));
#else
    printf("Error: periphery PERIPHERY_GPIO_CDEV_SUPPORT is undefined...\n");
    exit(1);
#endif

    printf("Defined GPIO macros:\n");
    printf("  GREEN_LED_GPIO = %s\n", GREEN_LED_GPIO);
    printf("  RED_LED_GPIO   = %s\n", RED_LED_GPIO);
    printf("  BUTTON_GPIO    = %s\n", BUTTON_GPIO);

    char button_chip[32];
    GPIO_CHIP(BUTTON_GPIO, button_chip, sizeof(button_chip));
    int button_line = GPIO_LINE(BUTTON_GPIO);
    printf("BUTTON_GPIO conversion:\n");
    printf("  Chip: %s\n", button_chip[0] ? button_chip : "(invalid)");
    printf("  Line: %d\n", button_line);

    char led_chip[32];
    GPIO_CHIP(RED_LED_GPIO, led_chip, sizeof(led_chip));
    int led_line = GPIO_LINE(RED_LED_GPIO);
    printf("RED_LED_GPIO conversion:\n");
    printf("  Chip: %s\n", led_chip[0] ? led_chip : "(invalid)");
    printf("  Line: %d\n", led_line);

    gpio_t *button_gpio = gpio_new();
    gpio_t *led_gpio = gpio_new();

    if (!button_gpio || !led_gpio)
    {
        fprintf(stderr, "Failed to allocate GPIO objects\n");
        exit(1);
    }

    printf("Opening button GPIO: chip='%s', line=%d, direction=IN\n", button_chip, button_line);
    if (gpio_open(button_gpio, button_chip, button_line, GPIO_DIR_IN) < 0)
    {
        fprintf(stderr, "gpio_open(button): %s\n", gpio_errmsg(button_gpio));
        gpio_free(button_gpio);
        gpio_free(led_gpio);
        exit(1);
    }

    printf("Opening LED GPIO: chip='%s', line=%d, direction=OUT\n", led_chip, led_line);
    if (gpio_open(led_gpio, led_chip, led_line, GPIO_DIR_OUT) < 0)
    {
        fprintf(stderr, "gpio_open(LED): %s\n", gpio_errmsg(led_gpio));
        gpio_close(button_gpio);
        gpio_free(button_gpio);
        gpio_free(led_gpio);
        exit(1);
    }

    printf("Reading button state, showing on LED...\n");
    printf("Press any key in the terminal to exit.\n");

    bool button_value;
    while (1)
    {
        if (gpio_read(button_gpio, &button_value) < 0)
        {
            fprintf(stderr, "gpio_read(): %s\n", gpio_errmsg(button_gpio));
            break;
        }

        if (gpio_write(led_gpio, button_value) < 0)
        {
            fprintf(stderr, "gpio_write(): %s\n", gpio_errmsg(led_gpio));
            break;
        }

        if (kbhit())
        {
            printf("Key pressed. Exiting loop...\n");
            break;
        }

        usleep(100000);
    }

    gpio_close(button_gpio);
    gpio_close(led_gpio);
    gpio_free(button_gpio);
    gpio_free(led_gpio);

    printf("Cleanup done, exiting.\n");
    return 0;
}

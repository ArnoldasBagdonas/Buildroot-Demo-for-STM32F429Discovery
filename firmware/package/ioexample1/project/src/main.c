#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "periphery/led.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Modify this if you want a different LED name
#define LED_NAME "led-green"
#define ORIGINAL_TRIGGER "heartbeat"

// Clear kernel trigger (e.g., "heartbeat") for manual control
void clear_led_trigger(const char *led_name)
{
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/leds/%s/trigger", led_name);

    int fd = open(path, O_WRONLY);
    if (fd < 0)
    {
        fprintf(stderr, "Warning: Failed to open '%s': %s\n", path, strerror(errno));
        return;
    }

    if (write(fd, "none", strlen("none")) < 0)
    {
        fprintf(stderr, "Warning: Failed to write 'none' to '%s': %s\n", path, strerror(errno));
    }
    else
    {
        printf("Cleared trigger for %s\n", led_name);
    }

    close(fd);
}

// Restore kernel trigger (e.g., "heartbeat") on exit
void restore_led_trigger(const char *led_name, const char *trigger)
{
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/leds/%s/trigger", led_name);

    int fd = open(path, O_WRONLY);
    if (fd < 0)
    {
        fprintf(stderr, "Warning: Failed to open '%s' to restore trigger: %s\n", path, strerror(errno));
        return;
    }

    if (write(fd, trigger, strlen(trigger)) < 0)
    {
        fprintf(stderr, "Warning: Failed to write '%s' to '%s': %s\n", trigger, path, strerror(errno));
    }
    else
    {
        printf("Restored trigger '%s' for %s\n", trigger, led_name);
    }

    close(fd);
}

int main(void)
{
#ifdef PERIPHERY_GPIO_CDEV_SUPPORT
    printf("periphery PERIPHERY_GPIO_CDEV_SUPPORT is defined as: %s\n", TOSTRING(PERIPHERY_GPIO_CDEV_SUPPORT));
#else
    printf("Error: periphery PERIPHERY_GPIO_CDEV_SUPPORT is undefined...\n");
    exit(1);
#endif

    led_t *led;
    unsigned int max_brightness;

    clear_led_trigger(LED_NAME);

    printf("Creating LED object...\n");
    led = led_new();
    if (!led)
    {
        printf("Error: led_new() returned NULL\n");
        exit(1);
    }

    printf("Opening LED '%s'...\n", LED_NAME);
    if (led_open(led, LED_NAME) < 0)
    {
        printf("Error: led_open() failed: %s\n", led_errmsg(led));
        led_free(led);
        exit(1);
    }
    printf("LED '%s' opened successfully.\n", LED_NAME);

    if (led_get_max_brightness(led, &max_brightness) < 0)
    {
        printf("Error: led_get_max_brightness() failed: %s\n", led_errmsg(led));
        led_close(led);
        led_free(led);
        exit(1);
    }

    char input[16];
    while (true)
    {
        printf("\nEnter 1 to turn ON, 0 to turn OFF, q to quit: ");
        if (fgets(input, sizeof(input), stdin) == NULL)
            continue;

        if (input[0] == '1')
        {
            if (led_write(led, true) < 0)
                printf("Error: led_write(true) failed: %s\n", led_errmsg(led));
            else
                printf("LED turned ON.\n");
        }
        else if (input[0] == '0')
        {
            if (led_write(led, false) < 0)
                printf("Error: led_write(false) failed: %s\n", led_errmsg(led));
            else
                printf("LED turned OFF.\n");
        }
        else if (input[0] == 'q' || input[0] == 'Q')
        {
            printf("Exiting program...\n");
            break;
        }
        else
        {
            printf("Invalid input. Please enter 1, 0, or q.\n");
        }
    }

    led_close(led);
    led_free(led);

    restore_led_trigger(LED_NAME, ORIGINAL_TRIGGER);

    return 0;
}

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "periphery/led.h"

int main(void)
{
    led_t *led;
    unsigned int max_brightness;

    printf("Creating LED object...\n");
    led = led_new();
    if (!led)
    {
        printf("Error: led_new() returned NULL\n");
        exit(1);
    }

    printf("Opening LED 'led-red'...\n");
    if (led_open(led, "led-red") < 0)
    {
        printf("Error: led_open() failed: %s\n", led_errmsg(led));
        led_free(led);
        exit(1);
    }
    printf("LED 'led-red' opened successfully.\n");

    // Read max brightness
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

    return 0;
}

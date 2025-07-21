#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>

#include "gpiodio/gpiodio.h"

static struct gpiod_chip *chip = NULL;
static struct gpiod_line *red_led_line = NULL;
static struct gpiod_line *button_line = NULL;
static int running = 1;

void cleanup(void) {
    if (red_led_line) {
        gpiod_line_release(red_led_line);
        red_led_line = NULL;
    }
    if (button_line) {
        gpiod_line_release(button_line);
        button_line = NULL;
    }
    if (chip) {
        gpiod_chip_close(chip);
        chip = NULL;
    }
}

void signal_handler(int signum) {
    if (signum == SIGINT) {
        printf("\nSIGINT received, terminating...\n");
    } else if (signum == SIGTERM) {
        printf("\nSIGTERM received, terminating...\n");
    }
    running = 0;
}

int main(void) {
    int ret, led_state = 0;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    chip = gpiod_chip_open(GPIO_CHIP);
    if (!chip) {
        perror("Failed to open GPIO chip");
        return 1;
    }

    red_led_line = gpiod_chip_get_line(chip, RED_LED_LINE);
    if (!red_led_line) {
        perror("Failed to get red LED line");
        cleanup();
        return 1;
    }

    button_line = gpiod_chip_get_line(chip, BUTTON_LINE);
    if (!button_line) {
        perror("Failed to get button line");
        cleanup();
        return 1;
    }

    ret = gpiod_line_request_output(red_led_line, "red_led_control", 0);
    if (ret < 0) {
        perror("Failed to request red LED line as output");
        cleanup();
        return 1;
    }

    ret = gpiod_line_request_input(button_line, "user_button");
    if (ret < 0) {
        perror("Failed to request button line as input");
        cleanup();
        return 1;
    }

    printf("Press the user button (PA0) to toggle the red LED (PG14).\n");
    printf("Press Ctrl+C to exit.\n");

    while (running) {
        int button_val = gpiod_line_get_value(button_line);
        if (button_val < 0) {
            perror("Failed to read button value");
            break;
        }

        if (button_val == 0) {  // button pressed (assuming active low)
            led_state = !led_state;
            ret = gpiod_line_set_value(red_led_line, led_state);
            if (ret < 0) {
                perror("Failed to set LED value");
                break;
            }

            printf("Button pressed! Red LED %s\n", led_state ? "ON" : "OFF");

            usleep(300000);
            while (running && gpiod_line_get_value(button_line) == 0) {
                usleep(10000);
            }
        }

        usleep(10000);
    }

    cleanup();
    printf("\nExiting gracefully.\n");
    return 0;
}

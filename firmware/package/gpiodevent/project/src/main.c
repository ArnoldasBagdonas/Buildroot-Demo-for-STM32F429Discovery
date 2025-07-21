#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>

#include "gpiodevent/gpiodevent.h"

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
    int ret;
    struct timespec timeout = {1, 0};  // 1 second timeout
    struct gpiod_line_event event;
    int events_received = 0;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    chip = gpiod_chip_open(GPIO_CHIP);
    if (!chip) {
        perror("Failed to open GPIO chip");
        return 1;
    }

    button_line = gpiod_chip_get_line(chip, BUTTON_LINE);
    if (!button_line) {
        perror("Failed to get button line");
        cleanup();
        return 1;
    }

    // Request rising edge events on button line
    ret = gpiod_line_request_rising_edge_events(button_line, CONSUMER);
    if (ret < 0) {
        perror("Failed to request rising edge events");
        cleanup();
        return 1;
    }

    printf("Waiting for up to %d rising edge events on line %u...\n", MAX_EVENTS, BUTTON_LINE);

    while (running && events_received < MAX_EVENTS) {
        ret = gpiod_line_event_wait(button_line, &timeout);
        if (ret < 0) {
            perror("Error while waiting for event");
            break;
        } else if (ret == 0) {
            printf("Timeout waiting for event on line %u\n", BUTTON_LINE);
            continue;
        }

        ret = gpiod_line_event_read(button_line, &event);
        if (ret < 0) {
            perror("Failed to read event");
            break;
        }

        printf("Event #%d detected on line %u: ", events_received + 1, BUTTON_LINE);
        if (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE) {
            printf("Rising edge\n");
        } else if (event.event_type == GPIOD_LINE_EVENT_FALLING_EDGE) {
            printf("Falling edge\n");
        } else {
            printf("Unknown event type\n");
        }

        events_received++;
    }

    cleanup();
    printf("Exiting after %d event(s)\n", events_received);
    return 0;
}

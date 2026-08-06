#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#include "periphery/pwm.h"

static int parse_nonnegative_int(const char *text, int max, int *result)
{
    char *endptr;
    long value;

    errno = 0;
    value = strtol(text, &endptr, 10);
    if (errno || endptr == text)
        return -1;

    while (isspace((unsigned char)*endptr))
        endptr++;

    if (*endptr != '\0' || value < 0 || value > max)
        return -1;

    *result = (int)value;
    return 0;
}

/**
 * @brief Main entry point of the program.
 *
 * Demonstrates enabling PWM on STM32F429 Disco board
 * and interactively changing its duty cycle.
 *
 * @return int 0 on success, non-zero on failure.
 */
int main(int argc, char *argv[])
{
    int chip = 0;    // Default chip
    int channel = 0; // Default channel
    int exit_status = EXIT_SUCCESS;
    pwm_t *pwm = NULL;

    /* Parse command-line arguments */
    if (argc == 3)
    {
        if (parse_nonnegative_int(argv[1], INT32_MAX, &chip) < 0)
        {
            fprintf(stderr, "Invalid chip value: '%s'\n", argv[1]);
            return EXIT_FAILURE;
        }

        if (parse_nonnegative_int(argv[2], INT32_MAX, &channel) < 0)
        {
            fprintf(stderr, "Invalid channel value: '%s'\n", argv[2]);
            return EXIT_FAILURE;
        }
    }
    else if (argc != 1)
    {
        fprintf(stderr, "Usage: %s [chip channel]\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("External LED wiring:\n");
    printf("  PB4 -> 330-680 ohm resistor -> LED anode\n");
    printf("  GND -> LED cathode\n\n");

    pwm = pwm_new();
    if (!pwm)
    {
        fprintf(stderr, "Failed to allocate PWM instance\n");
        return EXIT_FAILURE;
    }

    /* Open PWM */
    if (pwm_open(pwm, chip, channel) < 0)
    {
        fprintf(stderr, "pwm_open(): %s\n", pwm_errmsg(pwm));
        pwm_free(pwm);
        return EXIT_FAILURE;
    }

    /* Set base frequency */
    if (pwm_set_frequency(pwm, 1000.0) < 0)
    {
        fprintf(stderr, "pwm_set_frequency(): %s\n", pwm_errmsg(pwm));
        exit_status = EXIT_FAILURE;
        goto cleanup;
    }

    if (pwm_set_duty_cycle(pwm, 0.0) < 0)
    {
        fprintf(stderr, "pwm_set_duty_cycle(): %s\n", pwm_errmsg(pwm));
        exit_status = EXIT_FAILURE;
        goto cleanup;
    }
    printf("PWM frequency set to 1 kHz on chip%d, channel%d.\n", chip, channel);

    /* Enable PWM */
    if (pwm_enable(pwm) < 0)
    {
        fprintf(stderr, "pwm_enable(): %s\n", pwm_errmsg(pwm));
        exit_status = EXIT_FAILURE;
        goto cleanup;
    }
    printf("PWM enabled on chip%d, channel%d.\n", chip, channel);

    for (;;)
    {
        char input[32];
        char *value_text = input;
        int duty_percent;

        printf("\nEnter duty cycle (0..100%%) or q to quit: ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin))
            break;

        while (isspace((unsigned char)*value_text))
            value_text++;

        if (*value_text == 'q' || *value_text == 'Q')
        {
            char *tail = value_text + 1;

            while (isspace((unsigned char)*tail))
                tail++;

            if (*tail == '\0')
                break;
        }

        if (parse_nonnegative_int(value_text, 100, &duty_percent) < 0)
        {
            printf("Invalid value. Enter an integer from 0 to 100, or q.\n");
            continue;
        }

        if (pwm_set_duty_cycle(pwm, duty_percent / 100.0) < 0)
        {
            fprintf(stderr, "pwm_set_duty_cycle(): %s\n", pwm_errmsg(pwm));
            exit_status = EXIT_FAILURE;
            goto cleanup;
        }

        printf("Duty cycle set to %d%%. Observe the LED brightness.\n", duty_percent);
    }

cleanup:
    pwm_set_duty_cycle(pwm, 0.0);
    pwm_disable(pwm);
    pwm_close(pwm);
    pwm_free(pwm);
    printf("\nPWM disabled.\n");
    return exit_status;
}

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "periphery/pwm.h"

/**
 * @brief Waits for the user to press ENTER before continuing.
 *
 * @param message Message displayed before waiting.
 */
static void wait_for_user(const char *message)
{
    printf("\n%s\nPress ENTER to continue...", message);
    getchar();
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
    pwm_t *pwm = NULL;

    /* Parse command-line arguments */
    if (argc == 3)
    {
        char *endptr;

        errno = 0;
        long val_chip = strtol(argv[1], &endptr, 10);
        if (errno || *endptr != '\0' || val_chip < 0)
        {
            fprintf(stderr, "Invalid chip value: '%s'\n", argv[1]);
            return EXIT_FAILURE;
        }
        chip = (int)val_chip;

        errno = 0;
        long val_channel = strtol(argv[2], &endptr, 10);
        if (errno || *endptr != '\0' || val_channel < 0)
        {
            fprintf(stderr, "Invalid channel value: '%s'\n", argv[2]);
            return EXIT_FAILURE;
        }
        channel = (int)val_channel;
    }
    else
    {
        fprintf(stderr, "Usage: %s [chip] [channel]\nDefaulting to chip=%d, channel=%d\n",
                argv[0], chip, channel);
    }

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
        goto cleanup;
    }
    printf("PWM frequency set to 1 kHz on chip%d, channel%d.\n", chip, channel);

    /* Enable PWM */
    if (pwm_enable(pwm) < 0)
    {
        fprintf(stderr, "pwm_enable(): %s\n", pwm_errmsg(pwm));
        goto cleanup;
    }
    printf("PWM enabled on chip%d, channel%d.\n", chip, channel);

    /* Step 1: 25% duty */
    if (pwm_set_duty_cycle(pwm, 0.25) < 0)
    {
        fprintf(stderr, "pwm_set_duty_cycle(): %s\n", pwm_errmsg(pwm));
        goto cleanup;
    }
    printf("Duty cycle set to 25%% on chip%d, channel%d.\n", chip, channel);
    wait_for_user("Observe LED brightness at 25% duty");

    /* Step 2: 50% duty */
    if (pwm_set_duty_cycle(pwm, 0.50) < 0)
    {
        fprintf(stderr, "pwm_set_duty_cycle(): %s\n", pwm_errmsg(pwm));
        goto cleanup;
    }
    printf("Duty cycle set to 50%% on chip%d, channel%d.\n", chip, channel);
    wait_for_user("Observe LED brightness at 50% duty");

    /* Step 3: 75% duty */
    if (pwm_set_duty_cycle(pwm, 0.75) < 0)
    {
        fprintf(stderr, "pwm_set_duty_cycle(): %s\n", pwm_errmsg(pwm));
        goto cleanup;
    }
    printf("Duty cycle set to 75%% on chip%d, channel%d.\n", chip, channel);
    wait_for_user("Observe LED brightness at 75% duty");

    /* Step 4: 100% duty */
    if (pwm_set_duty_cycle(pwm, 1.0) < 0)
    {
        fprintf(stderr, "pwm_set_duty_cycle(): %s\n", pwm_errmsg(pwm));
        goto cleanup;
    }
    printf("Duty cycle set to 100%% (always ON) on chip%d, channel%d.\n", chip, channel);
    wait_for_user("Observe LED brightness at 100% duty");

    printf("\nTest complete. Disabling PWM on chip%d, channel%d...\n", chip, channel);

cleanup:
    pwm_disable(pwm);
    pwm_close(pwm);
    pwm_free(pwm);
    return 0;
}

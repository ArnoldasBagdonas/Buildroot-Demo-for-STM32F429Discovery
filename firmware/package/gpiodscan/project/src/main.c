#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main(void)
{
    int chipIndex = 0;
    struct gpiod_chip *chip;

    printf("Scanning all GPIO controllers and lines...\n");
    printf("---------------------------------------------------------\n");

    // Loop over chips until no more chips can be opened
    while ((chip = gpiod_chip_open_by_number(chipIndex)) != NULL) {
        const char *chip_name = gpiod_chip_name(chip);
        int num_lines = gpiod_chip_num_lines(chip);
        printf("GPIO Controller %d: %s with %d lines\n", chipIndex, chip_name, num_lines);
        printf("Line | Consumer Name\n");
        printf("-------------------\n");

        for (int lineNum = 0; lineNum < num_lines; lineNum++) {
            struct gpiod_line *line = gpiod_chip_get_line(chip, lineNum);
            if (!line) {
                printf("%4d | ERROR getting line\n", lineNum);
                continue;
            }

            // Try to get consumer name (NULL if unused)
            const char *consumer = gpiod_line_consumer(line);
            if (consumer == NULL)
                consumer = "(unused)";

            printf("%4d | %s\n", lineNum, consumer);

            gpiod_line_release(line);
        }

        printf("---------------------------------------------------------\n");

        gpiod_chip_close(chip);
        chipIndex++;
    }

    if (errno != 0) {
        perror("Error opening GPIO chip");
        return EXIT_FAILURE;
    }

    printf("Scan complete.\n");
    return EXIT_SUCCESS;
}

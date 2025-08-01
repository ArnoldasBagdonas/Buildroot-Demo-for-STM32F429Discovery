#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "periphery/serial.h"

#define MAX_MSG_LEN 255

serial_t *serial = NULL;

void cleanup(int exit_code)
{
    if (serial)
    {
        serial_close(serial);
        serial_free(serial);
        serial = NULL;
    }
    exit(exit_code);
}

void print_help(const char *progname)
{
    printf("\nUsage: %s <tty device>\n", progname);
    printf("Example: %s /dev/ttySTM1\n", progname);
    printf("--------------------------------------------------------\n");
    printf("This program configures the given serial port 115200 8N1\n");
    printf("no flow control mode and allows you to type messages\n");
    printf("(up to 255 characters) which will be sent every time you\n");
    printf(" press Enter.\n");
    printf("Press Enter on an empty line to exit.\n");
    printf("--------------------------------------------------------\n\n");
}

int main(int argc, char *argv[])
{
    char message[MAX_MSG_LEN + 1];
    const char *tty_path = "/dev/ttySTM1"; // Default

    print_help(argv[0]);

    if (argc == 1)
    {
        printf("[INFO] No serial device specified, using default: %s\n\n", tty_path);
    }
    else
    {
        tty_path = argv[1];
        printf("[INFO] Using serial device: %s\n\n", tty_path);
    }

    serial = serial_new();
    if (!serial)
    {
        fprintf(stderr, "Failed to create serial object\n");
        cleanup(EXIT_FAILURE);
    }

    if (serial_open(serial, tty_path, 115200) < 0)
    {
        fprintf(stderr, "serial_open(): %s\n", serial_errmsg(serial));
        cleanup(EXIT_FAILURE);
    }

    printf("Serial configured on %s. Type your message and press Enter.\n", tty_path);
    printf("Empty input will terminate the program.\n");

    while (1)
    {
        printf("> ");
        fflush(stdout);

        if (!fgets(message, sizeof(message), stdin))
        {
            printf("\nInput error or EOF detected. Exiting.\n");
            cleanup(EXIT_SUCCESS);
        }

        message[strcspn(message, "\n")] = '\0'; // Strip newline

        if (strlen(message) == 0)
        {
            printf("Empty input detected. Exiting.\n");
            cleanup(EXIT_SUCCESS);
        }

        int len = serial_write(serial, message, strlen(message));
        if (len < 0)
        {
            fprintf(stderr, "serial_write(): %s\n", serial_errmsg(serial));
            cleanup(EXIT_FAILURE);
        }

        printf("Sent %d bytes.\n", len);
    }

    cleanup(EXIT_SUCCESS); // This will never be reached but good practice
}

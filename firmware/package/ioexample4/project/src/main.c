#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>

#include "periphery/spi.h"

/* -------------------- Configuration -------------------- */
#define SPI_DEVICE "/dev/spidev0.0"
#define SPI_MODE 3
#define SPI_SPEED_HZ 1000000

#define REG_WHOAMI 0x0F
#define REG_CTRL1 0x20
#define REG_TEMP 0x26
#define REG_OUT_X_L 0x28

/* -------------------- Utility Functions -------------------- */

/**
 * @brief Write a single byte to a register via SPI.
 *
 * @param spi Pointer to the SPI device structure.
 * @param reg Register address (MSB = 0 for write).
 * @param val Value to write to the register.
 * @return 0 on success, negative value on error.
 */
static int spi_write_register(spi_t *spi, uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = {(uint8_t)(reg & 0x7F), val};
    return spi_transfer(spi, tx, NULL, sizeof(tx));
}

/**
 * @brief Read a single byte from a register via SPI.
 *
 * @param spi Pointer to the SPI device structure.
 * @param reg Register address (MSB = 1 for read).
 * @param val Pointer to store the read value.
 * @return 0 on success, negative value on error.
 */
static int spi_read_register(spi_t *spi, uint8_t reg, uint8_t *val)
{
    uint8_t tx[2] = {(uint8_t)(reg | 0x80), 0x00};
    uint8_t rx[2] = {0};

    if (spi_transfer(spi, tx, rx, sizeof(tx)) < 0)
        return -1;

    *val = rx[1];
    return 0;
}

/**
 * @brief Read multiple consecutive registers via SPI.
 *
 * @param spi Pointer to the SPI device structure.
 * @param start_reg Starting register address.
 * @param buf Buffer to store read data.
 * @param len Number of bytes to read.
 * @return 0 on success, negative value on error.
 */
static int spi_read_multi(spi_t *spi, uint8_t start_reg, uint8_t *buf, size_t len)
{
    uint8_t tx[1 + len];
    uint8_t rx[1 + len];

    tx[0] = start_reg | 0xC0; // Read + auto-increment
    for (size_t i = 1; i <= len; i++)
        tx[i] = 0x00;

    if (spi_transfer(spi, tx, rx, len + 1) < 0)
        return -1;

    for (size_t i = 0; i < len; i++)
        buf[i] = rx[i + 1];

    return 0;
}

/**
 * @brief Check if a key has been pressed (non-blocking).
 *
 * @return 1 if a key was pressed, 0 otherwise.
 */
static int kbhit(void)
{
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF)
    {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

/**
 * @brief Delay execution for a specified number of microseconds.
 *
 * @param us Delay duration in microseconds.
 */
void delay_us(unsigned int us)
{
    struct timeval tv;
    tv.tv_sec = us / 1000000;
    tv.tv_usec = us % 1000000;

    while (select(0, NULL, NULL, NULL, &tv) == -1)
    {
        if (errno == EINTR)
        {
            printf("Sleep interrupted, retrying...\n");
            continue;
        }
        else
        {
            perror("select");
            exit(EXIT_FAILURE);
        }
    }
}

/**
 * @brief Wait for a delay or stop if a key is pressed.
 *
 * @param us Delay duration in microseconds.
 * @return 1 if a key was pressed, 0 if timeout elapsed.
 */
int delay_or_keypress(unsigned int us)
{
    struct timeval tv;
    fd_set readfds;
    int retval;

    tv.tv_sec = us / 1000000;
    tv.tv_usec = us % 1000000;

    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    retval = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, 0);

    if (retval > 0 && FD_ISSET(STDIN_FILENO, &readfds))
    {
        getchar(); // Consume the key
        return 1;
    }
    return 0;
}

/* -------------------- Initialization & Cleanup -------------------- */

/**
 * @brief Free SPI resources safely.
 *
 * @param spi Pointer to the SPI device structure.
 */
static void cleanup(spi_t *spi)
{
    if (spi)
    {
        spi_close(spi);
        spi_free(spi);
    }
}

/* -------------------- Main Program -------------------- */

/**
 * @brief Entry point of the SPI sensor reader program.
 *
 * @param argc Argument count.
 * @param argv Argument vector (optional delay in ms).
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error.
 */
int main(int argc, char *argv[])
{
    unsigned int delay_ms = 1000; // Default 1 second
    spi_t *spi = NULL;

    /* Parse command-line argument */
    if (argc == 2)
    {
        char *endptr;
        errno = 0;
        long val = strtol(argv[1], &endptr, 10);
        if (errno || *endptr != '\0' || val <= 0)
        {
            fprintf(stderr, "Invalid delay value: '%s'\n", argv[1]);
            return EXIT_FAILURE;
        }
        delay_ms = (unsigned int)val;
    }
    else
    {
        fprintf(stderr, "Usage: %s [delay_ms]\nDefaulting to %u ms\n", argv[0], delay_ms);
    }

    printf("Starting SPI sensor readout...\n");

    /* Initialize SPI */
    spi = spi_new();
    if (spi_open(spi, SPI_DEVICE, SPI_MODE, SPI_SPEED_HZ) < 0)
    {
        fprintf(stderr, "spi_open(): %s\n", spi_errmsg(spi));
        cleanup(spi);
        return EXIT_FAILURE;
    }

    /* WHO_AM_I check */
    uint8_t whoami = 0;
    if (spi_read_register(spi, REG_WHOAMI, &whoami) < 0)
    {
        fprintf(stderr, "Failed to read WHO_AM_I\n");
        cleanup(spi);
        return EXIT_FAILURE;
    }
    printf("WHO_AM_I: 0x%02X\n", whoami);

    /* Enable device */
    if (spi_write_register(spi, REG_CTRL1, 0x0F) < 0)
    {
        fprintf(stderr, "Failed to write CTRL_REG1\n");
        cleanup(spi);
        return EXIT_FAILURE;
    }

    delay_us(100000);

    printf("Press any key to stop...\n");

    /* Main loop */
    while (!kbhit())
    {
        uint8_t temp_raw;
        uint8_t buf[6];

        if (spi_read_register(spi, REG_TEMP, &temp_raw) < 0)
        {
            fprintf(stderr, "\nError: Failed to read temperature register\n");
            break;
        }

        if (spi_read_multi(spi, REG_OUT_X_L, buf, sizeof(buf)) < 0)
        {
            fprintf(stderr, "\nError: Failed to read gyro XYZ registers\n");
            break;
        }

        int16_t x = (int16_t)(buf[1] << 8 | buf[0]);
        int16_t y = (int16_t)(buf[3] << 8 | buf[2]);
        int16_t z = (int16_t)(buf[5] << 8 | buf[4]);

        printf("\r0x%02X | Temp: %3d°C | X: %6d | Y: %6d | Z: %6d   ",
               whoami, 25 + (int8_t)temp_raw, x, y, z);
        fflush(stdout);

        if (delay_or_keypress(delay_ms * 1000))
        {
            printf("\nKey pressed. Stopping...\n");
            break;
        }
    }

    printf("\nStopping...\n");
    cleanup(spi);

    return EXIT_SUCCESS;
}

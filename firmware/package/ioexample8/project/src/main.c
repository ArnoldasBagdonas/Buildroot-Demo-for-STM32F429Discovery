#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/serial.h>

#define MAX_MSG_LEN 255
// #define READ_BUF_SIZE 256

// Use a unique variable name to avoid conflicts
int rs485_fd = -1;

void cleanup(int exit_code)
{
    if (rs485_fd >= 0)
        close(rs485_fd);

    exit(exit_code);
}

void print_help(const char *progname)
{
    printf("\nUsage: %s <tty device>\n", progname);
    printf("Example: %s /dev/ttySTM1\n", progname);
    printf("--------------------------------------------------------\n");
    printf("This program configures the given serial port for RS-485\n");
    printf("mode at 115200 8N1, no flow control. Type messages up to\n");
    printf("255 characters, press Enter to send. Empty line exits.\n");
    printf("--------------------------------------------------------\n\n");
}

int configure_serial(const char *tty_path)
{
    struct termios tty;

    rs485_fd = open(tty_path, O_RDWR | O_NOCTTY);
    if (rs485_fd < 0)
    {
        perror("open");
        return -1;
    }

    if (tcgetattr(rs485_fd, &tty) != 0)
    {
        perror("tcgetattr");
        return -1;
    }

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag &= ~PARENB; // 8N1
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

    tty.c_cflag &= ~CRTSCTS; // no flow control
    tty.c_cflag |= CREAD | CLOCAL;

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN] = 1; // Minimum number of bytes to read before read() returns.
                        // Setting VMIN = 1 means read() will block until at least 1 byte is received.

    tty.c_cc[VTIME] = 1; // Timeout for read() in tenths of a second (i.e., 0.1 seconds = 100 ms).
                         // This value specifies the maximum time to wait for data after receiving
                         // the first byte.

    if (tcsetattr(rs485_fd, TCSANOW, &tty) != 0)
    {
        perror("tcsetattr");
        return -1;
    }

    return 0;
}

int configure_rs485()
{
    // reference: https://docs.kernel.org/driver-api/serial/serial-rs485.html#dt-bindings
    struct serial_rs485 rs485conf;
    memset(&rs485conf, 0, sizeof(rs485conf));

    rs485conf.flags |= SER_RS485_ENABLED;         // Enable RS485 mode
    rs485conf.flags |= SER_RS485_RTS_ON_SEND;     // RTS high during send
    rs485conf.flags &= ~SER_RS485_RTS_AFTER_SEND; // RTS low after send

    // Optional RTS delay (microseconds)
    rs485conf.delay_rts_before_send = 1;
    rs485conf.delay_rts_after_send = 1;

    /* Set this flag if you want to receive data even while sending data */
    // rs485conf.flags |= SER_RS485_RX_DURING_TX;

    if (ioctl(rs485_fd, TIOCSRS485, &rs485conf) < 0)
    {
        perror("TIOCSRS485");
        return -1;
    }

    printf("[INFO] RS485 mode configured successfully.\n");
    return 0;
}

int main(int argc, char *argv[])
{
    char message[MAX_MSG_LEN + 1];
    const char *tty_path = "/dev/ttySTM1"; // default

    print_help(argv[0]);

    if (argc == 1)
        printf("[INFO] No serial device specified, using default: %s\n\n", tty_path);
    else
    {
        tty_path = argv[1];
        printf("[INFO] Using serial device: %s\n\n", tty_path);
    }

    if (configure_serial(tty_path) < 0)
        cleanup(EXIT_FAILURE);

    if (configure_rs485() < 0)
        cleanup(EXIT_FAILURE);

    printf("RS485 serial configured on %s. Type your message and press Enter.\n", tty_path);
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

        message[strcspn(message, "\n")] = '\0'; // strip newline

        if (strlen(message) == 0)
        {
            printf("Empty input detected. Exiting.\n");
            cleanup(EXIT_SUCCESS);
        }

        int len = write(rs485_fd, message, strlen(message));
        tcdrain(rs485_fd); // Wait until all data is transmitted
        if (len < 0)
        {
            perror("write");
            cleanup(EXIT_FAILURE);
        }

        printf("Sent %d bytes.\n", len);
#if defined(READ_BUF_SIZE)
        // Now read response with timeout 100ms
        char read_buf[READ_BUF_SIZE];
        memset(read_buf, 0, sizeof(read_buf));

        // read() will wait for 1 byte or timeout after 100ms due to termios settings
        int rlen = read(rs485_fd, read_buf, sizeof(read_buf) - 1);

        if (rlen < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                printf("Read timeout (no data received in 100 ms)\n");
            }
            else
            {
                perror("read");
                cleanup(EXIT_FAILURE);
            }
        }
        else if (rlen == 0)
        {
            printf("No data received (read returned 0).\n");
        }
        else
        {
            read_buf[rlen] = '\0'; // Null-terminate
            printf("Received %d bytes: %s\n", rlen, read_buf);
        }
#endif
    }

    cleanup(EXIT_SUCCESS);
}

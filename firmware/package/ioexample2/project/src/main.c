#include <fcntl.h>
#include <linux/gpio.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define BUTTON_CHIP "/dev/gpiochip0"
#define BUTTON_LINE 0
#define LED_CHIP "/dev/gpiochip6"
#define LED_LINE 13

static int request_line(const char *path, unsigned int offset, __u64 flags)
{
    struct gpio_v2_line_request request = {0};
    int chip_fd = open(path, O_RDONLY);

    if (chip_fd < 0)
    {
        perror(path);
        return -1;
    }

    request.offsets[0] = offset;
    request.num_lines = 1;
    request.config.flags = flags;
    strncpy(request.consumer, "ioexample2", sizeof(request.consumer) - 1);

    if (ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &request) < 0)
    {
        perror("GPIO_V2_GET_LINE_IOCTL");
        close(chip_fd);
        return -1;
    }

    close(chip_fd);
    return request.fd;
}

static int read_line(int fd, int *value)
{
    struct gpio_v2_line_values values = {.mask = 1};

    if (ioctl(fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &values) < 0)
        return -1;
    *value = values.bits & 1;
    return 0;
}

static int write_line(int fd, int value)
{
    struct gpio_v2_line_values values = {
        .bits = value ? 1 : 0,
        .mask = 1,
    };

    return ioctl(fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values);
}

int main(void)
{
    struct termios saved_termios;
    struct pollfd input = {.fd = STDIN_FILENO, .events = POLLIN};
    int button_fd;
    int led_fd;
    int have_termios = 0;

    button_fd = request_line(BUTTON_CHIP, BUTTON_LINE, GPIO_V2_LINE_FLAG_INPUT);
    if (button_fd < 0)
        return 1;

    led_fd = request_line(LED_CHIP, LED_LINE, GPIO_V2_LINE_FLAG_OUTPUT);
    if (led_fd < 0)
    {
        close(button_fd);
        return 1;
    }

    if (tcgetattr(STDIN_FILENO, &saved_termios) == 0)
    {
        struct termios raw = saved_termios;
        raw.c_lflag &= ~(ICANON | ECHO);
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0)
            have_termios = 1;
    }

    printf("Button PA0 controls green LED PG13. Press any key to exit.\n");
    for (;;)
    {
        int pressed;

        if (read_line(button_fd, &pressed) < 0)
        {
            perror("GPIO_V2_LINE_GET_VALUES_IOCTL");
            break;
        }
        if (write_line(led_fd, pressed) < 0)
        {
            perror("GPIO_V2_LINE_SET_VALUES_IOCTL");
            break;
        }
        if (poll(&input, 1, 100) > 0)
            break;
    }

    write_line(led_fd, 0);
    if (have_termios)
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);
    close(led_fd);
    close(button_fd);
    printf("\nCleanup done, exiting.\n");
    return 0;
}

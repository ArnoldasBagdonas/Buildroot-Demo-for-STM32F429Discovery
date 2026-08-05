#include <fcntl.h>
#include <linux/gpio.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define LED_CHIP "/dev/gpiochip6"
#define LED_LINE 14

static int open_led(void)
{
    struct gpio_v2_line_request request = {0};
    int chip_fd;

    chip_fd = open(LED_CHIP, O_RDONLY);
    if (chip_fd < 0)
    {
        perror("open " LED_CHIP);
        return -1;
    }

    request.offsets[0] = LED_LINE;
    request.num_lines = 1;
    request.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
    strncpy(request.consumer, "ioexample1", sizeof(request.consumer) - 1);

    if (ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &request) < 0)
    {
        perror("GPIO_V2_GET_LINE_IOCTL");
        close(chip_fd);
        return -1;
    }

    close(chip_fd);
    return request.fd;
}

static int set_led(int fd, int on)
{
    struct gpio_v2_line_values values = {
        .bits = on ? 1 : 0,
        .mask = 1,
    };

    return ioctl(fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values);
}

int main(void)
{
    char input[16];
    int led_fd = open_led();

    if (led_fd < 0)
        return 1;

    printf("Red LED ready on PG14.\n");
    for (;;)
    {
        printf("\nEnter 1 to turn ON, 0 to turn OFF, q to quit: ");
        if (!fgets(input, sizeof(input), stdin))
            break;

        if (input[0] == '1' || input[0] == '0')
        {
            int on = input[0] == '1';
            if (set_led(led_fd, on) < 0)
                perror("GPIO_V2_LINE_SET_VALUES_IOCTL");
            else
                printf("LED turned %s.\n", on ? "ON" : "OFF");
        }
        else if (input[0] == 'q' || input[0] == 'Q')
        {
            break;
        }
        else
        {
            printf("Invalid input. Please enter 1, 0, or q.\n");
        }
    }

    set_led(led_fd, 0);
    close(led_fd);
    return 0;
}

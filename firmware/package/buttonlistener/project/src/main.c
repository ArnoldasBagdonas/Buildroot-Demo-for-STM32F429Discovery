#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <string.h>
#include <errno.h>

#define DEVICE "/dev/input/event1"  // Change to your correct input device
#define KEY_CODE KEY_HOME           // From <linux/input-event-codes.h>

int main() {
    int fd = open(DEVICE, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open input device");
        return 1;
    }

    struct input_event ev;
    printf("Listening for KEY_HOME (button-0) events on %s...\n", DEVICE);

    while (1) {
        ssize_t n = read(fd, &ev, sizeof(ev));
        if (n == (ssize_t)sizeof(ev)) {
            if (ev.type == EV_KEY && ev.code == KEY_CODE) {
                if (ev.value == 1)
                    printf("Button pressed\n");
                else if (ev.value == 0)
                    printf("Button released\n");
            }
        } else {
            perror("Failed to read input event");
            break;
        }
    }

    close(fd);
    return 0;
}

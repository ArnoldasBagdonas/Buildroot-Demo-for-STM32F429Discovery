#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ledbrightness/ledbrightness.h"

void led_set(int value) {
    FILE *f = fopen(LED_BRIGHTNESS_PATH, "w");
    if (!f) {
        perror("Failed to open brightness file");
        exit(1);
    }
    fprintf(f, "%d", value);
    fclose(f);
}

int main() {
    // Blink LED 5 times
    for (int i = 0; i < 5; i++) {
        led_set(1);  // ON
        sleep(1);
        led_set(0);  // OFF
        sleep(1);
    }
    return 0;
}
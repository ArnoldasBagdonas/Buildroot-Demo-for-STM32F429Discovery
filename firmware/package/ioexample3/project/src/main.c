#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "periphery/i2c.h"

int main()
{
    const char *i2c_path = "/dev/i2c-0";
    i2c_t *i2c;
    int ret;

    i2c = i2c_new();
    if ((ret = i2c_open(i2c, i2c_path)) < 0)
    {
        fprintf(stderr, "Failed to open I2C bus: %s\n", i2c_errmsg(i2c));
        return 1;
    }

    printf("Scanning I2C bus %s...\n", i2c_path);

    for (uint8_t addr = 0x03; addr < 0x78; addr++)
    {
        struct i2c_msg msg;
        memset(&msg, 0, sizeof(msg));
        msg.addr = addr;
        msg.flags = 0; // Write transaction
        msg.len = 0;   // Zero-length write
        msg.buf = NULL;

        ret = i2c_transfer(i2c, &msg, 1);
        if (ret == 0)
        {
            printf("Device found at address 0x%02X\n", addr);
        }
    }

    i2c_close(i2c);
    i2c_free(i2c);
    return 0;
}

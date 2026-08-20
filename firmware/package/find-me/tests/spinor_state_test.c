#define _XOPEN_SOURCE 700

#include "spinor_state.h"

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_ERASE_SIZE 4096
#define TEST_DEVICE_SIZE (TEST_ERASE_SIZE * 2)
#define SECOND_SLOT_CRC_OFFSET (TEST_ERASE_SIZE + 12)

int main(void)
{
    char path[] = "/tmp/find-me-spinor-XXXXXX";
    uint8_t first[64];
    uint8_t second[64];
    uint8_t loaded[64];
    uint8_t corrupt;
    int descriptor;
    size_t index;

    descriptor = mkstemp(path);
    assert(descriptor >= 0);
    assert(ftruncate(descriptor, TEST_DEVICE_SIZE) == 0);
    close(descriptor);

    for (index = 0; index < sizeof(first); index++)
    {
        first[index] = (uint8_t)index;
        second[index] = (uint8_t)(0x5aU ^ index);
    }

    assert(!spinor_state_load(path, loaded, sizeof(loaded)));
    assert(spinor_state_save(path, first, sizeof(first)));
    assert(spinor_state_load(path, loaded, sizeof(loaded)));
    assert(memcmp(loaded, first, sizeof(first)) == 0);
    assert(spinor_state_save(path, second, sizeof(second)));
    assert(spinor_state_load(path, loaded, sizeof(loaded)));
    assert(memcmp(loaded, second, sizeof(second)) == 0);

    descriptor = open(path, O_RDWR);
    assert(descriptor >= 0);
    assert(pread(descriptor, &corrupt, 1U, SECOND_SLOT_CRC_OFFSET) == 1);
    corrupt ^= 0x80U;
    assert(pwrite(descriptor, &corrupt, 1U, SECOND_SLOT_CRC_OFFSET) == 1);
    close(descriptor);
    assert(spinor_state_load(path, loaded, sizeof(loaded)));
    assert(memcmp(loaded, first, sizeof(first)) == 0);

    assert(!spinor_state_save(path, first, SPINOR_STATE_MAX_PAYLOAD + 1U));
    assert(unlink(path) == 0);
    puts("SPI-NOR state tests passed");
    return 0;
}

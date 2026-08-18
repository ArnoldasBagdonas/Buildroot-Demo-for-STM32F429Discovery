#define _XOPEN_SOURCE 700

#include "fram_state.h"

#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define FRAM_SLOT_SIZE 256U
#define FRAM_SLOT_COUNT 2U
#define FRAM_HEADER_SIZE 16U
#define FRAM_MAGIC UINT32_C(0x314d5246) /* FRM1, little endian */
#define FRAM_FORMAT_VERSION 1U

typedef struct
{
    bool valid;
    uint32_t generation;
    uint8_t bytes[FRAM_SLOT_SIZE];
} FramSlot;

static uint16_t get_u16(const uint8_t *bytes)
{
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U);
}

static uint32_t get_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
}

static void put_u16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
}

static void put_u32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    bytes[2] = (uint8_t)(value >> 16U);
    bytes[3] = (uint8_t)(value >> 24U);
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *bytes, size_t length)
{
    size_t index;
    unsigned int bit;
    for (index = 0; index < length; index++)
    {
        crc ^= bytes[index];
        for (bit = 0; bit < 8U; bit++)
            crc = (crc >> 1U) ^ (UINT32_C(0xedb88320) & (uint32_t)-(int32_t)(crc & 1U));
    }
    return crc;
}

static uint32_t record_crc(const uint8_t *record, size_t payload_size)
{
    uint32_t crc = UINT32_MAX;
    crc = crc32_update(crc, record + 4U, 8U);
    crc = crc32_update(crc, record + FRAM_HEADER_SIZE, payload_size);
    return ~crc;
}

static bool read_exact(int descriptor, void *buffer, size_t length, off_t offset)
{
    uint8_t *bytes = buffer;
    size_t complete = 0;
    while (complete < length)
    {
        ssize_t count = pread(descriptor, bytes + complete, length - complete,
                              offset + (off_t)complete);
        if (count <= 0)
            return false;
        complete += (size_t)count;
    }
    return true;
}

static bool write_exact(int descriptor, const void *buffer, size_t length, off_t offset)
{
    const uint8_t *bytes = buffer;
    size_t complete = 0;
    while (complete < length)
    {
        ssize_t count = pwrite(descriptor, bytes + complete, length - complete,
                               offset + (off_t)complete);
        if (count <= 0)
            return false;
        complete += (size_t)count;
    }
    return true;
}

static bool generation_newer(uint32_t left, uint32_t right)
{
    return (int32_t)(left - right) > 0;
}

static void read_slot(int descriptor, unsigned int index, size_t payload_size, FramSlot *slot)
{
    uint32_t stored_crc;
    memset(slot, 0, sizeof(*slot));
    if (!read_exact(descriptor, slot->bytes, sizeof(slot->bytes),
                    (off_t)(index * FRAM_SLOT_SIZE)))
        return;
    if (get_u32(slot->bytes) != FRAM_MAGIC ||
        get_u16(slot->bytes + 4U) != FRAM_FORMAT_VERSION ||
        get_u16(slot->bytes + 6U) != payload_size)
        return;
    stored_crc = get_u32(slot->bytes + 12U);
    if (stored_crc != record_crc(slot->bytes, payload_size))
        return;
    slot->generation = get_u32(slot->bytes + 8U);
    slot->valid = true;
}

static int newest_slot(const FramSlot slots[FRAM_SLOT_COUNT])
{
    if (!slots[0].valid)
        return slots[1].valid ? 1 : -1;
    if (!slots[1].valid)
        return 0;
    return generation_newer(slots[1].generation, slots[0].generation) ? 1 : 0;
}

bool fram_state_load(const char *path, void *payload, size_t payload_size)
{
    FramSlot slots[FRAM_SLOT_COUNT];
    int descriptor;
    int newest;
    if (path == NULL || payload == NULL || payload_size > FRAM_STATE_MAX_PAYLOAD)
        return false;
    descriptor = open(path, O_RDONLY);
    if (descriptor < 0)
        return false;
    read_slot(descriptor, 0U, payload_size, &slots[0]);
    read_slot(descriptor, 1U, payload_size, &slots[1]);
    close(descriptor);
    newest = newest_slot(slots);
    if (newest < 0)
        return false;
    memcpy(payload, slots[newest].bytes + FRAM_HEADER_SIZE, payload_size);
    return true;
}

bool fram_state_save(const char *path, const void *payload, size_t payload_size)
{
    FramSlot slots[FRAM_SLOT_COUNT];
    uint8_t record[FRAM_SLOT_SIZE];
    uint8_t verify[FRAM_SLOT_SIZE];
    uint32_t generation = 1U;
    int descriptor;
    int newest;
    unsigned int target;
    bool ok;
    if (path == NULL || payload == NULL || payload_size > FRAM_STATE_MAX_PAYLOAD)
        return false;
    descriptor = open(path, O_RDWR);
    if (descriptor < 0)
        return false;
    read_slot(descriptor, 0U, payload_size, &slots[0]);
    read_slot(descriptor, 1U, payload_size, &slots[1]);
    newest = newest_slot(slots);
    if (newest >= 0)
        generation = slots[newest].generation + 1U;
    target = newest == 0 ? 1U : 0U;

    memset(record, 0xff, sizeof(record));
    put_u32(record, FRAM_MAGIC);
    put_u16(record + 4U, FRAM_FORMAT_VERSION);
    put_u16(record + 6U, (uint16_t)payload_size);
    put_u32(record + 8U, generation);
    memcpy(record + FRAM_HEADER_SIZE, payload, payload_size);
    put_u32(record + 12U, record_crc(record, payload_size));

    ok = write_exact(descriptor, record, sizeof(record),
                     (off_t)(target * FRAM_SLOT_SIZE));
    if (ok)
        ok = read_exact(descriptor, verify, sizeof(verify),
                        (off_t)(target * FRAM_SLOT_SIZE));
    close(descriptor);
    return ok && memcmp(record, verify, sizeof(record)) == 0;
}

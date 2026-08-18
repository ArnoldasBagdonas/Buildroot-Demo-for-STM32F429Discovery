#define _XOPEN_SOURCE 700

#include "spinor_state.h"

#include <fcntl.h>
#include <mtd/mtd-user.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#define SPINOR_RECORD_SIZE 256U
#define SPINOR_HEADER_SIZE 16U
#define SPINOR_MAGIC UINT32_C(0x31524f4e) /* NOR1, little endian */
#define SPINOR_FORMAT_VERSION 1U

typedef struct
{
    bool valid;
    uint32_t generation;
    uint8_t bytes[SPINOR_RECORD_SIZE];
} SpinorSlot;

typedef struct
{
    int descriptor;
    uint32_t erase_size;
    uint64_t device_size;
    bool regular_file;
} SpinorDevice;

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
    crc = crc32_update(crc, record + SPINOR_HEADER_SIZE, payload_size);
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

static bool open_device(const char *path, int flags, SpinorDevice *device)
{
    mtd_info_t info;
#ifdef SPINOR_STATE_TEST
    struct stat status;
#endif
    memset(device, 0, sizeof(*device));
    device->descriptor = open(path, flags);
    if (device->descriptor < 0)
        return false;
    if (ioctl(device->descriptor, MEMGETINFO, &info) == 0)
    {
        if (info.type != MTD_NORFLASH || info.erasesize < SPINOR_RECORD_SIZE)
            goto fail;
        device->erase_size = info.erasesize;
        device->device_size = info.size;
    }
#ifdef SPINOR_STATE_TEST
    else if (fstat(device->descriptor, &status) == 0 && S_ISREG(status.st_mode))
    {
        device->erase_size = 4096U;
        device->device_size = (uint64_t)status.st_size;
        device->regular_file = true;
    }
#endif
    else
    {
        goto fail;
    }
    if (device->device_size < (uint64_t)device->erase_size * 2U)
        goto fail;
    return true;

fail:
    close(device->descriptor);
    device->descriptor = -1;
    return false;
}

static bool generation_newer(uint32_t left, uint32_t right)
{
    return (int32_t)(left - right) > 0;
}

static void read_slot(const SpinorDevice *device, unsigned int index,
                      size_t payload_size, SpinorSlot *slot)
{
    uint32_t stored_crc;
    memset(slot, 0, sizeof(*slot));
    if (!read_exact(device->descriptor, slot->bytes, sizeof(slot->bytes),
                    (off_t)((uint64_t)index * device->erase_size)))
        return;
    if (get_u32(slot->bytes) != SPINOR_MAGIC ||
        get_u16(slot->bytes + 4U) != SPINOR_FORMAT_VERSION ||
        get_u16(slot->bytes + 6U) != payload_size)
        return;
    stored_crc = get_u32(slot->bytes + 12U);
    if (stored_crc != record_crc(slot->bytes, payload_size))
        return;
    slot->generation = get_u32(slot->bytes + 8U);
    slot->valid = true;
}

static int newest_slot(const SpinorSlot slots[2])
{
    if (!slots[0].valid)
        return slots[1].valid ? 1 : -1;
    if (!slots[1].valid)
        return 0;
    return generation_newer(slots[1].generation, slots[0].generation) ? 1 : 0;
}

static bool erase_slot(const SpinorDevice *device, unsigned int index)
{
    erase_info_t erase;
    erase.start = index * device->erase_size;
    erase.length = device->erase_size;
#ifdef SPINOR_STATE_TEST
    if (device->regular_file)
    {
        uint8_t erased[256];
        uint32_t offset;
        memset(erased, 0xff, sizeof(erased));
        for (offset = 0; offset < device->erase_size; offset += sizeof(erased))
            if (!write_exact(device->descriptor, erased, sizeof(erased),
                             (off_t)erase.start + offset))
                return false;
        return true;
    }
#endif
    return ioctl(device->descriptor, MEMERASE, &erase) == 0;
}

bool spinor_state_load(const char *path, void *payload, size_t payload_size)
{
    SpinorDevice device;
    SpinorSlot slots[2];
    int newest;
    if (path == NULL || payload == NULL || payload_size > SPINOR_STATE_MAX_PAYLOAD)
        return false;
    if (!open_device(path, O_RDONLY, &device))
        return false;
    read_slot(&device, 0U, payload_size, &slots[0]);
    read_slot(&device, 1U, payload_size, &slots[1]);
    close(device.descriptor);
    newest = newest_slot(slots);
    if (newest < 0)
        return false;
    memcpy(payload, slots[newest].bytes + SPINOR_HEADER_SIZE, payload_size);
    return true;
}

bool spinor_state_save(const char *path, const void *payload, size_t payload_size)
{
    SpinorDevice device;
    SpinorSlot slots[2];
    uint8_t record[SPINOR_RECORD_SIZE];
    uint8_t verify[SPINOR_RECORD_SIZE];
    uint32_t generation = 1U;
    int newest;
    unsigned int target;
    bool ok;
    if (path == NULL || payload == NULL || payload_size > SPINOR_STATE_MAX_PAYLOAD)
        return false;
    if (!open_device(path, O_RDWR | O_SYNC, &device))
        return false;
    read_slot(&device, 0U, payload_size, &slots[0]);
    read_slot(&device, 1U, payload_size, &slots[1]);
    newest = newest_slot(slots);
    if (newest >= 0)
        generation = slots[newest].generation + 1U;
    target = newest == 0 ? 1U : 0U;

    memset(record, 0xff, sizeof(record));
    put_u32(record, SPINOR_MAGIC);
    put_u16(record + 4U, SPINOR_FORMAT_VERSION);
    put_u16(record + 6U, (uint16_t)payload_size);
    put_u32(record + 8U, generation);
    memcpy(record + SPINOR_HEADER_SIZE, payload, payload_size);
    put_u32(record + 12U, record_crc(record, payload_size));

    ok = erase_slot(&device, target);
    if (ok)
        ok = write_exact(device.descriptor, record, sizeof(record),
                         (off_t)((uint64_t)target * device.erase_size));
    if (ok)
        ok = read_exact(device.descriptor, verify, sizeof(verify),
                        (off_t)((uint64_t)target * device.erase_size));
    close(device.descriptor);
    return ok && memcmp(record, verify, sizeof(record)) == 0;
}

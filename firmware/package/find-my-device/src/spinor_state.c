#define _XOPEN_SOURCE 700

#include "spinor_state.h"

#include <fcntl.h>
#include <mtd/mtd-user.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#define SPINOR_MAGIC UINT32_C(0x31524f4e) /* NOR1, little endian */

typedef struct
{
    int descriptor;
    uint32_t erase_size;
    uint64_t device_size;
    bool regular_file;
} SpinorDevice;

static bool read_exact(int descriptor, void *buffer, size_t length, off_t offset)
{
    uint8_t *bytes = buffer;
    size_t complete = 0U;
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
    size_t complete = 0U;
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
        if (info.type != MTD_NORFLASH || info.erasesize < STATE_RECORD_SIZE)
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
    if (device->device_size < (uint64_t)device->erase_size * STATE_RECORD_SLOT_COUNT)
        goto fail;
    return true;

fail:
    close(device->descriptor);
    device->descriptor = -1;
    return false;
}

static void read_slot(const SpinorDevice *device, unsigned int index,
                      size_t payload_size, StateRecordSlot *slot)
{
    memset(slot, 0, sizeof(*slot));
    if (!read_exact(device->descriptor, slot->bytes, sizeof(slot->bytes),
                    (off_t)((uint64_t)index * device->erase_size)))
        return;
    state_record_parse(slot, SPINOR_MAGIC, payload_size);
}

static bool erase_slot(const SpinorDevice *device, unsigned int index)
{
    erase_info_t erase;
    erase.start = index * device->erase_size;
    erase.length = device->erase_size;
#ifdef SPINOR_STATE_TEST
    if (device->regular_file)
    {
        uint8_t erased[STATE_RECORD_SIZE];
        uint32_t offset;
        memset(erased, 0xff, sizeof(erased));
        for (offset = 0U; offset < device->erase_size; offset += sizeof(erased))
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
    StateRecordSlot slots[STATE_RECORD_SLOT_COUNT];
    int newest;
    if (path == NULL || payload == NULL || payload_size > SPINOR_STATE_MAX_PAYLOAD)
        return false;
    if (!open_device(path, O_RDONLY, &device))
        return false;
    read_slot(&device, 0U, payload_size, &slots[0]);
    read_slot(&device, 1U, payload_size, &slots[1]);
    close(device.descriptor);
    newest = state_record_newest(slots);
    if (newest < 0)
        return false;
    state_record_copy_payload(&slots[newest], payload, payload_size);
    return true;
}

bool spinor_state_save(const char *path, const void *payload, size_t payload_size)
{
    SpinorDevice device;
    StateRecordSlot slots[STATE_RECORD_SLOT_COUNT];
    uint8_t record[STATE_RECORD_SIZE];
    uint8_t verify[STATE_RECORD_SIZE];
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
    newest = state_record_newest(slots);
    if (newest >= 0)
        generation = slots[newest].generation + 1U;
    target = newest == 0 ? 1U : 0U;

    state_record_build(record, SPINOR_MAGIC, generation, payload, payload_size);
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

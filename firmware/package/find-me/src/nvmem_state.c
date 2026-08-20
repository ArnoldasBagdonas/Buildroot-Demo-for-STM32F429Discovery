#define _XOPEN_SOURCE 700

#include "nvmem_state.h"

#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define NVMEM_MAGIC UINT32_C(0x314d564e) /* NVM1, little endian */

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

static void read_slot(int descriptor, unsigned int index, size_t payload_size,
                      StateRecordSlot *slot)
{
    memset(slot, 0, sizeof(*slot));
    if (!read_exact(descriptor, slot->bytes, sizeof(slot->bytes),
                    (off_t)(index * STATE_RECORD_SIZE)))
        return;
    state_record_parse(slot, NVMEM_MAGIC, payload_size);
}

bool nvmem_state_load(const char *path, void *payload, size_t payload_size)
{
    StateRecordSlot slots[STATE_RECORD_SLOT_COUNT];
    int descriptor;
    int newest;
    if (path == NULL || payload == NULL || payload_size > NVMEM_STATE_MAX_PAYLOAD)
        return false;
    descriptor = open(path, O_RDONLY);
    if (descriptor < 0)
        return false;
    read_slot(descriptor, 0U, payload_size, &slots[0]);
    read_slot(descriptor, 1U, payload_size, &slots[1]);
    close(descriptor);
    newest = state_record_newest(slots);
    if (newest < 0)
        return false;
    state_record_copy_payload(&slots[newest], payload, payload_size);
    return true;
}

bool nvmem_state_save(const char *path, const void *payload, size_t payload_size)
{
    StateRecordSlot slots[STATE_RECORD_SLOT_COUNT];
    uint8_t record[STATE_RECORD_SIZE];
    uint8_t verify[STATE_RECORD_SIZE];
    uint32_t generation = 1U;
    int descriptor;
    int newest;
    unsigned int target;
    bool ok;
    if (path == NULL || payload == NULL || payload_size > NVMEM_STATE_MAX_PAYLOAD)
        return false;
    descriptor = open(path, O_RDWR | O_SYNC);
    if (descriptor < 0)
        return false;
    read_slot(descriptor, 0U, payload_size, &slots[0]);
    read_slot(descriptor, 1U, payload_size, &slots[1]);
    newest = state_record_newest(slots);
    if (newest >= 0)
        generation = slots[newest].generation + 1U;
    target = newest == 0 ? 1U : 0U;

    state_record_build(record, NVMEM_MAGIC, generation, payload, payload_size);
    ok = write_exact(descriptor, record, sizeof(record),
                     (off_t)(target * STATE_RECORD_SIZE));
    if (ok)
        ok = read_exact(descriptor, verify, sizeof(verify),
                        (off_t)(target * STATE_RECORD_SIZE));
    close(descriptor);
    return ok && memcmp(record, verify, sizeof(record)) == 0;
}

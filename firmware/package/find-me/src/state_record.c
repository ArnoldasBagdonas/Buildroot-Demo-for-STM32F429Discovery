#include "state_record.h"

#include <string.h>

#define STATE_RECORD_FORMAT_VERSION 1U

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
            crc = (crc >> 1U) ^ (UINT32_C(0xedb88320) &
                                  (uint32_t)-(int32_t)(crc & 1U));
    }
    return crc;
}

static uint32_t record_crc(const uint8_t *record, size_t payload_size)
{
    uint32_t crc = UINT32_MAX;
    crc = crc32_update(crc, record + 4U, 8U);
    crc = crc32_update(crc, record + STATE_RECORD_HEADER_SIZE, payload_size);
    return ~crc;
}

void state_record_parse(StateRecordSlot *slot, uint32_t magic, size_t payload_size)
{
    uint32_t stored_crc;
    slot->valid = false;
    slot->generation = 0U;
    if (payload_size > STATE_RECORD_MAX_PAYLOAD ||
        get_u32(slot->bytes) != magic ||
        get_u16(slot->bytes + 4U) != STATE_RECORD_FORMAT_VERSION ||
        get_u16(slot->bytes + 6U) != payload_size)
        return;
    stored_crc = get_u32(slot->bytes + 12U);
    if (stored_crc != record_crc(slot->bytes, payload_size))
        return;
    slot->generation = get_u32(slot->bytes + 8U);
    slot->valid = true;
}

static bool generation_newer(uint32_t left, uint32_t right)
{
    return (int32_t)(left - right) > 0;
}

int state_record_newest(const StateRecordSlot slots[STATE_RECORD_SLOT_COUNT])
{
    if (!slots[0].valid)
        return slots[1].valid ? 1 : -1;
    if (!slots[1].valid)
        return 0;
    return generation_newer(slots[1].generation, slots[0].generation) ? 1 : 0;
}

void state_record_build(uint8_t record[STATE_RECORD_SIZE], uint32_t magic,
                        uint32_t generation, const void *payload, size_t payload_size)
{
    memset(record, 0xff, STATE_RECORD_SIZE);
    put_u32(record, magic);
    put_u16(record + 4U, STATE_RECORD_FORMAT_VERSION);
    put_u16(record + 6U, (uint16_t)payload_size);
    put_u32(record + 8U, generation);
    memcpy(record + STATE_RECORD_HEADER_SIZE, payload, payload_size);
    put_u32(record + 12U, record_crc(record, payload_size));
}

void state_record_copy_payload(const StateRecordSlot *slot, void *payload,
                               size_t payload_size)
{
    memcpy(payload, slot->bytes + STATE_RECORD_HEADER_SIZE, payload_size);
}

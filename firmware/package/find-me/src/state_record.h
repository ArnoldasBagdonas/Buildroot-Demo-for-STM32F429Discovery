#ifndef STATE_RECORD_H
#define STATE_RECORD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STATE_RECORD_SIZE 256U
#define STATE_RECORD_HEADER_SIZE 16U
#define STATE_RECORD_MAX_PAYLOAD (STATE_RECORD_SIZE - STATE_RECORD_HEADER_SIZE)
#define STATE_RECORD_SLOT_COUNT 2U

typedef struct
{
    bool valid;
    uint32_t generation;
    uint8_t bytes[STATE_RECORD_SIZE];
} StateRecordSlot;

void state_record_parse(StateRecordSlot *slot, uint32_t magic, size_t payload_size);
int state_record_newest(const StateRecordSlot slots[STATE_RECORD_SLOT_COUNT]);
void state_record_build(uint8_t record[STATE_RECORD_SIZE], uint32_t magic,
                        uint32_t generation, const void *payload, size_t payload_size);
void state_record_copy_payload(const StateRecordSlot *slot, void *payload,
                               size_t payload_size);

#endif

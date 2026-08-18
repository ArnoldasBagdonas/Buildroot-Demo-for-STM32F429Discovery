#ifndef NVMEM_STATE_H
#define NVMEM_STATE_H

#include "state_record.h"

#include <stdbool.h>
#include <stddef.h>

#define NVMEM_STATE_MAX_PAYLOAD STATE_RECORD_MAX_PAYLOAD

/* Two alternating records occupy the first 512 bytes of byte-addressable NVMEM. */
bool nvmem_state_load(const char *path, void *payload, size_t payload_size);
bool nvmem_state_save(const char *path, const void *payload, size_t payload_size);

#endif

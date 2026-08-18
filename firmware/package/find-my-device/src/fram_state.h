#ifndef FRAM_STATE_H
#define FRAM_STATE_H

#include <stdbool.h>
#include <stddef.h>

#define FRAM_STATE_MAX_PAYLOAD 240U

/*
 * Store alternating, checksummed records in the first 512 bytes of an NVMEM
 * device. The remaining CY15B256Q capacity is left available to other users.
 */
bool fram_state_load(const char *path, void *payload, size_t payload_size);
bool fram_state_save(const char *path, const void *payload, size_t payload_size);

#endif

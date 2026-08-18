#ifndef SPINOR_STATE_H
#define SPINOR_STATE_H

#include <stdbool.h>
#include <stddef.h>

#define SPINOR_STATE_MAX_PAYLOAD 240U

/* Store alternating records in the first two eraseblocks of an MTD partition. */
bool spinor_state_load(const char *path, void *payload, size_t payload_size);
bool spinor_state_save(const char *path, const void *payload, size_t payload_size);

#endif

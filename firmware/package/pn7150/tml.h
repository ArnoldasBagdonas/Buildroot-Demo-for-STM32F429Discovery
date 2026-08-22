// SPDX-License-Identifier: MIT
#ifndef PN7150_TML_H
#define PN7150_TML_H

#include <stdint.h>

#define TIMEOUT_INFINITE 0
#define TIMEOUT_100MS 100
#define TIMEOUT_1S 1000
#define TIMEOUT_2S 2000

void tml_Connect(void);
void tml_Disconnect(void);
void tml_Send(uint8_t *buffer, uint16_t length, uint16_t *bytes_sent);
void tml_Receive(uint8_t *buffer, uint16_t capacity, uint16_t *bytes_read,
		 uint16_t timeout_ms);

#endif

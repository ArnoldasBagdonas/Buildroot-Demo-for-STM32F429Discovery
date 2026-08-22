// SPDX-License-Identifier: MIT

#include <errno.h>
#include <poll.h>

#include "tool.h"

void Sleep(unsigned int milliseconds)
{
	int result;

	do {
		result = poll(NULL, 0, (int)milliseconds);
	} while (result < 0 && errno == EINTR);
}

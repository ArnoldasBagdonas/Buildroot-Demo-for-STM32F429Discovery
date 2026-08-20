CC ?= cc
CFLAGS ?= -O2
CPPFLAGS ?=
LDFLAGS ?=

TARGET := find-me
STATE_SOURCES ?=
SOURCES := find_me.c device_info.c oauth_service.c mdns_codec.c $(STATE_SOURCES)
OBJECTS := $(SOURCES:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 -Wall -Wextra -c -o $@ $<

clean:
	rm -f $(TARGET) $(TARGET).gdb $(OBJECTS) nvmem_state.o spinor_state.o state_record.o

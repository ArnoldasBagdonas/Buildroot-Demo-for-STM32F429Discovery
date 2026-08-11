CC ?= cc
CFLAGS ?= -O2 -g
CPPFLAGS ?=

SOURCE_DIR := ../src
COMMON_FLAGS := $(CPPFLAGS) $(CFLAGS) -std=c11 -Wall -Wextra -Werror -I$(SOURCE_DIR)
TESTS := device_info_test oauth_service_test mdns_codec_test

.PHONY: all check integration clean

all: $(TESTS)

device_info_test: device_info_test.c $(SOURCE_DIR)/device_info.c
	$(CC) $(COMMON_FLAGS) -o $@ $^

oauth_service_test: oauth_service_test.c $(SOURCE_DIR)/oauth_service.c
	$(CC) $(COMMON_FLAGS) -o $@ $^

mdns_codec_test: mdns_codec_test.c $(SOURCE_DIR)/mdns_codec.c
	$(CC) $(COMMON_FLAGS) -o $@ $^

check: all
	./device_info_test
	./oauth_service_test
	./mdns_codec_test

integration:
	@set -eu; \
	$(MAKE) -C $(SOURCE_DIR) clean all \
		CC="$(CC)" CFLAGS="$(CFLAGS) -Wall -Wextra -Werror"; \
	trap '$(MAKE) -C $(SOURCE_DIR) clean' EXIT; \
	FMD_BINARY=$(abspath $(SOURCE_DIR)/find-my-device) ./integration_test.py

clean:
	rm -f $(TESTS)

# Makefile for a mixed C project

# Target executable name
TARGET ?= ioexample2

# Build configuration flags
DEBUG_FLAGS    = -O0 -g -Wall
RELEASE_FLAGS  = -O2
MINISIZE_FLAGS = -Os -fdata-sections -ffunction-sections

# Directory structure
SRC_DIR = src
BIN_DIR = bin
OBJ_DIR = $(BIN_DIR)/obj

# Include directory for headers
INCLUDES = -I./include

# Source and object file discovery
SRC := $(wildcard $(SRC_DIR)/**/*.c) $(wildcard $(SRC_DIR)/*.c)
OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))

# Compiler settings
CC      ?= gcc
CFLAGS  ?= $(MINISIZE_FLAGS)
LDFLAGS ?= $(CFLAGS)

# Default target
all: $(BIN_DIR)/$(TARGET) copy-config

# Link object files into target executable
$(BIN_DIR)/$(TARGET): $(OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

# Compile source files into object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Configuration-based builds
debug: CFLAGS := $(DEBUG_FLAGS)
debug: LDFLAGS := $(DEBUG_FLAGS)
debug: all

release: CFLAGS := $(RELEASE_FLAGS)
release: LDFLAGS := $(RELEASE_FLAGS)
release: all

minisize: CFLAGS := $(MINISIZE_FLAGS)
minisize: LDFLAGS := $(MINISIZE_FLAGS)
minisize: all

# Copy configuration or other assets (placeholder)
copy-config:
	@mkdir -p $(BIN_DIR)

# Clean targets
clean:
	rm -f $(OBJ) $(BIN_DIR)/$(TARGET)

distclean: clean
	rm -rf $(BIN_DIR)

# Run the program
run: $(BIN_DIR)/$(TARGET)
	./$(BIN_DIR)/$(TARGET)

# Show info
info:
	@echo "[*] Source dir:  $(SRC_DIR)"
	@echo "[*] Object dir:  $(OBJ_DIR)"
	@echo "[*] Sources:     $(SRC)"
	@echo "[*] Objects:     $(OBJ)"
	@echo "[*] Target:      $(BIN_DIR)/$(TARGET)"

# Phony targets
.PHONY: all run debug release minisize clean distclean info copy-config

# Makefile for a mixed C project

# Target executable name
TARGET ?= hellomk

# Configuration file for the target
CONFIG_FILE ?= $(TARGET).ini

# Build configurations000

DEBUG_FLAGS     = -O0 -g -Wall
RELEASE_FLAGS   = -O2
MINISIZE_FLAGS  = -Os -fdata-sections -ffunction-sections

# Directory structure
SRC_DIR = src
BIN_DIR = bin
OBJ_DIR = $(BIN_DIR)/obj

# Include directory for headers
INCLUDES = -I./include

# Find source files
SRC := $(wildcard $(SRC_DIR)/**/*.c) $(wildcard $(SRC_DIR)/*.c)

# Convert source files to object files
OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))

# Compiler settings
CC ?= gcc

# Default compiler flags
CFLAGS   ?= $(MINISIZE_FLAGS)

# Linker flags default to CXXFLAGS for mixed C/C++
LDFLAGS  ?= $(CFLAGS)

# Optional preprocessor defines
DEFINES = -DCONFIG_FILE="\"$(CONFIG_FILE)\""

# Default target
all: $(BIN_DIR)/$(TARGET) copy-config

# Link object files into the target executable
# Create binary directory if it doesn't exist
$(BIN_DIR)/$(TARGET): $(OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

# Compile C source files into object files
# Create object directory if it doesn't exist
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

# Targets for specific build configurations
debug: CFLAGS := $(DEBUG_FLAGS)
debug: LDFLAGS := $(DEBUG_FLAGS)
debug: DEFINES += -DDEBUG
debug: all

release: CFLAGS := $(RELEASE_FLAGS)
release: LDFLAGS := $(RELEASE_FLAGS)
release: all

minisize: CFLAGS := $(MINISIZE_FLAGS)
minisize: LDFLAGS := $(MINISIZE_FLAGS)
minisize: all

# Copy configuration file to the binary directory
# Ensure the binary directory exists
copy-config:
	@mkdir -p $(BIN_DIR)
	@cp $(CONFIG_FILE) $(BIN_DIR)/

# Clean up object files and the binary
clean:
	rm -f $(OBJ) $(BIN_DIR)/$(TARGET)

# Full clean including the entire binary directory
distclean: clean
	rm -rf $(BIN_DIR)

# Run the built program
run: $(BIN_DIR)/$(TARGET)
	./$(BIN_DIR)/$(TARGET)

# Display useful build information
info:
	@echo "[*] Source dir:      $(SRC_DIR)"
	@echo "[*] Object dir:      $(OBJ_DIR)"
	@echo "[*] Sources:         $(SRC)"
	@echo "[*] Objects:         $(OBJ)"
	@echo "[*] Target:          $(BIN_DIR)/$(TARGET)"

# Declare phony targets to avoid conflicts with actual file names
.PHONY: all run debug release minisize clean distclean info copy-config


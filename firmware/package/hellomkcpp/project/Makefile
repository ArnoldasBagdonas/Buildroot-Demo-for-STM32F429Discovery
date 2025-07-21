# Makefile for a mixed C/C++ project

# Target executable name
TARGET ?= hellomkcpp

# Configuration file for the target
CONFIG_FILE ?= $(TARGET).ini

# Build configurations000

DEBUG_FLAGS     = -g -Wall
RELEASE_FLAGS   = -O3
MINISIZE_FLAGS  = -Os -fdata-sections -ffunction-sections

# Directory structure
SRC_DIR = src
BIN_DIR = bin
OBJ_DIR = $(BIN_DIR)/obj

# Include directory for headers
INCLUDES = -I./include

# Find source files
SRC_CPP := $(wildcard $(SRC_DIR)/**/*.cpp) $(wildcard $(SRC_DIR)/*.cpp)
SRC_C   := $(wildcard $(SRC_DIR)/**/*.c) $(wildcard $(SRC_DIR)/*.c)

# Convert source files to object files
OBJ_CPP := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRC_CPP))
OBJ_C   := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC_C))

# Combined object files list
OBJ = $(OBJ_C) $(OBJ_CPP)

# Compiler settings
CXX ?= g++
CC ?= gcc

# Default compiler flags
CXXFLAGS ?= $(MINISIZE_FLAGS)
CFLAGS   ?= $(CXXFLAGS)

# Linker flags default to CXXFLAGS for mixed C/C++
LDFLAGS  ?= $(CXXFLAGS)

# Optional preprocessor defines
DEFINES = -DCONFIG_FILE="\"$(CONFIG_FILE)\""

# Default target
all: $(BIN_DIR)/$(TARGET) copy-config

# Link object files into the target executable
# Create binary directory if it doesn't exist
$(BIN_DIR)/$(TARGET): $(OBJ)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $@ $^ $(LDFLAGS)

# Compile C++ source files into object files
# Create object directory if it doesn't exist
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

# Compile C source files into object files
# Create object directory if it doesn't exist
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

# Targets for specific build configurations
debug: CFLAGS := $(DEBUG_FLAGS)
debug: CXXFLAGS := $(DEBUG_FLAGS)
debug: LDFLAGS := $(DEBUG_FLAGS)
debug: DEFINES += -DDEBUG
debug: all

release: CFLAGS := $(RELEASE_FLAGS)
release: CXXFLAGS := $(RELEASE_FLAGS)
release: LDFLAGS := $(RELEASE_FLAGS)
release: all

minisize: CFLAGS := $(MINISIZE_FLAGS)
minisize: CXXFLAGS := $(MINISIZE_FLAGS)
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
	@echo "[*] C++ Sources:     $(SRC_CPP)"
	@echo "[*] C Sources:       $(SRC_C)"
	@echo "[*] Objects:         $(OBJ)"
	@echo "[*] Target:          $(BIN_DIR)/$(TARGET)"

# Declare phony targets to avoid conflicts with actual file names
.PHONY: all run debug release minisize clean distclean info copy-config

# Project Configuration
TARGET      := build/ssg
SRC_DIR     := src
OBJ_DIR     := build

# Architecture Detection
UNAME_M := $(shell uname -m)

# File Discovery
SOURCES     := $(shell find $(SRC_DIR) -type f -name '*.c')
OBJECTS     := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))

# Compiler Configuration
CC          := gcc
BASE_CFLAGS := -Wall -Wextra -Werror -pedantic -std=c11 -O3 \
               -Iinclude -DREPORT_INTERVAL=0.5 \
               -I/opt/homebrew/Cellar/cmark/0.31.1/include/ \
               -Xpreprocessor -fopenmp -Wno-pedantic \
               -I/opt/homebrew/Cellar/libomp/19.1.7/include/

# Architecture-Specific Flags
ifeq ($(UNAME_M),x86_64)
  SIMD_FLAGS := -mavx2 -mfma -march=native -DARCH_X86
else ifeq ($(UNAME_M),arm64)
  SIMD_FLAGS := -march=armv8-a+simd -DARCH_ARM -DNEON_ENABLED
endif

LDFLAGS     := -lm $(shell pkg-config --libs libcmark) \
               -L/opt/homebrew/opt/libomp/lib -lomp \
               -L/opt/homebrew/Cellar/cmark/0.31.1/lib

# Combine Flags
CFLAGS := $(BASE_CFLAGS) $(SIMD_FLAGS)

# Development vs Release
ifeq ($(DEBUG),1)
    CFLAGS += -g -DDEBUG
else
    CFLAGS += -DNDEBUG
endif

# Main Targets
all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "Linking $@ (Arch: $(UNAME_M))"
	@$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $< (Arch: $(UNAME_M))"
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c $< -o $@

# Utility Targets
clean:
	@echo "Cleaning build artifacts"
	@rm -rf $(OBJ_DIR) $(TARGET) public/* .cssg_cache test_files/output

run: $(TARGET)
	@./$(TARGET)

test: $(TARGET)
	@./$(TARGET) test_config.yaml

help:
	@echo "Available targets:"
	@echo "  all       - Build project (default)"
	@echo "  clean     - Remove build artifacts"
	@echo "  run       - Build and run the program"
	@echo "  test      - Run test build"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Flags:"
	@echo "  DEBUG=1   - Build with debug symbols"
	@echo "  UNAME_M   - Detected architecture: $(UNAME_M)"
	@echo "  SIMD      - Active SIMD flags: $(SIMD_FLAGS)"

.PHONY: all clean run test help
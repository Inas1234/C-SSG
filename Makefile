# Project Configuration
TARGET      := build/ssg
SRC_DIR     := src
OBJ_DIR     := build

# File Discovery
SOURCES     := $(shell find $(SRC_DIR) -type f -name '*.c')
OBJECTS     := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))

# Compiler Configuration
CC          := gcc
CFLAGS      := -Wall -Wextra -Werror -pedantic -std=c11 -O2 -Iinclude -I/opt/homebrew/Cellar/cmark/0.31.1/include/ -DREPORT_INTERVAL=0.5 \
				-Xpreprocessor -fopenmp  -Wno-pedantic \
				-I/opt/homebrew/Cellar/libomp/19.1.7/include/ 
LDFLAGS     := -lm $(shell pkg-config --libs libcmark) -L/opt/homebrew/opt/libomp/lib -lomp \
          -L/opt/homebrew/Cellar/cmark/0.31.1/lib \

# Development vs Release
ifeq ($(DEBUG),1)
    CFLAGS += -g -DDEBUG
else
    CFLAGS += -DNDEBUG
endif

# Main Targets
all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "Linking $@"
	@$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<"
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c $< -o $@

# Utility Targets
clean:
	@echo "Cleaning build artifacts"
	@rm -rf $(OBJ_DIR) $(TARGET) public/* .cssg_cache test_files/output

run: $(TARGET)
	@./$(TARGET)


test: $(TARGET)
	@./$(TARGET) test_files/content test_files/output

help:
	@echo "Available targets:"
	@echo "  all       - Build project (default)"
	@echo "  clean     - Remove build artifacts"
	@echo "  run       - Build and run the program"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Flags:"
	@echo "  DEBUG=1   - Build with debug symbols"

.PHONY: all clean run help
# ScarlettOS Driver Makefile Template

# Driver name
DRIVER_NAME = example_driver

# Source files
SOURCES = driver_template.c

# SDK path
SDK_PATH ?= /usr/local/scarlettos-sdk

# Include directories
INCLUDES = -I$(SDK_PATH)/include

# Libraries
LIBS = -lscarlettos

# Compiler flags
CFLAGS = -Wall -Wextra -O2 -std=c11 $(INCLUDES)
LDFLAGS = $(LIBS)

# Compiler
CC = gcc

# Output directory
BUILD_DIR = build

# Object files
OBJECTS = $(SOURCES:%.c=$(BUILD_DIR)/%.o)

# Default target
all: $(BUILD_DIR)/$(DRIVER_NAME)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile source files
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link driver
$(BUILD_DIR)/$(DRIVER_NAME): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Install driver
install: $(BUILD_DIR)/$(DRIVER_NAME)
	install -D $(BUILD_DIR)/$(DRIVER_NAME) /usr/local/drivers/$(DRIVER_NAME)

# Uninstall
uninstall:
	rm -f /usr/local/drivers/$(DRIVER_NAME)

.PHONY: all clean install uninstall

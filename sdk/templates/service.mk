# ScarlettOS Service Makefile Template

# Service name
SERVICE_NAME = example_service

# Source files
SOURCES = service_template.c

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
all: $(BUILD_DIR)/$(SERVICE_NAME)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile source files
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link service
$(BUILD_DIR)/$(SERVICE_NAME): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Install service
install: $(BUILD_DIR)/$(SERVICE_NAME)
	install -D $(BUILD_DIR)/$(SERVICE_NAME) /usr/local/services/$(SERVICE_NAME)

# Uninstall
uninstall:
	rm -f /usr/local/services/$(SERVICE_NAME)

# Run service (for testing)
run: $(BUILD_DIR)/$(SERVICE_NAME)
	$(BUILD_DIR)/$(SERVICE_NAME)

.PHONY: all clean install uninstall run

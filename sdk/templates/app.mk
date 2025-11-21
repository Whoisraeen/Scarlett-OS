# ScarlettOS Application Makefile Template
# Copy this file to your project directory and customize

# Application name
APP_NAME = myapp

# Source files
SOURCES = main.c

# Include directories
INCLUDES = -I$(SDK_PATH)/include

# Libraries to link
LIBS = -lscarlettos

# Compiler flags
CFLAGS = -Wall -Wextra -O2 -std=c11 $(INCLUDES)
LDFLAGS = -L$(SDK_PATH)/lib $(LIBS)

# SDK path (set this to your SDK installation)
SDK_PATH ?= /usr/local/scarlettos-sdk

# Compiler
CC = gcc

# Output directory
BUILD_DIR = build

# Object files
OBJECTS = $(SOURCES:%.c=$(BUILD_DIR)/%.o)

# Default target
all: $(BUILD_DIR)/$(APP_NAME)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile source files
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link executable
$(BUILD_DIR)/$(APP_NAME): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Install application
install: $(BUILD_DIR)/$(APP_NAME)
	install -D $(BUILD_DIR)/$(APP_NAME) /usr/local/bin/$(APP_NAME)

# Uninstall application
uninstall:
	rm -f /usr/local/bin/$(APP_NAME)

.PHONY: all clean install uninstall

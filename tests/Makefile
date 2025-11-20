# Test Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -ffreestanding -fno-stack-protector -mno-red-zone
INCLUDES = -I../kernel/include -I../libs/libc/include
LDFLAGS = -nostdlib -static

# Test sources
SERVICE_TESTS = integration/test_services.c
FILESYSTEM_TESTS = integration/test_filesystem.c
NETWORK_TESTS = integration/test_network.c
ALL_TESTS = integration/test_all.c

# Test objects
SERVICE_OBJS = $(SERVICE_TESTS:.c=.o)
FILESYSTEM_OBJS = $(FILESYSTEM_TESTS:.c=.o)
NETWORK_OBJS = $(NETWORK_TESTS:.c=.o)
ALL_OBJS = $(ALL_TESTS:.c=.o)

# Test binaries
SERVICE_BIN = build/test_services.bin
FILESYSTEM_BIN = build/test_filesystem.bin
NETWORK_BIN = build/test_network.bin
ALL_BIN = build/test_all.bin

.PHONY: all clean

all: $(ALL_BIN)

$(ALL_BIN): $(ALL_OBJS) $(SERVICE_OBJS) $(FILESYSTEM_OBJS) $(NETWORK_OBJS)
	@mkdir -p build
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -f $(SERVICE_OBJS) $(FILESYSTEM_OBJS) $(NETWORK_OBJS) $(ALL_OBJS)
	rm -f $(SERVICE_BIN) $(FILESYSTEM_BIN) $(NETWORK_BIN) $(ALL_BIN)


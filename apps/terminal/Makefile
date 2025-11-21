# Makefile for Scarlett OS Terminal Emulator

TARGET = terminal
SRCS = main.c terminal.c
OBJS = $(SRCS:.c=.o)

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -I../../gui -I../../kernel/include
LDFLAGS = -L../../gui/compositor -L../../gui/widgets -L../../gui/ugal
LIBS = -lcompositor -lwidgets -lugal

# Build target
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	mkdir -p /usr/bin
	cp $(TARGET) /usr/bin/

.PHONY: all clean install

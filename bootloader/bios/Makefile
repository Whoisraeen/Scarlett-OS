# BIOS Bootloader Makefile

AS = nasm
CC = gcc
LD = ld

ASFLAGS = -f bin
CFLAGS = -m16 -ffreestanding -fno-builtin -Wall -Wextra
LDFLAGS = -T linker.ld

TARGET = boot.bin

all: $(TARGET)

boot.bin: boot.S
	$(AS) $(ASFLAGS) -o $@ $<

stage2.bin: stage2.c
	$(CC) $(CFLAGS) -c -o stage2.o $<
	$(LD) $(LDFLAGS) -o $@ stage2.o

clean:
	rm -f *.o *.bin

.PHONY: all clean


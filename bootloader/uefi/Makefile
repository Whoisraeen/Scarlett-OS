# UEFI Bootloader Makefile

TARGET = BOOTX64.EFI
CC = gcc
LD = ld
OBJCOPY = objcopy

CFLAGS = -target x86_64-unknown-windows \
         -ffreestanding \
         -fno-stack-protector \
         -fno-stack-check \
         -fshort-wchar \
         -mno-red-zone \
         -Wall -Wextra \
         -I. -I../common \
         -DEFI_FUNCTION_WRAPPER

LDFLAGS = -target x86_64-unknown-windows \
          -nostdlib \
          -Wl,-entry:efi_main \
          -Wl,-subsystem:efi_application \
          -fuse-ld=lld

SRCS = main.c elf.c paging.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean


# UEFI Bootloader Makefile using GNU-EFI

TARGET = BOOTX64.EFI
GNUEFI_INC = /usr/include/efi
GNUEFI_LIB = /usr/lib

CC = gcc
LD = ld
OBJCOPY = objcopy

CFLAGS = -I$(GNUEFI_INC) \
         -I$(GNUEFI_INC)/x86_64 \
         -I$(GNUEFI_INC)/protocol \
         -I. -I../common \
         -ffreestanding \
         -fno-stack-protector \
         -fno-stack-check \
         -fshort-wchar \
         -mno-red-zone \
         -maccumulate-outgoing-args \
         -mno-sse \
         -mno-mmx \
         -mstackrealign \
         -fpic \
         -Wall -Wextra \
         -DGNU_EFI_USE_MS_ABI

LDFLAGS = -nostdlib \
          -znocombreloc \
          -T $(GNUEFI_LIB)/elf_x86_64_efi.lds \
          -shared \
          -Bsymbolic \
          -L $(GNUEFI_LIB) \
          $(GNUEFI_LIB)/crt0-efi-x86_64.o

LIBS = -lefi -lgnuefi

SRCS = main.c elf.c paging.c
OBJS = $(SRCS:.c=.o)
SO_TARGET = bootloader.so

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(SO_TARGET): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)

$(TARGET): $(SO_TARGET)
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym \
	           -j .rel -j .rela -j .reloc \
	           --target=efi-app-x86_64 --subsystem=10 $< $@

clean:
	rm -f $(OBJS) $(TARGET) $(SO_TARGET)

.PHONY: all clean

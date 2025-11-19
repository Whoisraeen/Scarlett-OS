# UEFI Bootloader Makefile

TARGET = BOOTX64.EFI
ELF_TARGET = bootloader.elf
CC = x86_64-w64-mingw32-gcc
OBJCOPY = x86_64-w64-mingw32-objcopy

# Try MinGW first (native PE), fall back to GNU tools
MINGW_AVAILABLE := $(shell which x86_64-w64-mingw32-gcc 2>/dev/null)

ifneq ($(MINGW_AVAILABLE),)
    # Use MinGW for native PE compilation
    CC = x86_64-w64-mingw32-gcc
    LD = x86_64-w64-mingw32-gcc
    OBJCOPY = x86_64-w64-mingw32-objcopy

    CFLAGS = -I. -I../common \
             -ffreestanding \
             -fno-stack-protector \
             -fno-stack-check \
             -fshort-wchar \
             -mno-red-zone \
             -DEFI_FUNCTION_WRAPPER \
             -Wall -Wextra

    LDFLAGS = -nostdlib \
              -Wl,-dll \
              -Wl,--subsystem,10 \
              -Wl,-e,efi_main \
              -Wl,--image-base,0 \
              -shared
else
    # Fall back to GCC + manual conversion
    CC = gcc
    LD = gcc
    OBJCOPY = objcopy

    CFLAGS = -I. -I../common \
             -ffreestanding \
             -fno-stack-protector \
             -fpie \
             -fshort-wchar \
             -mno-red-zone \
             -DEFI_FUNCTION_WRAPPER \
             -fno-strict-aliasing \
             -Wall -Wextra

    LDFLAGS = -nostdlib \
              -pie \
              -Wl,-Bsymbolic \
              -Wl,-T,elf_x86_64_efi.lds \
              -Wl,--no-undefined
endif

SRCS = main.c elf.c paging.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

ifneq ($(MINGW_AVAILABLE),)
# MinGW path - direct PE output
$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@
else
# GNU path - ELF to PE conversion
$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(ELF_TARGET)
	$(OBJCOPY) -j .text -j .sdata -j .data -j .rodata \
	           -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc \
	           --target=efi-app-x86_64 --subsystem=10 \
	           $(ELF_TARGET) $@
endif

clean:
	rm -f $(OBJS) $(TARGET) $(ELF_TARGET)

.PHONY: all clean

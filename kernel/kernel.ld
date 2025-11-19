/**
 * Kernel Linker Script for x86_64
 * 
 * Key: First LOAD segment must include multiboot header!
 */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(uefi_entry)

SECTIONS
{
    /* Start at 1MB - this is where GRUB loads us */
    . = 0x100000;
    
    _boot_start = .;

    /* Multiboot2 header - MUST be in first LOAD segment */
    .multiboot ALIGN(8) :
    {
        KEEP(*(.multiboot))
    }

    /* 32-bit boot code immediately after */
    .boot ALIGN(16) :
    {
        *(.boot)
    }

    /* Boot data (page tables, stack) */
    .boot.data ALIGN(4096) :
    {
        *(.boot.data)
    }

    _boot_end = .;
    _kernel_start = .;

    /* 64-bit kernel code - SAME SEGMENT */
    .text ALIGN(4096) :
    {
        *(.text)
        *(.text.*)
    }

    /* Read-only data */
    .rodata ALIGN(4096) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* Initialized data */
    .data ALIGN(4096) :
    {
        *(.data)
        *(.data.*)
    }

    /* GOT/PLT */
    .got ALIGN(4096) :
    {
        *(.got)
        *(.got.plt)
    }

    /* Uninitialized data */
    .bss ALIGN(4096) :
    {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    }

    _kernel_end = .;

    /* Discard sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.*)
    }
}

/**
 * Kernel Linker Script for x86_64
 * 
 * Key: First LOAD segment must include multiboot header!
 */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);  /* RX - Executable */
    rodata PT_LOAD FLAGS(4);  /* R - Read only */
    data PT_LOAD FLAGS(6);  /* RW - Writable */
}

SECTIONS
{
    /* Higher-half kernel - Limine requirement */
    /* FFFFFFFF80000000 + 1MB offset for compatibility */
    . = 0xFFFFFFFF80100000;

    _kernel_start = .;

    /* Multiboot2 header MUST be early in the file */
    .multiboot ALIGN(8) :
    {
        *(.multiboot)
    } :text

    /* 64-bit kernel code */
    .text ALIGN(4096) :
    {
        *(.text)
        *(.text.*)
    } :text

    /* Read-only data */
    .rodata ALIGN(4096) :
    {
        *(.rodata)
        *(.rodata.*)

        /* Limine requests */
        KEEP(*(.limine_requests_start_marker))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end_marker))
    } :rodata

    /* Initialized data */
    .data ALIGN(4096) :
    {
        *(.data)
        *(.data.*)
    } :data

    /* GOT/PLT */
    .got ALIGN(4096) :
    {
        *(.got)
        *(.got.plt)
    } :data

    /* Uninitialized data */
    .bss ALIGN(4096) :
    {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    } :data

    _kernel_end = .;

    /* Discard sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.*)
    }
}

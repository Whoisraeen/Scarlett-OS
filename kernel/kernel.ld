/**
 * Kernel Linker Script for x86_64
 *
 * Production-grade linker script with proper boot section handling
 * Supports both low-memory boot and higher-half kernel
 */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);    /* RX - Executable */
    rodata PT_LOAD FLAGS(4);  /* R - Read only */
    data PT_LOAD FLAGS(6);    /* RW - Writable */
}

SECTIONS
{
    /* Higher-half kernel starts here */
    . = 0xFFFFFFFF80000000;
    . = ALIGN(4096);

    _kernel_start = .;

    /* 64-bit kernel code */
    .text : {
        *(.text)
        *(.text.*)
        *(.boot.text)
        *(.ap_startup)
    } :text

    . = ALIGN(4096);

    /* Read-only data */
    .rodata : {
        *(.rodata)
        *(.rodata.*)

        /* Limine requests */
        KEEP(*(.limine_requests_start_marker))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end_marker))
    } :rodata

    . = ALIGN(4096);

    /* Initialized data */
    .data : {
        *(.data)
        *(.data.*)
        *(.boot.data)
    } :data

    . = ALIGN(4096);

    /* GOT/PLT */
    .got : {
        *(.got)
        *(.got.plt)
    } :data

    . = ALIGN(4096);

    /* Uninitialized data */
    .bss : {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(.boot.bss)
        *(COMMON)
        _bss_end = .;
    } :data

    _kernel_end = .;
    _kernel_phys_end = . - 0xFFFFFFFF80000000; /* Approximate physical end */

    /* Discard sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.*)
        *(.gnu.hash)
        *(.multiboot)
        *(.boot)
    }
}

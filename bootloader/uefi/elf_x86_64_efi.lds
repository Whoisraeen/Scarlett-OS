/* GNU-EFI style linker script for x86_64 UEFI applications */
OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(efi_main)

SECTIONS
{
  . = 0;
  ImageBase = .;

  /* Code section */
  .text : {
    _text = .;
    *(.text.head)
    *(.text)
    *(.text.*)
    *(.gnu.linkonce.t.*)
    . = ALIGN(16);
    _etext = .;
  }

  /* Relocation section - CRITICAL for UEFI */
  .reloc : {
    *(.reloc)
  }

  /* Data sections */
  .data : {
    _data = .;
    *(.rodata*)
    *(.got.plt)
    *(.got)
    *(.data*)
    *(.sdata)
    /* EFI runtime services data */
    . = ALIGN(16);
    _edata = .;
  }

  .dynamic : { *(.dynamic) }
  .dynsym : { *(.dynsym) }
  .dynstr : { *(.dynstr) }
  .rel : {
    *(.rel.data)
    *(.rel.data.*)
    *(.rel.got)
    *(.rel.stab)
  }
  .rela : {
    *(.rela.text)
    *(.rela.data)
    *(.rela.data.*)
    *(.rela.got)
    *(.rela.stab)
  }

  /* BSS section */
  .bss : {
    _bss = .;
    *(.sbss)
    *(.scommon)
    *(.dynbss)
    *(.bss)
    *(COMMON)
    . = ALIGN(16);
    _ebss = .;
  }

  /* Discard unneeded sections */
  /DISCARD/ : {
    *(.rel.reloc)
    *(.eh_frame)
    *(.note.GNU-stack)
    *(.comment)
  }
}

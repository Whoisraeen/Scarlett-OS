/* GNU-EFI style linker script for x86_64 UEFI applications */
OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(efi_main)

PHDRS
{
  text PT_LOAD;
  data PT_LOAD;
  dynamic PT_DYNAMIC;
}

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
  } :text

  /* Relocation section - CRITICAL for UEFI */
  .reloc : {
    *(.reloc)
    *(.rel)
    *(.rel.*)
    *(.rela)
    *(.rela.*)
    *(.rel.data)
    *(.rel.data.*)
    *(.rel.got)
    *(.rel.stab)
    *(.rela.text)
    *(.rela.data)
    *(.rela.data.*)
    *(.rela.got)
    *(.rela.stab)
    *(.rela.dyn)
    *(.rel.dyn)
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
  } :data

  .dynamic : { *(.dynamic) } :data :dynamic
  .dynsym : { *(.dynsym) } :data
  .dynstr : { *(.dynstr) } :data

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

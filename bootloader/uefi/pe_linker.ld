/* PE linker script for UEFI x86-64 applications */
OUTPUT_FORMAT(pei-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(efi_main)

SECTIONS
{
  . = 0x1000;
  ImageBase = .;
  
  .text ALIGN(0x1000) : {
    *(.text .text.*)
    *(.gnu.linkonce.t.*)
  }
  
  .rodata ALIGN(0x1000) : {
    *(.rodata .rodata.*)
    *(.gnu.linkonce.r.*)
  }
  
  .data ALIGN(0x1000) : {
    *(.data .data.*)
    *(.gnu.linkonce.d.*)
    *(.got .got.plt)
    *(.sdata)
  }
  
  .bss ALIGN(0x1000) : {
    *(.bss .bss.*)
    *(COMMON)
    *(.sbss)
  }
  
  .reloc ALIGN(0x1000) : {
    *(.reloc)
  }
  
  /DISCARD/ : {
    *(.comment)
    *(.note*)
    *(.eh_frame)
    *(.debug*)
    *(.gnu.*)
  }
}

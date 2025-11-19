ENTRY(efi_main)
SECTIONS
{
    . = 0;
    ImageBase = .;
    .text : ALIGN(4096) {
        _text = .;
        *(.text)
        *(.text.*)
        *(.gnu.linkonce.t.*)
        _etext = .;
    }
    .reloc : ALIGN(4096) {
        *(.reloc)
        *(.rela.dyn)
        *(.rel.dyn)
        *(.rel.text)
        *(.rel.data)
        *(.rel.*)
        *(.rela.text)
        *(.rela.data)
        *(.rela.*)
    }
    .data : ALIGN(4096) {
        _data = .;
        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d.*)
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
        *(.sdata)
        _edata = .;
    }
    .dynamic : ALIGN(4096) {
        *(.dynamic)
    }
    .dynsym : ALIGN(4096) {
        *(.dynsym)
    }
    .hash : ALIGN(4096) {
        *(.hash)
    }
    .gnu.hash : ALIGN(4096) {
        *(.gnu.hash)
    }
    .dynstr : ALIGN(4096) {
        *(.dynstr)
    }
    .bss : ALIGN(4096) {
        _bss = .;
        *(.bss)
        *(.bss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        _ebss = .;
    }
    /DISCARD/ : {
        *(.eh_frame)
        *(.note.GNU-stack)
        *(.note.gnu.property)
        *(.comment)
    }
}

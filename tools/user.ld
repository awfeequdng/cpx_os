
OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i86")
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS {
    . = 0x800020;

    .text : {
        *(.text .stub .text.* .gnu.linkonce.t.*)
    }

    PROVIDE(etext = .);

    .rodata : {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    }

    . = ALIGN(0x1000);

    .data : {
        *(.data)
    }

    PROVIDE(edata = .);

    .bss : {
        *(.bss)
    }

    PROVIDE(end = .);

    .stab_info 0x200000 : {
        LONG(__STAB_BEGIN__);
        LONG(__STAB_END__);
        LONG(__STABSTR_BEGIN__);
        LONG(__STABSTR_END__);
    }

    .stab : {
        __STAB_BEGIN__ = DEFINED(__STAB_BEGIN__) ? __STAB_BEGIN__ : .;
        *(.stab);
        __STAB_END__ = DEFINED(__STAB_END__) ? __STAB_END__ : .;
        BYTE(0)
    }

    .stabstr : {
        __STABSTR_BEGIN__ = DEFINED(__STABSTR_BEGIN__) ? __STABSTR_BEGIN__ : .;
        *(.stabstr);
        __STABSTR_END__ = DEFINED(__STABSTR_END__) ? __STABSTR_END__ : .;
        BYTE(0)     /* Force the linker to allocate space
                   for this section */
    }

    /DISCARD/ : {
        *(.eh_frame .note.GNU-stack .comment)
    }

}
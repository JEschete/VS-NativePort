ENTRY(_start)
SECTIONS {
    . = 0x80010000;
    .text   : { *(.text) *(.text.*) }
    .rodata : { *(.rodata) *(.rodata.*) }
    .data   : { *(.data) *(.data.*) }
    .bss    : { *(.bss) *(.bss.*) *(COMMON) }
    /DISCARD/ : {
        *(.note.gnu.build-id) *(.reginfo) *(.MIPS.abiflags)
        *(.gnu.attributes) *(.comment) *(.pdr) *(.mdebug*)
    }
}

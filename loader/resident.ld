ENTRY(pwl_resident_allocate_pages)
SECTIONS {
    . = 0;
    .text : { *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data : { *(.data .data.*) }
    .bss : { *(.bss .bss.*) *(COMMON) }
    /DISCARD/ : { *(.eh_frame*) *(.comment) *(.note*) }
}
ASSERT(SIZEOF(.data) == 0, "Resident image must not contain writable data")
ASSERT(SIZEOF(.bss) == 0, "Resident image must not contain BSS")

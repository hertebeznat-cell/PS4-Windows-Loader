OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)
SECTIONS {
 . = 0;
 .text : { *(.text.start) *(.text*) }
 .rodata : { *(.rodata*) }
 .data : { *(.data*) *(.got*) }
 .bss : { *(.bss*) *(COMMON) }
 __pwl_image_end = .;
 /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.interp) }
}

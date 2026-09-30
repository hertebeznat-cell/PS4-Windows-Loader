.text
.macro RAW name,number
.globl \name
.type \name,@function
\name:
    movq $\number,%rax
    movq %rcx,%r10
    syscall
    jnc 1f
    negq %rax
1:  ret
.size \name,.-\name
.endm
RAW pwl_raw_open,5
RAW pwl_raw_write,4
RAW pwl_raw_fsync,95
RAW pwl_raw_close,6
.section .note.GNU-stack,"",@progbits

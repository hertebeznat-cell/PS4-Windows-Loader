.section .text.start,"ax",@progbits
.globl _start
.type _start,@function
_start:
    pushq %rbp
    movq %rsp,%rbp
    andq $-16,%rsp
    cld
    call _main
    movq %rbp,%rsp
    popq %rbp
    ret
.size _start,.-_start
.section .note.GNU-stack,"",@progbits

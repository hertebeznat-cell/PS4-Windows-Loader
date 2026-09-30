#include "anchor_capture.h"
.text
.p2align 4
.globl pwl_anchor_capture
.type pwl_anchor_capture,@function
pwl_anchor_capture:
    pushfq
    popq %rdx
    xorl %eax, %eax
    movw %cs, %ax
    movq %rax, pwl_anchor_result+PWL_ANCHOR_CS(%rip)
    andl $3, %eax
    jnz .Lwrong_ring
    movq %rdx, pwl_anchor_result+PWL_ANCHOR_FLAGS(%rip)
    movq %rsp, %rax
    andl $15, %eax
    movq %rax, pwl_anchor_result+PWL_ANCHOR_STACK_MOD16(%rip)
    pushq %rbx
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb .Lno_syscall
    movl $0x80000001, %eax
    cpuid
    testl $0x800, %edx
    jz .Lno_syscall
    movl $0xc0000082, %ecx
    rdmsr
    shlq $32, %rdx
    orq %rdx, %rax
    movq %rax, pwl_anchor_result+PWL_ANCHOR_LSTAR(%rip)
    movq $PWL_ANCHOR_COMPLETE, pwl_anchor_result+PWL_ANCHOR_STATE(%rip)
    popq %rbx
    xorl %eax, %eax
    ret
.Lno_syscall:
    movq $PWL_ANCHOR_NO_SYSCALL, pwl_anchor_result+PWL_ANCHOR_STATE(%rip)
    popq %rbx
    xorl %eax, %eax
    ret
.Lwrong_ring:
    movq $PWL_ANCHOR_WRONG_RING, pwl_anchor_result+PWL_ANCHOR_STATE(%rip)
    xorl %eax, %eax
    ret
.size pwl_anchor_capture, .-pwl_anchor_capture
.globl pwl_anchor_capture_end
pwl_anchor_capture_end:
.section .note.GNU-stack,"",@progbits

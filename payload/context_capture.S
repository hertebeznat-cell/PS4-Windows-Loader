#include "context_capture.h"

/* Called only through the already-installed runtime callback facility.
 * No host calls, CPU-control writes, allocations, or device operations.
 * Normal SysV callee-saved registers and stack are preserved.
 */
.text
.p2align 4
.globl pwl_context_capture
.type pwl_context_capture,@function
pwl_context_capture:
    pushfq
    popq %rdx
    xorl %eax, %eax
    movw %cs, %ax
    movq %rax, pwl_context_result+PWL_CONTEXT_CS(%rip)
    andl $3, %eax
    jnz .Lwrong_ring
    movq %rdx, pwl_context_result+PWL_CONTEXT_RFLAGS(%rip)
    movq %cr0, %rax
    movq %rax, pwl_context_result+PWL_CONTEXT_CR0(%rip)
    movq %cr3, %rax
    movq %rax, pwl_context_result+PWL_CONTEXT_CR3(%rip)
    movq %cr4, %rax
    movq %rax, pwl_context_result+PWL_CONTEXT_CR4(%rip)
    movl $0xc0000080, %ecx
    rdmsr
    shlq $32, %rdx
    orq %rdx, %rax
    movq %rax, pwl_context_result+PWL_CONTEXT_EFER(%rip)
    movq $PWL_CONTEXT_COMPLETE, pwl_context_result+PWL_CONTEXT_STATE(%rip)
    xorl %eax, %eax
    ret
.Lwrong_ring:
    movq $PWL_CONTEXT_WRONG_RING, pwl_context_result+PWL_CONTEXT_STATE(%rip)
    xorl %eax, %eax
    ret
.size pwl_context_capture, .-pwl_context_capture
.globl pwl_context_capture_end
pwl_context_capture_end:
.section .note.GNU-stack,"",@progbits

.text
.globl pwl_test_fp_roundtrip
.type pwl_test_fp_roundtrip,@function
/* test buffer, original buffer, mask, kind. Restore original state before
 * returning. All save areas were zeroed and sized from real CPUID by C. */
pwl_test_fp_roundtrip:
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    subq $32,%rsp
    movq %rdi,%r12
    movq %rsi,%r13
    movq %rdx,%r14
    movq %rcx,%r15
    movq %r13,%rdi
    movq %r14,%rsi
    movq %r15,%rdx
    call pwl_x64_fp_save
    fninit
    fld1
    fldpi
    movw $0x077f,0(%rsp)
    fldcw 0(%rsp)
    movl $0x3f80,4(%rsp)
    ldmxcsr 4(%rsp)
    movdqu .Lvector(%rip),%xmm0
    movdqu .Lvector+16(%rip),%xmm15
    testq $4,%r14
    jz .Lready
    vmovdqu .Lvector(%rip),%ymm0
.Lready:
    movq %r12,%rdi
    movq %r14,%rsi
    movq %r15,%rdx
    call pwl_x64_fp_save
    fninit
    pxor %xmm0,%xmm0
    pxor %xmm15,%xmm15
    movl $0x1f80,4(%rsp)
    ldmxcsr 4(%rsp)
    testq $4,%r14
    jz .Lrestore
    vzeroall
.Lrestore:
    movq %r12,%rdi
    movq %r14,%rsi
    movq %r15,%rdx
    call pwl_x64_fp_restore
    movl $1,%ebx
    fnstcw 0(%rsp)
    cmpw $0x077f,0(%rsp)
    jne .Lbad
    stmxcsr 4(%rsp)
    cmpl $0x3f80,4(%rsp)
    jne .Lbad
    movdqu %xmm0,8(%rsp)
    movq .Lvector(%rip),%rax
    cmpq %rax,8(%rsp)
    jne .Lbad
    movq .Lvector+8(%rip),%rax
    cmpq %rax,16(%rsp)
    jne .Lbad
    movdqu %xmm15,8(%rsp)
    movq .Lvector+16(%rip),%rax
    cmpq %rax,8(%rsp)
    jne .Lbad
    movq .Lvector+24(%rip),%rax
    cmpq %rax,16(%rsp)
    jne .Lbad
    testq $4,%r14
    jz .Lx87
    vextractf128 $1,%ymm0,%xmm1
    movdqu %xmm1,8(%rsp)
    movq .Lvector+16(%rip),%rax
    cmpq %rax,8(%rsp)
    jne .Lbad
    movq .Lvector+24(%rip),%rax
    cmpq %rax,16(%rsp)
    jne .Lbad
.Lx87:
    fstpt 8(%rsp)
    movq 32(%r12),%rax /* FXSAVE legacy ST0 = pi, including exponent. */
    cmpq %rax,8(%rsp)
    jne .Lbad
    movw 40(%r12),%ax
    cmpw %ax,16(%rsp)
    jne .Lbad
    jmp .Lfinish
.Lbad:
    xorl %ebx,%ebx
.Lfinish:
    movq %r13,%rdi
    movq %r14,%rsi
    movq %r15,%rdx
    call pwl_x64_fp_restore
    movl %ebx,%eax
    addq $32,%rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    ret
.size pwl_test_fp_roundtrip,.-pwl_test_fp_roundtrip
.section .rodata
.Lvector:
    .quad 0x0123456789abcdef,0xfedcba9876543210
    .quad 0xaaaabbbbccccdddd,0x1357246813572468
.section .note.GNU-stack,"",@progbits

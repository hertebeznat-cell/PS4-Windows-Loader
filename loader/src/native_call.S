.text
.globl pwl_x64_native_call
.type pwl_x64_native_call,@function
pwl_x64_native_call:
    /* This guard must precede argument reads, CLI, CR reads and RDMSR. */
    movw %cs,%ax
    andl $3,%eax
    jnz .Luser
    testq %rdi,%rdi
    jz .Lbad_argument
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    pushfq
    cli
    movq %rsp,%r12
    movq %rdi,%r15
    testq $0x400,(%r12)
    jnz .Lreject
    movq %cr0,%r14
    cmpq 0(%r15),%r14
    jne .Lreject
    movq %cr3,%r13
    cmpq 8(%r15),%r13
    jne .Lreject
    movq %cr4,%rbp
    cmpq 16(%r15),%rbp
    jne .Lreject
    movl $0xc0000080,%ecx
    rdmsr
    shlq $32,%rdx
    orq %rdx,%rax
    cmpq 24(%r15),%rax
    jne .Lreject
    andl $0xd00,%eax
    cmpl $0xd00,%eax
    jne .Lreject
    movq %r14,%rax
    movl $0x80010023,%edx
    andq %rdx,%rax
    cmpq %rdx,%rax
    jne .Lreject
    testq $0x60000004,%r14
    jnz .Lreject
    movq %rbp,%rax
    andl $0x220,%eax
    cmpl $0x220,%eax
    jne .Lreject
    testq $0x1c21000,%rbp /* PKS/CET/PKE, PCID, LA57 */
    jnz .Lreject
    movabsq $0x000ffffffffff000,%rdx
    movq %r13,%rax
    notq %rdx
    testq %rdx,%rax
    jnz .Lreject
    movq 32(%r15),%rax
    testq %rax,%rax
    jz .Lreject
    testq %rdx,%rax
    jnz .Lreject
    movq 40(%r15),%rax
    testq %rax,%rax
    jz .Lreject
    testq $15,%rax
    jnz .Lreject
    cmpq $0,48(%r15)
    je .Lreject
    cmpq $0,56(%r15)
    je .Lreject
    movq 96(%r15),%rax
    addq $32,%rax /* Save helper / MXCSR scratch below the saved RSP. */
    jc .Lreject
    cmpq %rax,%r12
    jb .Lreject
    addq 104(%r15),%rax
    jc .Lreject
    movq %r12,%rdx
    addq $64,%rdx
    jc .Lreject
    cmpq %rdx,%rax
    jb .Lreject
    cmpq $0,64(%r15)
    je .Lreject
    testq $63,64(%r15)
    jnz .Lreject
    cmpq $65536,72(%r15)
    ja .Lreject
    movq 80(%r15),%rax
    cmpq $3,%rax
    je .Lmask_ok
    cmpq $7,%rax
    jne .Lreject
.Lmask_ok:
    xorl %eax,%eax
    cpuid
    movl %eax,%r10d
    cmpl $1,%eax
    jb .Lreject
    movl $1,%eax
    xorl %ecx,%ecx
    cpuid
    andl $0x05000000,%edx /* FXSAVE and SSE2 */
    cmpl $0x05000000,%edx
    jne .Lreject
    testq $0x40000,%rbp
    jz .Lfx_check
    andl $0x0c000000,%ecx
    cmpl $0x0c000000,%ecx
    jne .Lreject
    cmpl $13,%r10d
    jb .Lreject
    cmpq $2,88(%r15)
    jne .Lreject
    xorl %ecx,%ecx
    xgetbv
    shlq $32,%rdx
    orq %rdx,%rax
    cmpq 80(%r15),%rax
    jne .Lreject
    movl $13,%eax
    cpuid
    cmpq 72(%r15),%rbx
    jne .Lreject
    cmpl $576,%ebx
    jb .Lreject
    jmp .Lvalidated
.Lfx_check:
    cmpq $1,88(%r15)
    jne .Lreject
    cmpq $512,72(%r15)
    jne .Lreject
    cmpq $3,80(%r15)
    jne .Lreject
.Lvalidated:
    /* No FP/control changes until every live prerequisite is checked. */
    movq $0,176(%r15)
    movq $0,184(%r15)
    movq $0,192(%r15)
    movq $0,200(%r15)
    movq $0,208(%r15)
    movq %r13,112(%r15)
    movq %r12,136(%r15)
    movq 64(%r15),%rdi
    movq 72(%r15),%rcx
    xorl %eax,%eax
    rep stosb /* Includes zeroed XSAVE reserved header before XSAVE. */
    clts
    movq 64(%r15),%rdi
    movq 80(%r15),%rsi
    movq 88(%r15),%rdx
    call .Lfp_save
    movq $1,176(%r15)
    fninit
    subq $16,%rsp
    movl $0x1f80,(%rsp)
    ldmxcsr (%rsp)
    addq $16,%rsp
    cmpq $7,80(%r15)
    jne .Lno_avx
    vzeroupper
.Lno_avx:
    movq %rbp,%rax
    andq $-129,%rax
    movq %rax,%cr4 /* PGE 1 -> 0 flushes the old global translations. */
    movq 32(%r15),%rax
    movq %rax,%cr3
    movq 40(%r15),%rsp
    movq %cr3,%rax
    movq %rax,120(%r15)
    movq %rsp,144(%r15)
    movq %cr0,%rax
    movq %rax,160(%r15)
    movq %cr4,%rax
    movq %rax,168(%r15)
    movq 56(%r15),%rdi
    call *48(%r15)
    movslq %eax,%rax
    movq %rax,192(%r15)
    /* Callback contract retains the mode, XCR0, mappings and masked IRQs. */
    cli
    cld
    clts
    movq %cr4,%rax
    andq $-129,%rax
    movq %rax,%cr4 /* Flush again if the returning callback enabled PGE. */
    movq %r13,%cr3
    movq %r12,%rsp
    movq %rbp,%rax
    andq $-129,%rax
    movq %rax,%cr4
    movq %r14,%rax
    andq $-9,%rax
    movq %rax,%cr0
    movq 64(%r15),%rdi
    movq 80(%r15),%rsi
    movq 88(%r15),%rdx
    call .Lfp_restore
    movq $1,184(%r15)
    movq %rbp,%cr4
    movq %r14,%cr0
    movq %cr3,%rax
    movq %rax,128(%r15)
    movq %rsp,152(%r15)
    movq $1,200(%r15)
    xorl %eax,%eax
    jmp .Ldone
.Lreject:
    movq $-4,208(%r15)
    movl $-4,%eax
.Ldone:
    popfq
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    ret
.Luser:
    movl $-8,%eax
    ret
.Lbad_argument:
    movl $-1,%eax
    ret

.globl pwl_x64_fp_save
.type pwl_x64_fp_save,@function
pwl_x64_fp_save:
.Lfp_save:
    cmpq $2,%rdx
    je .Lxsave
    fxsave64 (%rdi)
    ret
.Lxsave:
    movq %rsi,%rax
    movq %rsi,%rdx
    shrq $32,%rdx
    xsave64 (%rdi)
    ret
.size pwl_x64_fp_save,.-pwl_x64_fp_save
.globl pwl_x64_fp_restore
.type pwl_x64_fp_restore,@function
pwl_x64_fp_restore:
.Lfp_restore:
    cmpq $2,%rdx
    je .Lxrestore
    fxrstor64 (%rdi)
    ret
.Lxrestore:
    movq %rsi,%rax
    movq %rsi,%rdx
    shrq $32,%rdx
    xrstor64 (%rdi)
    ret
.size pwl_x64_fp_restore,.-pwl_x64_fp_restore
.globl pwl_x64_native_call_end
pwl_x64_native_call_end:
.size pwl_x64_native_call,.-pwl_x64_native_call
.section .note.GNU-stack,"",@progbits

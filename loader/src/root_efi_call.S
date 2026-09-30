.text
.globl pwl_x64_root_efi_call
.type pwl_x64_root_efi_call,@function
pwl_x64_root_efi_call:
    pushq %rbx
    pushq %rbp
    pushq %r15
    pushq %r12
    pushq %r13
    pushq %r14
    pushfq
    cli
    movq %rsp,%r12
    movq %cr3,%r13
    movq %rdx,%r14
    movq %cr4,%rax
    testq $0x321000,%rax
    jne .Lroot_changed
    movq %r9,%r15
    cmpq 0(%r15),%r13
    jne .Lroot_changed
    xorl %eax,%eax
.Lcompare:
    movq (%rcx,%rax,8),%rbx
    cmpq (%r8,%rax,8),%rbx
    jne .Ltable_changed
    incq %rax
    cmpq $512,%rax
    jne .Lcompare
    movq %r13,0(%r14)
    movq %r12,24(%r14)
    movq %rdi,%cr3
    movq %rsi,%rsp
    andq $-16,%rsp
    movq %cr3,%rax
    movq %rax,8(%r14)
    movq %rsp,32(%r14)
    movq 16(%r15),%rdi
    call *8(%r15)
    movq %r13,%cr3
    movq %r12,%rsp
    movq %cr3,%rdx
    movq %rdx,16(%r14)
    movq %rsp,40(%r14)
    jmp .Ldone
.Lroot_changed:
    movl $-1,%eax
    jmp .Ldone
.Ltable_changed:
    movl $-2,%eax
.Ldone:
    popfq
    popq %r14
    popq %r13
    popq %r12
    popq %r15
    popq %rbp
    popq %rbx
    ret
.size pwl_x64_root_efi_call,.-pwl_x64_root_efi_call
.section .note.GNU-stack,"",@progbits

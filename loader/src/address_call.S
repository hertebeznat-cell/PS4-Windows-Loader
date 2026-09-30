.text
.globl pwl_x64_address_call
.type pwl_x64_address_call,@function
pwl_x64_address_call:
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    pushfq
    cli
    movq %rsp,%r12
    movq %cr3,%r13
    movq %r8,%r14
    movq %rdx,%r15
    movq %rcx,%rbx
    movq %r13,0(%r14)
    movq %r12,24(%r14)
    movq %rdi,%cr3
    movq %rsi,%rsp
    andq $-16,%rsp
    movq %cr3,%rbp
    movq %rbp,8(%r14)
    movq %rsp,32(%r14)
    movq %rbx,%rdi
    call *%r15
    movq %r13,%cr3
    movq %r12,%rsp
    movq %cr3,%rdx
    movq %rdx,16(%r14)
    movq %rsp,40(%r14)
    popfq
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    ret
.size pwl_x64_address_call,.-pwl_x64_address_call
.section .note.GNU-stack,"",@progbits

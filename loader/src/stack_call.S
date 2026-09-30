.text
.global pwl_stack_call
.type pwl_stack_call,@function
pwl_stack_call:
    push %r12
    push %r13
    mov %rsp,%r12
    mov %rcx,%r13
    mov %rsp,0(%r13)
    mov %rsi,%rax
    mov %rdi,%rsp
    and $-16,%rsp
    mov %rsp,8(%r13)
    mov %rdx,%rdi
    call *%rax
    mov %r12,%rsp
    mov %rsp,16(%r13)
    pop %r13
    pop %r12
    ret
.size pwl_stack_call,.-pwl_stack_call
.section .note.GNU-stack,"",@progbits

.text
.globl pwl_x64_efi_entry_call
.type pwl_x64_efi_entry_call,@function
pwl_x64_efi_entry_call:
    pushq %rbx
    movq %rdi,%rbx
    subq $32,%rsp
    movq 8(%rbx),%rcx
    movq 16(%rbx),%rdx
    movq $0,32(%rbx)
    call *0(%rbx)
    movq %rax,24(%rbx)
    movq $1,32(%rbx)
    addq $32,%rsp
    popq %rbx
    xorl %eax,%eax
    ret
.globl pwl_x64_efi_entry_call_end
pwl_x64_efi_entry_call_end:
.size pwl_x64_efi_entry_call,.-pwl_x64_efi_entry_call
.section .note.GNU-stack,"",@progbits

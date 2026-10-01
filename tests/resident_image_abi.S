.text
.globl test_image_abi
.type test_image_abi,@function
# SysV arguments: StartImage destination and image handle. This host-only
# caller checks the Microsoft ABI registers after an application's Exit jump.
test_image_abi:
    subq $72,%rsp
    movdqu %xmm6,32(%rsp)
    movdqu %xmm15,48(%rsp)
    pcmpeqd %xmm6,%xmm6
    pcmpeqd %xmm15,%xmm15
    movq %rdi,%rax
    movq %rsi,%rcx
    xorl %edx,%edx
    xorl %r8d,%r8d
    call *%rax
    pmovmskb %xmm6,%eax
    cmpl $65535,%eax
    jne 1f
    pmovmskb %xmm15,%eax
    cmpl $65535,%eax
    jne 1f
    xorl %eax,%eax
    jmp 2f
1:  movl $1,%eax
2:  movdqu 32(%rsp),%xmm6
    movdqu 48(%rsp),%xmm15
    addq $72,%rsp
    ret
.size test_image_abi,.-test_image_abi
.section .note.GNU-stack,"",@progbits

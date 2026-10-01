.section .rodata.pwl_binding,"a",@progbits
.balign 8
.global pwl_resident_binding
.hidden pwl_resident_binding
.type pwl_resident_binding,@object
pwl_resident_binding:
.quad 0
.size pwl_resident_binding,8
.section .note.GNU-stack,"",@progbits

/* Microsoft AMD64 error-return adapter. No argument reads, stack adjustment,
 * data dependency or imports. Assembly avoids incompatible C function-pointer
 * types across the differing unimplemented service signatures. */
.section .text.pwl_unsupported,"ax",@progbits
.global pwl_resident_unsupported
.hidden pwl_resident_unsupported
.type pwl_resident_unsupported,@function
pwl_resident_unsupported:
    movabsq $0x8000000000000003,%rax
    ret
.size pwl_resident_unsupported,.-pwl_resident_unsupported

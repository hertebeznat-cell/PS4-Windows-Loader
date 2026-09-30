.section .rodata.pwl_binding,"a",@progbits
.balign 8
.global pwl_resident_binding
.hidden pwl_resident_binding
.type pwl_resident_binding,@object
pwl_resident_binding:
.quad 0
.size pwl_resident_binding,8
.section .note.GNU-stack,"",@progbits

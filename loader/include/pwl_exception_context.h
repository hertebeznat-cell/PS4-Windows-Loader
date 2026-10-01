#ifndef PWL_EXCEPTION_CONTEXT_H
#define PWL_EXCEPTION_CONTEXT_H
#include "pwl.h"
typedef struct pwl_x64_tss_descriptor {
    uint64_t base,bytes;
    unsigned busy;
} pwl_x64_tss_descriptor_t;
typedef struct pwl_x64_idt_gate {
    uint64_t entry;
    uint16_t selector;
    unsigned ist,dpl,trap;
} pwl_x64_idt_gate_t;
typedef struct pwl_x64_tss_stacks {
    uint64_t rsp[3],ist[7];
    uint16_t iomap_base;
} pwl_x64_tss_stacks_t;
/* Decode explicit copied bytes, never dereference descriptor-derived addresses.
 * Four-level canonical addresses and AMD64 descriptors only. Outputs remain
 * unchanged on failure. These checks do not supply exception-handler mappings
 * or claim that a CPU transition is recoverable. */
pwl_status_t pwl_x64_tss_descriptor_decode(const void *gdt,size_t bytes,
    uint16_t selector,pwl_x64_tss_descriptor_t *out);
pwl_status_t pwl_x64_idt_gate_decode(const void *gdt,size_t gdt_bytes,
    const void *idt,size_t idt_bytes,unsigned vector,pwl_x64_idt_gate_t *out);
pwl_status_t pwl_x64_tss_stacks_decode(const void *tss,size_t bytes,
    pwl_x64_tss_stacks_t *out);
#endif

#ifndef PS4WL_CONTEXT_CAPTURE_H
#define PS4WL_CONTEXT_CAPTURE_H

/* Shared offsets for the assembly callback and user-process log writer. */
#define PWL_CONTEXT_STATE 0
#define PWL_CONTEXT_CS 8
#define PWL_CONTEXT_CR0 16
#define PWL_CONTEXT_CR3 24
#define PWL_CONTEXT_CR4 32
#define PWL_CONTEXT_EFER 40
#define PWL_CONTEXT_RFLAGS 48
#define PWL_CONTEXT_BYTES 56
#define PWL_CONTEXT_PENDING 1
#define PWL_CONTEXT_COMPLETE 2
#define PWL_CONTEXT_WRONG_RING 3

#ifndef __ASSEMBLER__
#include <stddef.h>
struct pwl_context_snapshot {
    unsigned long long state, cs, cr0, cr3, cr4, efer, rflags;
};
#define PWL_CHECK_OFFSET(member, value) \
    _Static_assert(offsetof(struct pwl_context_snapshot, member) == value, "context ABI")
PWL_CHECK_OFFSET(state, PWL_CONTEXT_STATE);
PWL_CHECK_OFFSET(cs, PWL_CONTEXT_CS);
PWL_CHECK_OFFSET(cr0, PWL_CONTEXT_CR0);
PWL_CHECK_OFFSET(cr3, PWL_CONTEXT_CR3);
PWL_CHECK_OFFSET(cr4, PWL_CONTEXT_CR4);
PWL_CHECK_OFFSET(efer, PWL_CONTEXT_EFER);
PWL_CHECK_OFFSET(rflags, PWL_CONTEXT_RFLAGS);
_Static_assert(sizeof(struct pwl_context_snapshot) == PWL_CONTEXT_BYTES, "context size");
extern volatile struct pwl_context_snapshot pwl_context_result;
void pwl_context_capture(void);
extern const unsigned char pwl_context_capture_end[];
#endif
#endif

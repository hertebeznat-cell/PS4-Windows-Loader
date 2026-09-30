#ifndef PWL_ANCHOR_CAPTURE_H
#define PWL_ANCHOR_CAPTURE_H
#define PWL_ANCHOR_STATE 0
#define PWL_ANCHOR_CS 8
#define PWL_ANCHOR_FLAGS 16
#define PWL_ANCHOR_STACK_MOD16 24
#define PWL_ANCHOR_LSTAR 32
#define PWL_ANCHOR_BYTES 40
#define PWL_ANCHOR_PENDING 1
#define PWL_ANCHOR_COMPLETE 2
#define PWL_ANCHOR_WRONG_RING 3
#define PWL_ANCHOR_NO_SYSCALL 4
#ifndef __ASSEMBLER__
#include <stddef.h>
struct pwl_anchor_snapshot {
    unsigned long long state, cs, flags, stack_mod16, lstar;
};
_Static_assert(offsetof(struct pwl_anchor_snapshot, lstar) == PWL_ANCHOR_LSTAR, "anchor ABI");
_Static_assert(sizeof(struct pwl_anchor_snapshot) == PWL_ANCHOR_BYTES, "anchor size");
extern volatile struct pwl_anchor_snapshot pwl_anchor_result;
void pwl_anchor_capture(void);
extern const unsigned char pwl_anchor_capture_end[];
#endif
#endif

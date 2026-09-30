#ifndef PWL_STACK_CALL_H
#define PWL_STACK_CALL_H
#include "pwl.h"
typedef struct pwl_stack_report {
    uint64_t before, entered, after;
} pwl_stack_report_t;
/* SysV entry/callback. Caller owns an RW stack with sufficient space and
 * guard pages. top/report/callback must be valid in the current context.
 * No CR3, control registers, interrupt or address-context changes.
 */
int pwl_stack_call(void *top,int (*callback)(void *),void *context,
                   pwl_stack_report_t *report);
#endif

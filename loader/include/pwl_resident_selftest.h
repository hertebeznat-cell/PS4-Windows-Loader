#ifndef PWL_RESIDENT_SELFTEST_H
#define PWL_RESIDENT_SELFTEST_H
#include "pwl_resident.h"
typedef struct pwl_resident_call_report {
    uint32_t passed_mask, last_call;
    uint64_t exit_status;
} pwl_resident_call_report_t;
/* Returning diagnostic for a prepared, resident execution context. Uses synthetic memory descriptors and never
 * accesses returned page addresses. code must be an audited executable copy,
 * with its binding slot pointing to data in this same address context.
 * checkpoint is called before each entry; a failure stops further calls.
 */
pwl_status_t pwl_resident_calls_test(const pwl_resident_image_t *image,
    void *code,pwl_resident_data_t *data,pwl_resident_call_report_t *report,
    int (*checkpoint)(unsigned,void *),void *context);
#endif

#ifndef PWL_ROOT_CLONE_REPORT_H
#define PWL_ROOT_CLONE_REPORT_H
#include <stdint.h>
#include "workspace_report.h"
typedef struct pwl_root_clone_report {
 uint64_t cr0,cr3,cr4,efer,source,source_pa,kva,pa;
 uint64_t root_before,root_entered,root_after,stack_before,stack_entered,stack_after;
 unsigned stage,error,switched,restored,released;
 int transition_status;
} pwl_root_clone_report_t;
int pwl_root_clone_experiment(void *,void *,pwl_workspace_alloc_fn,
 pwl_workspace_free_fn,pwl_workspace_extract_fn,volatile pwl_root_clone_report_t *);
#endif

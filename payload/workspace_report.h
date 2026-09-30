#ifndef PWL_WORKSPACE_REPORT_H
#define PWL_WORKSPACE_REPORT_H
typedef unsigned long long (*pwl_workspace_alloc_fn)(void *,unsigned long long,int,unsigned long long,unsigned long long,unsigned long,unsigned long,char);
typedef void (*pwl_workspace_free_fn)(void *,unsigned long long,unsigned long long);
typedef unsigned long long (*pwl_workspace_extract_fn)(void *,unsigned long long);
typedef struct pwl_workspace_report {
 unsigned long long kva,pa,bytes,root;
 int prepare_status,table_status,release_status,efi_status;
 unsigned int stage,tables,regions,copy_ok;
} pwl_workspace_report_t;
int pwl_workspace_experiment(void *,void *,pwl_workspace_alloc_fn,pwl_workspace_free_fn,
                            pwl_workspace_extract_fn,volatile pwl_workspace_report_t *);
#endif

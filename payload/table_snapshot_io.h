#ifndef PWL_TABLE_SNAPSHOT_IO_H
#define PWL_TABLE_SNAPSHOT_IO_H
#include "pwl_table_snapshot.h"
#include "pwl_root_clone.h"
#include "workspace_report.h"
typedef struct pwl_console_table_reader {
    void *pmap;
    pwl_workspace_extract_fn extract;
    uint32_t direct_pml4,direct_pdpt;
    uint64_t expected_root;
} pwl_console_table_reader_t;
/* Same-context diagnostic adapter. Explicit verified translator and runtime
 * direct-map indices; no offset lookup, binding approval or PA-to-pointer cast.
 * Initialization validates both endpoints of the kernel root and active root.
 * A platform must retain the pmap and prevent table mutation/migration. */
pwl_status_t pwl_console_table_reader_prepare(void *pmap,
    pwl_workspace_extract_fn extract,uint32_t direct_pml4,uint32_t direct_pdpt,
    uint64_t kernel_root_alias,const pwl_x64_cpu_state_t *cpu,
    pwl_console_table_reader_t *out);
/* Pure address resolution, before any source bytes are read. */
pwl_status_t pwl_console_table_address(const pwl_console_table_reader_t *reader,
    uint64_t physical_address,uint64_t *out);
/* Kernel-context callback: checks current CR3 before/after, exact physical
 * translations before/after copy, then returns bytes to the generic collector.
 * Rejection does not certify fault recovery for source memory invalidation. */
pwl_status_t pwl_console_table_read(void *context,uint64_t physical_address,
                                   uint64_t out[512]);
pwl_status_t pwl_console_table_snapshot_capture(pwl_console_table_reader_t *reader,
    pwl_x64_table_page_t *tables,size_t capacity,size_t *used_out);
#endif

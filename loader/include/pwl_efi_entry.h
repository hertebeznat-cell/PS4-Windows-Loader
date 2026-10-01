#ifndef PWL_EFI_ENTRY_H
#define PWL_EFI_ENTRY_H
#include "pwl_native_transition.h"
typedef struct pwl_efi_entry_context {
    uint64_t entry, image_handle, system_table;
    uint64_t status, returned;
} pwl_efi_entry_context_t;
_Static_assert(sizeof(pwl_efi_entry_context_t)==40,"EFI entry context ABI");
_Static_assert(offsetof(pwl_efi_entry_context_t,status)==24,"EFI status offset");
/* Constructs arguments for the resident StartImage wrapper of the audited
 * relocated application and validates
 * the prepared root. Output unchanged on failure. Does not enter any code or
 * publish the context in resident memory. Caller must copy to owned RAM and
 * establish both-root thunk/report/stack continuity before CPU entry. */
pwl_status_t pwl_native_efi_entry_prepare(const pwl_native_workspace_t *workspace,
    const pwl_resident_image_t *image,const pwl_native_transition_plan_t *plan,
    const pwl_x64_table_page_t *tables,pwl_efi_entry_context_t *out);
/* Bind the private permission service to the final inactive transition root.
 * Verifies contiguous owned table addresses and identity RW/NX mappings.
 * The transition preparer's exclusive table ownership contract still applies.
 * No CPU changes. Output state unchanged on failure. */
pwl_status_t pwl_native_image_mapping_bind(const pwl_native_workspace_t *workspace,
    const pwl_resident_image_t *image,const pwl_native_transition_plan_t *plan,
    const pwl_x64_table_page_t *tables);
/* Optional fixed linear framebuffer output. Publishes GOP/SimpleTextOutput
 * only after geometry, final identity RW/NX/cache mappings and registry space
 * are checked. The actual video mode, device ownership and PAT/MTRR contract
 * come from platform capture, not these checks. Does not write video bytes. */
pwl_status_t pwl_native_graphics_publish(pwl_native_workspace_t *workspace,
    const pwl_resident_image_t *image,const pwl_native_transition_plan_t *plan,
    const pwl_x64_table_page_t *tables,const pwl_graphics_spec_t *spec);
/* SysV callback adapter for pwl_x64_address_call. Context points to resident
 * memory under the active root. Calls the EFI entry with Microsoft AMD64 ABI,
 * preserves all 64 EFI_STATUS bits in context, returns zero after EFI returns.
 * No CPU mode change or exception recovery. Never call a PA from user mode.
 * Text between these symbols is copyable, with no external references. */
int pwl_x64_efi_entry_call(void *context);
extern const unsigned char pwl_x64_efi_entry_call_end[];
#endif

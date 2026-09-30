#ifndef PWL_ADDRESS_CALL_H
#define PWL_ADDRESS_CALL_H
#include <stdint.h>
#include <stddef.h>
/* Experimental SysV entry for a controlled CPL0 environment. NOT wired to PS4.
 * Entry code must have identical executable virtual mappings under both roots;
 * the report must have identical writable mappings under both roots. The new
 * stack and callback/context must be resident under the new root. Caller must
 * establish four-level paging, PCID disabled, coherent cache types and CPU
 * ownership and clear DF. IRQ masking does not block NMI or machine-check exceptions; their
 * handler dependencies must remain mapped. No fault recovery is supplied.
 * Roots/stacks are trusted validated inputs, never user-process addresses.
 */
typedef struct pwl_address_call_report {
    uint64_t root_before, root_entered, root_after;
    uint64_t stack_before, stack_entered, stack_after;
} pwl_address_call_report_t;
_Static_assert(sizeof(pwl_address_call_report_t)==48, "address report size");
_Static_assert(offsetof(pwl_address_call_report_t,stack_after)==40, "address report offsets");
int pwl_x64_address_call(uint64_t root, uint64_t stack_top,
    int (*callback)(void *), void *context, pwl_address_call_report_t *report);
#endif

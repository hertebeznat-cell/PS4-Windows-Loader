#ifndef PWL_DEVICES_H
#define PWL_DEVICES_H
#include "pwl.h"
#define PWL_DEVICE_MAX 32U
/* Preparation-context operations under exclusive driver/device ownership.
 * read/write are exactly 16-bit PCI config accesses, never a combined write
 * to Command+Status (Status is W1C). IDs identify actual enumerated functions.
 * stop prevents new work; drain confirms completion of ALL outstanding DMA.
 * Neither clearing Bus Master nor masking MSI supplies that confirmation.
 * Failed stop/write may have effects: their undo is attempted too.
 * Callbacks/context, saved state and the complete inventory stay resident.
 */
typedef struct pwl_device_ops {
    void *context;
    pwl_status_t (*read16)(void *,uint64_t,unsigned,uint16_t *);
    pwl_status_t (*write16)(void *,uint64_t,unsigned,uint16_t);
    pwl_status_t (*stop)(void *,uint64_t);
    pwl_status_t (*drain)(void *,uint64_t);
    pwl_status_t (*resume)(void *,uint64_t);
} pwl_device_ops_t;
typedef struct pwl_device_saved {
    uint64_t id;
    uint16_t vendor,device,header,command,msi,msix,msi_control,msix_control;
    unsigned stopped,dirty;
} pwl_device_saved_t;
typedef struct pwl_device_lease {
    pwl_device_saved_t devices[PWL_DEVICE_MAX];
    size_t count;
    unsigned quiesced;
    pwl_status_t operation_status,restore_status;
} pwl_device_lease_t;
/* Fresh zero lease; nonempty duplicate-free explicitly supplied inventory.
 * Captures every function before changing any; stops/drains all, then masks
 * MSI/MSI-X/INTx and Bus Master with readback. No CPU switch, global interrupt
 * controller operation or inference that the supplied inventory is complete.
 * Failure rolls back. A rollback failure retains the lease and MUST block
 * CPU entry, owner release and unpinning; release may then be retried.
 */
pwl_status_t pwl_devices_quiesce(const pwl_device_ops_t *,const uint64_t *,size_t,
    pwl_device_lease_t *);
/* Config-only rollback while platform IRQ/controllers remain masked. Keeps
 * stopped drivers and lease; release subsequently resumes and drops it. */
pwl_status_t pwl_devices_restore(const pwl_device_ops_t *,pwl_device_lease_t *);
pwl_status_t pwl_devices_release(const pwl_device_ops_t *,pwl_device_lease_t *);
#endif

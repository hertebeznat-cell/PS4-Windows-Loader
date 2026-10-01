#ifndef PWL_PCI_MMIO_H
#define PWL_PCI_MMIO_H
#include "pwl_devices.h"
#include "pwl_transition_map.h"
typedef struct pwl_pci_mapping {uint64_t id,address,physical;} pwl_pci_mapping_t;
typedef struct pwl_pci_mmio {
    uint64_t root,pat;
    size_t count;
    pwl_pci_mapping_t mappings[PWL_DEVICE_MAX];
} pwl_pci_mmio_t;
/* Actual already mapped ECAM function pages only, from authoritative platform
 * inventory. Validate supervisor RW/NX, exact PA and PAT type UC=0 against a
 * stable complete table snapshot. Does not discover ECAM or certify that PA
 * is a PCI function, not ordinary RAM. Source/callback lifetime and unchanged
 * mappings/PAT are exclusive platform contracts; no address is synthesized. */
pwl_status_t pwl_pci_mappings_validate(const pwl_x64_table_page_t *,size_t,uint64_t,
    uint64_t,const pwl_pci_mapping_t *,size_t);
/* Real CPL0/root/PAT bind; copied mapping inventory, output unchanged failure.
 * CPL3 refuses before reading any argument. Table provenance remains caller's
 * protected snapshot contract, not inferred from supplying a matching root. */
pwl_status_t pwl_pci_mmio_bind(const pwl_x64_table_page_t *,size_t,
    const pwl_pci_mapping_t *,size_t,pwl_pci_mmio_t *);
pwl_status_t pwl_pci_mmio_read16(void *,uint64_t,unsigned,uint16_t *);
pwl_status_t pwl_pci_mmio_write16(void *,uint64_t,unsigned,uint16_t);
#endif

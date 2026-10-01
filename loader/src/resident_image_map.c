#include "pwl_image_permissions.h"
/* Isolated privileged adapter. No CR3 writes or interrupt changes. The owner
 * must have already stopped other CPUs/DMA and installed these private tables.
 * Readiness of the whole platform is outside this service's contract.
 */
uint64_t __attribute__((ms_abi)) pwl_resident_image_map(pwl_resident_data_t *d,
    const pwl_pe_loaded_t *image,unsigned restore)
{
    uint16_t cs;
    __asm__ volatile("mov %%cs,%0":"=r"(cs));
    if ((cs&3)!=0) return PWL_EFI_UNSUPPORTED;
    if (!d || !image || !d->image_mapping.root) return PWL_EFI_UNSUPPORTED;
    uint64_t cr0,cr3,cr4,flags;uint32_t lo,hi;
    __asm__ volatile("mov %%cr0,%0;mov %%cr3,%1;mov %%cr4,%2;pushfq;popq %3"
        :"=r"(cr0),"=r"(cr3),"=r"(cr4),"=r"(flags)::"memory");
    if (cr3!=d->image_mapping.root || !(cr0&(UINT64_C(1)<<31)) ||
        !(cr0&(UINT64_C(1)<<16)) || (cr0&12) || !(cr4&(UINT64_C(1)<<9)) ||
        cr4&((UINT64_C(1)<<17)|(UINT64_C(1)<<12)|(UINT64_C(1)<<7)) || flags&(512|1024))
        return PWL_EFI_UNSUPPORTED;
    __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(UINT32_C(0xc0000080)));
    (void)hi;
    if (!(lo&(1U<<11))) return PWL_EFI_UNSUPPORTED; /* EFER.NXE */
    if (restore>2) return PWL_EFI_INVALID_PARAMETER;
    uint64_t status=pwl_image_permissions(&d->image_mapping,image,restore==1,restore==2 ? 2 : 1);
    if (status) return status;
    if (restore==2) return PWL_EFI_SUCCESS;
    for (uint64_t p=image->physical_address;p<image->physical_address+image->image_size;p+=4096)
        __asm__ volatile("invlpg (%0)"::"r"(p):"memory");
    return PWL_EFI_SUCCESS;
}

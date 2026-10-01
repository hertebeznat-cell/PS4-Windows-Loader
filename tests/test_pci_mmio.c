#include "pwl_pci_mmio.h"
#include <assert.h>
#include <stdio.h>
#define NX (UINT64_C(1)<<63)
static uint64_t pages[4][512];
int main(void)
{
    pwl_x64_table_page_t tables[4];
    for(unsigned i=0;i<4;i++)tables[i]=(pwl_x64_table_page_t){0x1000+i*4096,pages[i]};
    uint64_t va=UINT64_C(0xffff800000000000),pa=UINT64_C(0xe0000000);
    uint64_t pat=UINT64_C(0x0007040600070406);
    pages[0][256]=0x2007;pages[1][0]=0x3007;pages[2][0]=0x4007;
    pages[3][0]=pa|NX|0x1b;pages[3][1]=(pa+4096)|NX|0x1b;
    pwl_pci_mapping_t m[2]={{9,va,pa},{10,va+4096,pa+4096}};
    assert(pwl_pci_mappings_validate(tables,4,0x1000,pat,m,2)==PWL_OK);
    const uint64_t bad[]={pa|NX|3,pa|0x1b,pa|NX|0x1f,pa|NX|0x19,(pa+4096)|NX|0x1b};
    for(size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        pages[3][0]=bad[i];
        assert(pwl_pci_mappings_validate(tables,4,0x1000,pat,m,2)==PWL_ERR_BAD_IMAGE);
    }
    pages[3][0]=pa|NX|0x1b;
    assert(pwl_pci_mappings_validate(tables,4,0x1000,pat|(UINT64_C(7)<<24),m,2)==PWL_ERR_BAD_IMAGE);
    m[1].id=9;assert(pwl_pci_mappings_validate(tables,4,0x1000,pat,m,2)==PWL_ERR_INVALID_ARGUMENT);
    m[1].id=10;m[1].physical=pa;
    assert(pwl_pci_mappings_validate(tables,4,0x1000,pat,m,2)==PWL_ERR_INVALID_ARGUMENT);
    m[1].physical=pa+4096;m[1].address=va;
    assert(pwl_pci_mappings_validate(tables,4,0x1000,pat,m,2)==PWL_ERR_INVALID_ARGUMENT);
    m[1].address=UINT64_C(1)<<47;
    assert(pwl_pci_mappings_validate(tables,4,0x1000,pat,m,2)==PWL_ERR_BAD_IMAGE);
    m[1].address=UINT64_MAX-4095;
    assert(pwl_pci_mappings_validate(tables,4,0x1000,pat,m,2)==PWL_ERR_BAD_IMAGE);
    assert(pwl_pci_mmio_bind((void *)1,1,(void *)1,1,(void *)1)==PWL_ERR_UNSUPPORTED);
    assert(pwl_pci_mmio_read16((void *)1,0,0,(void *)1)==PWL_ERR_UNSUPPORTED);
    assert(pwl_pci_mmio_write16((void *)1,0,4,0)==PWL_ERR_UNSUPPORTED);
    puts("PCI config: synthetic UC/supervisor/RW/NX/PA audit and actual CPL3 refusal before unreadable arguments; no MMIO hardware access tested");
}

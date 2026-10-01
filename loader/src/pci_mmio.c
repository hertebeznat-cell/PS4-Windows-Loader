#include "pwl_pci_mmio.h"
static int privileged(void)
{uint16_t cs;__asm__ volatile("mov %%cs,%0":"=r"(cs)::"memory");return !(cs&3);}
static void live(uint64_t *root,uint64_t *pat)
{uint32_t lo,hi;__asm__ volatile("mov %%cr3,%0":"=r"(*root));
 __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0x277));*pat=(uint64_t)lo|((uint64_t)hi<<32);}
pwl_status_t pwl_pci_mappings_validate(const pwl_x64_table_page_t *tables,size_t tn,uint64_t root,
    uint64_t pat,const pwl_pci_mapping_t *m,size_t n)
{
    if(!tables || !m || !n || n>PWL_DEVICE_MAX)return PWL_ERR_INVALID_ARGUMENT;
    for(size_t i=0;i<n;i++) {
        if(!m[i].address || m[i].address%4096 || m[i].address>UINT64_MAX-4095 ||
           !m[i].physical || m[i].physical%4096 || m[i].physical>=(UINT64_C(1)<<52))return PWL_ERR_INVALID_ARGUMENT;
        for(size_t j=0;j<i;j++)if(m[i].id==m[j].id || m[i].address==m[j].address ||
            m[i].physical==m[j].physical)return PWL_ERR_INVALID_ARGUMENT;
        for(unsigned last=0;last<2;last++) {
            pwl_x64_translation_t x;uint64_t offset=last?4095:0;
            if(pwl_x64_translate(tables,tn,root,m[i].address+offset,&x)!=PWL_OK ||
               x.physical_address!=m[i].physical+offset || !x.writable || x.executable ||
               x.user || ((pat>>(8*x.pat_index))&255))return PWL_ERR_BAD_IMAGE;
        }
    }
    return PWL_OK;
}
pwl_status_t pwl_pci_mmio_bind(const pwl_x64_table_page_t *tables,size_t tn,
    const pwl_pci_mapping_t *m,size_t n,pwl_pci_mmio_t *out)
{
    if(!privileged())return PWL_ERR_UNSUPPORTED;
    if(!out)return PWL_ERR_INVALID_ARGUMENT;
    uint64_t root,pat;live(&root,&pat);
    pwl_status_t s=pwl_pci_mappings_validate(tables,tn,root,pat,m,n);if(s!=PWL_OK)return s;
    pwl_pci_mmio_t prepared={0};prepared.root=root;prepared.pat=pat;prepared.count=n;
    for(size_t i=0;i<n;i++)prepared.mappings[i]=m[i];
    uint64_t again_root,again_pat;live(&again_root,&again_pat);
    if(root!=again_root || pat!=again_pat)return PWL_ERR_ACCESS_DENIED;
    *out=prepared;return PWL_OK;
}
static pwl_status_t address(pwl_pci_mmio_t *p,uint64_t id,unsigned at,volatile uint16_t **out)
{
    if(!p || !p->count || p->count>PWL_DEVICE_MAX || at>254 || (at&1))return PWL_ERR_INVALID_ARGUMENT;
    uint64_t root,pat;live(&root,&pat);
    if(root!=p->root || pat!=p->pat)return PWL_ERR_ACCESS_DENIED;
    for(size_t i=0;i<p->count;i++)if(p->mappings[i].id==id) {
        if(!p->mappings[i].address || p->mappings[i].address%4096 ||
            p->mappings[i].address>UINT64_MAX-4095)return PWL_ERR_BAD_IMAGE;
        *out=(volatile uint16_t *)(uintptr_t)(p->mappings[i].address+at);return PWL_OK;
    }
    return PWL_ERR_NOT_FOUND;
}
pwl_status_t pwl_pci_mmio_read16(void *context,uint64_t id,unsigned at,uint16_t *out)
{
    if(!privileged())return PWL_ERR_UNSUPPORTED;
    if(!out)return PWL_ERR_INVALID_ARGUMENT;
    volatile uint16_t *p;pwl_status_t s=address(context,id,at,&p);if(s!=PWL_OK)return s;
    __asm__ volatile("mfence":::"memory");uint16_t value=*p;
    __asm__ volatile("mfence":::"memory");*out=value;return PWL_OK;
}
pwl_status_t pwl_pci_mmio_write16(void *context,uint64_t id,unsigned at,uint16_t value)
{
    if(!privileged())return PWL_ERR_UNSUPPORTED;
    if(at!=4 && (at<0x42 || (at&3)!=2))return PWL_ERR_INVALID_ARGUMENT;
    volatile uint16_t *p;pwl_status_t s=address(context,id,at,&p);if(s!=PWL_OK)return s;
    if(at!=4) {uint16_t kind=*(p-1)&255;if(kind!=5 && kind!=0x11)return PWL_ERR_INVALID_ARGUMENT;}
    __asm__ volatile("mfence":::"memory");*p=value;
    __asm__ volatile("mfence":::"memory");return PWL_OK;
}

#include "pwl_image_permissions.h"
#define PA_MASK UINT64_C(0x000ffffffffff000)
#define NX (UINT64_C(1)<<63)
static int table_owned(const pwl_image_mapping_t *m,uint64_t pa)
{
    return pa>=m->tables_base && pa-m->tables_base<m->tables_bytes && !(pa&4095);
}
static uint64_t *leaf(const pwl_image_mapping_t *m,uint64_t pa,unsigned execute)
{
    uint64_t table=m->root;
    for (unsigned level=4;level;level--) {
        if (!table_owned(m,table)) return NULL;
        uint64_t *slot=(uint64_t *)(uintptr_t)table+((pa>>(12+9*(level-1)))&511);
        uint64_t value=*slot;
        if (!(value&1) || (value&4) || (execute && (value&NX))) return NULL;
        if (level==1) {
            /* Existing NX at a leaf is expected before executable publication. */
            return (value&PA_MASK)==pa && !(value&256) ? slot : NULL;
        }
        if (value&128 || !(value&2)) return NULL;
        table=value&PA_MASK;
    }
    return NULL;
}
uint64_t pwl_image_permissions(pwl_image_mapping_t *m,const pwl_pe_loaded_t *im,
    unsigned restore,unsigned apply)
{
    if (!m || !im || restore>1 || apply>2 || !m->tables_base ||
        m->tables_base&4095 || !m->tables_bytes || m->tables_bytes&4095 ||
        m->tables_bytes>128*PWL_PAGE_SIZE ||
        m->tables_base>=(UINT64_C(1)<<47) ||
        m->tables_bytes>(UINT64_C(1)<<47)-m->tables_base ||
        !table_owned(m,m->root) || !m->heap_base || m->heap_base&4095 ||
        !m->heap_bytes || m->heap_bytes&4095 || m->heap_base>=(UINT64_C(1)<<47) ||
        m->heap_bytes>(UINT64_C(1)<<47)-m->heap_base ||
        (m->heap_base<m->tables_base+m->tables_bytes && m->tables_base<m->heap_base+m->heap_bytes) ||
        !im->image_size || im->image_size&4095 || im->physical_address&4095 ||
        !im->range_count || im->range_count>PWL_PE_MAX_RANGES)
        return PWL_EFI_INVALID_PARAMETER;
    int heap=im->physical_address>=m->heap_base && im->physical_address-m->heap_base<=m->heap_bytes &&
        im->image_size<=m->heap_bytes-(im->physical_address-m->heap_base);
    if (!heap && (restore || im->physical_address!=m->boot_base || im->image_size!=m->boot_bytes))
        return PWL_EFI_INVALID_PARAMETER;
    if (im->physical_address>=(UINT64_C(1)<<47) || im->image_size>(UINT64_C(1)<<47)-im->physical_address)
        return PWL_EFI_INVALID_PARAMETER;
    uint64_t covered=0;unsigned entry=0;
    for (size_t i=0;i<im->range_count;i++) {
        const pwl_x64_identity_range_t *r=&im->ranges[i];
        if (r->base!=im->physical_address+covered || !r->size || r->size&4095 ||
            r->size>im->image_size-covered || r->writable>1 || r->executable>1 ||
            (r->writable && r->executable)) return PWL_EFI_INVALID_PARAMETER;
        if (r->executable && im->entry_address>=r->base && im->entry_address-r->base<r->size) entry=1;
        for (uint64_t p=r->base;p<r->base+r->size;p+=4096) {
            /* Executable restriction is checked on ancestors only; leaf NX
             * is intentionally replaced below after complete validation. */
            uint64_t table=m->root;
            for (unsigned l=4;l>1;l--) {
                if (!table_owned(m,table)) return PWL_EFI_DEVICE_ERROR;
                uint64_t value=((uint64_t *)(uintptr_t)table)[(p>>(12+9*(l-1)))&511];
                if (!(value&1) || !(value&2) || value&(4|128) ||
                    (!restore && r->executable && value&NX)) return PWL_EFI_DEVICE_ERROR;
                table=value&PA_MASK;
            }
            uint64_t *slot=leaf(m,p,0);
            if (!slot) return PWL_EFI_DEVICE_ERROR;
            if (apply==2 && (!!(*slot&2)!=r->writable || !!(*slot&NX)==r->executable))
                return PWL_EFI_DEVICE_ERROR;
        }
        covered+=r->size;
    }
    if (covered!=im->image_size || !entry) return PWL_EFI_INVALID_PARAMETER;
    if (apply!=1) return PWL_EFI_SUCCESS;
    /* Caller keeps table graph exclusive between validation and this pass. */
    for (size_t i=0;i<im->range_count;i++) {
        const pwl_x64_identity_range_t *r=&im->ranges[i];
        for (uint64_t p=r->base;p<r->base+r->size;p+=4096) {
            uint64_t *slot=leaf(m,p,0);
            *slot=(*slot&~(NX|UINT64_C(2))) |
                ((restore || r->writable) ? 2 : 0) |
                ((restore || !r->executable) ? NX : 0);
        }
    }
    return PWL_EFI_SUCCESS;
}

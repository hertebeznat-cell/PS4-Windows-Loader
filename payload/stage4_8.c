/* Stage 4.8: low-address allocation bridge.
 *
 * Hardware Stage 4.7 showed that the requested 0x00102000 page is reported
 * free, yet an exact one-page mapping at that address is rejected.  This
 * revision tests a wider aligned backing window from 0x00100000 to 0x00103fff
 * and, only when that whole window is free, exposes the requested 0x00102000
 * page to the existing EFI allocator.
 *
 * The extra backing pages remain an implementation detail; EFI still sees the
 * single requested page.  The matching munmap path releases the full backing
 * window if that EFI page is later freed.
 */

#define mmap ps4wl_mmap48
#define munmap ps4wl_munmap48
#include "stage4_5.c"
#undef mmap
#undef munmap

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
int munmap(void *addr, size_t len);

#define WINDOW48_BASE 0x0000000000100000ULL
#define WINDOW48_SIZE 0x0000000000004000ULL
#define TARGET48_ADDR 0x0000000000102000ULL

static int window48_active;
static int trace48_busy;

static int window48_free(void)
{
    u64 a;
    for(a=WINDOW48_BASE;a<WINDOW48_BASE+WINDOW48_SIZE;a+=EFI_PAGE_SIZE){
        char vec=0;
        if(mincore((void*)(unsigned long)a,(size_t)EFI_PAGE_SIZE,&vec)==0)
            return 0;
    }
    return 1;
}

void *ps4wl_mmap48(void *addr,size_t len,int prot,int flags,int fd,off_t off)
{
    u64 requested=(u64)(unsigned long)addr;
    int target=addr && requested==TARGET48_ADDR &&
               len==(size_t)EFI_PAGE_SIZE && (flags&MAP_ANONYMOUS);
    void *p;

    if(target && !window48_active && window48_free()){
        p=mmap((void*)(unsigned long)WINDOW48_BASE,(size_t)WINDOW48_SIZE,
               prot,flags|MAP_FIXED,fd,off);
        if(!trace48_busy){
            trace48_busy=1;
            log45("MMAP48: aligned backing window attempt\n");
            log_hex45("MMAP48: window_base=",WINDOW48_BASE);
            log_hex45("MMAP48: window_size=",WINDOW48_SIZE);
            log_hex45("MMAP48: returned=",(u64)(unsigned long)p);
            trace48_busy=0;
        }
        if((u64)(unsigned long)p==WINDOW48_BASE){
            window48_active=1;
            log45("MMAP48: backing window ready; exposing requested page\n");
            return (void*)(unsigned long)TARGET48_ADDR;
        }
        if(p!=(void*)-1)
            (void)munmap(p,(size_t)WINDOW48_SIZE);
    }

    p=mmap(addr,len,prot,flags,fd,off);
    if(target && !trace48_busy){
        trace48_busy=1;
        log45("MMAP48: normal request fallback\n");
        log_hex45("MMAP48: requested=",requested);
        log_hex45("MMAP48: returned=",(u64)(unsigned long)p);
        trace48_busy=0;
    }
    return p;
}

int ps4wl_munmap48(void *addr,size_t len)
{
    u64 a=(u64)(unsigned long)addr;
    if(window48_active && a==TARGET48_ADDR && len==(size_t)EFI_PAGE_SIZE){
        int rc=munmap((void*)(unsigned long)WINDOW48_BASE,(size_t)WINDOW48_SIZE);
        log45("MMAP48: releasing aligned backing window\n");
        log_hex45("MMAP48: munmap_status=",(u64)(unsigned int)rc);
        if(rc==0) window48_active=0;
        return rc;
    }
    return munmap(addr,len);
}

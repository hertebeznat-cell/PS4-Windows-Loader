/* Stage 4.8: low-address allocation bridge.
 *
 * Hardware Stage 4.7 showed that the requested 0x00102000 page is reported
 * free, yet an exact one-page mapping at that address is rejected.  This
 * revision tests a wider aligned backing window from 0x00100000 to 0x00103fff
 * and, only when that whole window is free, exposes the requested 0x00102000
 * page to the existing EFI allocator.
 *
 * EFI sees the requested page plus reserved descriptors for the three backing
 * pages. The matching munmap path releases the full backing window.
 */

#include <signal.h>
#define PS4WL_STAGE48 1
#define PS4WL_TRACE_PATH "/mnt/usb0/PS4WL_STAGE48.LOG"
#define mmap ps4wl_mmap48
#define munmap ps4wl_munmap48
#include "stage4_5.c"
#undef mmap
#undef munmap

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
int munmap(void *addr, size_t len);
/* The pinned PS4 runtime exports errno and the raw __sysctl syscall. */
extern int errno;
int __sysctl(int *name, unsigned int namelen, void *oldp, size_t *oldlenp,
             void *newp, size_t newlen);
void sys_exit(int status);

#define WINDOW48_BASE 0x0000000000100000ULL
#define WINDOW48_SIZE 0x0000000000004000ULL
#define TARGET48_ADDR 0x0000000000102000ULL
#define PS4WL_ENOMEM 12
#define EFI_RESERVED_MEMORY_TYPE 0U
#define PS4WL_CTL_HW 6
#define PS4WL_HW_PAGESIZE 7

static int window48_active;
static int trace48_busy;
static size_t native_page_size48;
static int fault48_installed;

static void snapshot48(void)
{
    static const u64 offset=0x58660ULL;
    const u8 *image=(const u8*)loaded32.ImageBase;
    UINTN i;
    if(!image||loaded32.ImageSize<offset+32U)return;
    log45("MMAP48: bootmgfw bytes near previous fault offset\n");
    log_hex45("MMAP48: image offset=",offset);
    for(i=0;i<4;i++){
        u64 word=0;
        mem_copy(&word,image+offset+i*8U,sizeof(word));
        log_hex45("MMAP48: image word=",word);
    }
}

static void fault48(int signo,siginfo_t *info,void *context)
{
    u64 base=(u64)(unsigned long)loaded32.ImageBase;
    UINTN i;
    log45("FAULT48: synchronous processor fault\n");
    log_hex45("FAULT48: signal=",(u64)(u32)signo);
    log_hex45("FAULT48: siginfo ptr=",(u64)(unsigned long)info);
    log_hex45("FAULT48: context ptr=",(u64)(unsigned long)context);
    if(info){
        log_hex45("FAULT48: code=",(u64)(u32)info->si_code);
        log_hex45("FAULT48: address=",(u64)(unsigned long)info->si_addr);
        if(base && (u64)(unsigned long)info->si_addr>=base &&
           (u64)(unsigned long)info->si_addr-base<loaded32.ImageSize)
            log_hex45("FAULT48: image offset=",(u64)(unsigned long)info->si_addr-base);
    }
    if(context){
        const u64 *words=(const u64*)context;
        for(i=0;i<32;i++){
            char label[]="FAULT48: context[00]=";
            label[17]=(char)('0'+i/10U);
            label[18]=(char)('0'+i%10U);
            log_hex45(label,words[i]);
        }
    }
    /* Do not resume the instruction that faulted. */
    sys_exit(128+signo);
    for(;;){}
}

static void install_fault48(void)
{
    struct sigaction action;
    int signals[]={SIGSEGV,SIGBUS,SIGILL};
    UINTN i;
    if(fault48_installed)return;
    fault48_installed=1;
    mem_zero(&action,sizeof(action));
    action.sa_sigaction=fault48;
    action.sa_flags=SA_SIGINFO|SA_RESETHAND;
    for(i=0;i<(UINTN)(sizeof(signals)/sizeof(signals[0]));i++){
        int rc=sigaction(signals[i],&action,0);
        log_hex45("FAULT48: installing handler for signal=",(u64)(u32)signals[i]);
        log_hex45("FAULT48: sigaction status=",(u64)(u32)rc);
        if(rc!=0)log_hex45("FAULT48: sigaction errno=",(u64)(u32)errno);
    }
}

static size_t page_size48(void)
{
    int mib[2]={PS4WL_CTL_HW,PS4WL_HW_PAGESIZE};
    int value=0,rc,error;
    size_t length=sizeof(value);
    if(native_page_size48)return native_page_size48;
    errno=0;
    rc=__sysctl(mib,2,&value,&length,0,0);
    error=errno;
    log_hex45("MMAP48: hw.pagesize sysctl status=",(u64)(u32)rc);
    if(rc!=0){
        log_hex45("MMAP48: hw.pagesize errno=",(u64)(u32)error);
        return 0;
    }
    log_hex45("MMAP48: native page size=",(u64)(u32)value);
    if(length!=sizeof(value)||value<4096||value>WINDOW48_SIZE||
       ((u32)value&((u32)value-1U))!=0)
        return 0;
    native_page_size48=(size_t)value;
    return native_page_size48;
}

static int window48_free(void)
{
    u64 a;
    size_t granularity=page_size48();
    if(!granularity){
        log45("MMAP48: native page size unknown; skipping fixed mapping\n");
        return 0;
    }
    if((TARGET48_ADDR&(granularity-1U))!=0){
        log45("MMAP48: EFI address is not native-page aligned\n");
    }
    for(a=WINDOW48_BASE;a<WINDOW48_BASE+WINDOW48_SIZE;a+=(u64)granularity){
        char vec=0;
        int error;
        errno=0;
        if(mincore((void*)(unsigned long)a,granularity,&vec)==0){
            log_hex45("MMAP48: backing page occupied=",a);
            log_hex45("MMAP48: mincore vector=",(u64)(u8)vec);
            return 0;
        }
        error=errno;
        if(error!=PS4WL_ENOMEM){
            log_hex45("MMAP48: mincore unexpected errno=",(u64)(u32)error);
            return 0;
        }
    }
    return 1;
}

void *ps4wl_mmap48(void *addr,size_t len,int prot,int flags,int fd,off_t off)
{
    u64 requested=(u64)(unsigned long)addr;
    int target=addr && requested==TARGET48_ADDR &&
               len==(size_t)EFI_PAGE_SIZE && (flags&MAP_ANONYMOUS);
    void *p;

    if(target)install_fault48();
    if(target && !window48_active && window48_free()){
        errno=0;
        p=mmap((void*)(unsigned long)WINDOW48_BASE,(size_t)WINDOW48_SIZE,
               prot,flags|MAP_FIXED,fd,off);
        {
            int map_error=errno;
            if(!trace48_busy){
                trace48_busy=1;
                log45("MMAP48: aligned backing window attempt\n");
                log_hex45("MMAP48: window_base=",WINDOW48_BASE);
                log_hex45("MMAP48: window_size=",WINDOW48_SIZE);
                log_hex45("MMAP48: returned=",(u64)(unsigned long)p);
                if(p==(void*)-1)log_hex45("MMAP48: mmap errno=",(u64)(u32)map_error);
                trace48_busy=0;
            }
        }
        if((u64)(unsigned long)p==WINDOW48_BASE){
            window48_active=1;
            log45("MMAP48: backing window ready; exposing requested page\n");
            snapshot48();
            return (void*)(unsigned long)TARGET48_ADDR;
        }
        if(p!=(void*)-1)
            (void)munmap(p,(size_t)WINDOW48_SIZE);
    }

    errno=0;
    p=mmap(addr,len,prot,flags,fd,off);
    {
        int map_error=errno;
        if(target && !trace48_busy){
            trace48_busy=1;
            log45("MMAP48: normal request fallback\n");
            log_hex45("MMAP48: requested=",requested);
            log_hex45("MMAP48: returned=",(u64)(unsigned long)p);
            if(p==(void*)-1)log_hex45("MMAP48: fallback errno=",(u64)(u32)map_error);
            trace48_busy=0;
        }
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

/* Account for the three extra mapped pages, including on size-only queries. */
static EFI_STATUS EFIAPI get_map48(UINTN*size,EFI_MEMORY_DESCRIPTOR*map,
                                    UINTN*key,UINTN*ds,u32*ver)
{
    UINTN base_size=0,needed,count,i;
    EFI_STATUS rc;
    if(!size||!key||!ds||!ver)return EFI_INVALID_PARAMETER;
    rc=get_map45(&base_size,0,key,ds,ver);
    if(rc!=EFI_BUFFER_TOO_SMALL)return rc;
    needed=base_size+(window48_active?2U*sizeof(EFI_MEMORY_DESCRIPTOR):0U);
    if(!map||*size<needed){*size=needed;return EFI_BUFFER_TOO_SMALL;}
    rc=get_map45(&base_size,map,key,ds,ver);
    if(rc!=EFI_SUCCESS)return rc;
    if(window48_active){
        EFI_MEMORY_DESCRIPTOR *d;
        count=base_size/sizeof(EFI_MEMORY_DESCRIPTOR);
        d=&map[count];
        mem_zero(d,2U*sizeof(*d));
        d[0].Type=EFI_RESERVED_MEMORY_TYPE;
        d[0].PhysicalStart=WINDOW48_BASE;
        d[0].NumberOfPages=2;
        d[1].Type=EFI_RESERVED_MEMORY_TYPE;
        d[1].PhysicalStart=TARGET48_ADDR+EFI_PAGE_SIZE;
        d[1].NumberOfPages=1;
        for(i=count;i<count+2U;i++){
            EFI_MEMORY_DESCRIPTOR tmp=map[i];
            UINTN j=i;
            while(j && map[j-1U].PhysicalStart>tmp.PhysicalStart){
                map[j]=map[j-1U];--j;
            }
            map[j]=tmp;
        }
    }
    *size=needed;
    return EFI_SUCCESS;
}

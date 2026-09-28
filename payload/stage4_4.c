#define main stage4_3_base_main
#include "stage4_3.c"
#undef main

/*
 * Stage 4.4
 *
 * Hardware Stage 4.3 reached the third interesting firmware interaction:
 * Microsoft successfully consumed LoadedImage and the synthetic DevicePath,
 * then called AllocatePages(1 page).  The Stage 4.0 allocator only accepted
 * AllocateAnyPages and returned EFI_UNSUPPORTED for the other UEFI allocation
 * policies.
 *
 * Stage 4.4 replaces page allocation with a small real firmware-style arena
 * allocator supporting AllocateAnyPages, AllocateMaxAddress and
 * AllocateAddress, emits a non-overlapping memory map, and attempts to place
 * the conventional-memory arena below 4 GiB before bootmgfw is entered.
 *
 * Trace: /mnt/usb0/PS4WL_STAGE44.LOG
 */

#define EFI_ALLOCATE_MAX_ADDRESS 1U
#define EFI_ALLOCATE_ADDRESS     2U
#define ARENA44_MAX_PAGES        32768U

static u8 arena_type44[ARENA44_MAX_PAGES];

static void log44(const char *s){ log43(s); }
static void log_hex44(const char *tag,u64 v){ log_hex43(tag,v); }

static void init_arena44(void)
{
    UINTN i;
    for(i=0;i<ARENA44_MAX_PAGES;i++) arena_type44[i]=0xffU;
}

static int try_low_arena44(void)
{
    static const u64 hints[]={
        0x0000000040000000ULL,
        0x0000000060000000ULL,
        0x0000000080000000ULL,
        0x00000000A0000000ULL,
        0x0000000020000000ULL
    };
    size_t bytes=(size_t)(arena_pages40*EFI_PAGE_SIZE);
    UINTN i;
    u64 cur=(u64)(unsigned long)arena32;

    log_hex44("ARENA44: original base=",cur);
    log_hex44("ARENA44: pages=",arena_pages40);

    if(cur+bytes<=0x100000000ULL){
        log44("ARENA44: existing arena already below 4GiB\n");
        return 1;
    }

    for(i=0;i<(UINTN)(sizeof(hints)/sizeof(hints[0]));i++){
        void *want=(void*)(unsigned long)hints[i];
        void *p;
        if(hints[i]+bytes>0x100000000ULL) continue;
        log_hex44("ARENA44: trying hint=",hints[i]);
        p=mmap(want,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
        if(p==(void*)-1){
            log44("ARENA44: mmap failed\n");
            continue;
        }
        if(p==want){
            void *old=arena32;
            size_t old_bytes=(size_t)(arena_pages40*EFI_PAGE_SIZE);
            arena32=p;
            if(old && old!=(void*)-1) (void)munmap(old,old_bytes);
            log_hex44("ARENA44: low arena installed at=",(u64)(unsigned long)arena32);
            return 1;
        }
        log_hex44("ARENA44: hint not honored, got=",(u64)(unsigned long)p);
        (void)munmap(p,bytes);
    }

    log44("ARENA44: low arena unavailable; keeping original arena\n");
    return 0;
}

static int arena_range_free44(UINTN start,UINTN pages)
{
    UINTN i;
    if(!pages || start>=arena_pages40 || start+pages>arena_pages40) return 0;
    for(i=0;i<pages;i++) if(arena_type44[start+i]!=0xffU) return 0;
    return 1;
}

static void arena_mark44(UINTN start,UINTN pages,u32 type)
{
    UINTN i;
    for(i=0;i<pages;i++) arena_type44[start+i]=(u8)type;
}

static void arena_unmark44(UINTN start,UINTN pages)
{
    UINTN i;
    for(i=0;i<pages;i++) arena_type44[start+i]=0xffU;
}

static EFI_STATUS arena_alloc_at44(UINTN start,u32 mt,UINTN pages,EFI_PHYSICAL_ADDRESS*out)
{
    if(mt>=0xffU) return EFI_INVALID_PARAMETER;
    if(!arena_range_free44(start,pages)) return EFI_NOT_FOUND;
    arena_mark44(start,pages,mt);
    *out=(EFI_PHYSICAL_ADDRESS)((u64)(unsigned long)arena32+(u64)start*EFI_PAGE_SIZE);
    mem_zero((void*)(unsigned long)(*out),(size_t)(pages*EFI_PAGE_SIZE));
    ++map_key32;
    return EFI_SUCCESS;
}

static EFI_STATUS arena_alloc_any44(u32 mt,UINTN pages,EFI_PHYSICAL_ADDRESS*out)
{
    UINTN start;
    if(!pages || pages>arena_pages40) return EFI_OUT_OF_RESOURCES;
    for(start=0;start+pages<=arena_pages40;start++){
        if(arena_range_free44(start,pages)) return arena_alloc_at44(start,mt,pages,out);
    }
    return EFI_OUT_OF_RESOURCES;
}

static EFI_STATUS arena_alloc_max44(u32 mt,UINTN pages,u64 maxaddr,EFI_PHYSICAL_ADDRESS*out)
{
    u64 base=(u64)(unsigned long)arena32;
    u64 bytes=(u64)pages*EFI_PAGE_SIZE;
    u64 arena_end=base+(u64)arena_pages40*EFI_PAGE_SIZE;
    u64 limit_end;
    u64 candidate;
    UINTN start;

    if(!pages || pages>arena_pages40) return EFI_NOT_FOUND;
    if(maxaddr>=arena_end-1ULL) limit_end=arena_end;
    else {
        if(maxaddr==0xffffffffffffffffULL) limit_end=arena_end;
        else limit_end=maxaddr+1ULL;
    }
    if(limit_end<=base || bytes>limit_end-base) return EFI_NOT_FOUND;

    candidate=(limit_end-bytes)&~(EFI_PAGE_SIZE-1ULL);
    if(candidate<base) return EFI_NOT_FOUND;
    start=(UINTN)((candidate-base)/EFI_PAGE_SIZE);

    for(;;){
        if(start+pages<=arena_pages40 && arena_range_free44(start,pages))
            return arena_alloc_at44(start,mt,pages,out);
        if(start==0) break;
        --start;
    }
    return EFI_NOT_FOUND;
}

static EFI_STATUS external_exact44(u32 mt,UINTN pages,u64 requested,EFI_PHYSICAL_ADDRESS*out)
{
    ALLOC40*r;
    void*p;
    size_t bytes;
    if(!pages || (requested&(EFI_PAGE_SIZE-1ULL))) return EFI_INVALID_PARAMETER;
    r=free_slot40(); if(!r) return EFI_OUT_OF_RESOURCES;
    bytes=(size_t)(pages*EFI_PAGE_SIZE);
    p=mmap((void*)(unsigned long)requested,bytes,PROT_READ|PROT_WRITE|PROT_EXEC,
           MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(p==(void*)-1) return EFI_NOT_FOUND;
    if((u64)(unsigned long)p!=requested){ (void)munmap(p,bytes); return EFI_NOT_FOUND; }
    r->used=1;r->pool=0;r->base=p;r->pages=pages;r->type=mt;
    *out=(EFI_PHYSICAL_ADDRESS)requested;
    mem_zero(p,bytes);
    ++map_key32;
    return EFI_SUCCESS;
}

static EFI_STATUS external_max44(u32 mt,UINTN pages,u64 maxaddr,EFI_PHYSICAL_ADDRESS*out)
{
    ALLOC40*r;
    size_t bytes;
    u64 top,step=0x01000000ULL;
    UINTN tries=0;

    if(!pages) return EFI_INVALID_PARAMETER;
    bytes=(size_t)(pages*EFI_PAGE_SIZE);
    if((u64)bytes-1ULL>maxaddr) return EFI_NOT_FOUND;
    r=free_slot40(); if(!r) return EFI_OUT_OF_RESOURCES;

    top=maxaddr-(u64)bytes+1ULL;
    top&=~(EFI_PAGE_SIZE-1ULL);
    if(top>0x00000000F0000000ULL) top=0x00000000F0000000ULL;

    while(top>=0x01000000ULL && tries<96U){
        void*p=mmap((void*)(unsigned long)top,bytes,PROT_READ|PROT_WRITE|PROT_EXEC,
                    MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
        if(p!=(void*)-1){
            u64 a=(u64)(unsigned long)p;
            if(a+(u64)bytes-1ULL<=maxaddr){
                r->used=1;r->pool=0;r->base=p;r->pages=pages;r->type=mt;
                *out=(EFI_PHYSICAL_ADDRESS)a;
                mem_zero(p,bytes);
                ++map_key32;
                return EFI_SUCCESS;
            }
            (void)munmap(p,bytes);
        }
        if(top<step+0x01000000ULL) break;
        top-=step;
        ++tries;
    }
    return EFI_NOT_FOUND;
}

static EFI_STATUS EFIAPI alloc_pages44(u32 at,u32 mt,UINTN pages,EFI_PHYSICAL_ADDRESS*out)
{
    EFI_STATUS rc;
    u64 requested;
    if(!out||!pages) return EFI_INVALID_PARAMETER;
    requested=*out;

    log44("AP44: AllocatePages\n");
    log_hex44("AP44: type=",(u64)at);
    log_hex44("AP44: memtype=",(u64)mt);
    log_hex44("AP44: pages=",pages);
    log_hex44("AP44: requested=",requested);

    if(at==EFI_ALLOCATE_ANY_PAGES){
        rc=arena_alloc_any44(mt,pages,out);
        if(rc!=EFI_SUCCESS) rc=alloc_pages40(EFI_ALLOCATE_ANY_PAGES,mt,pages,out);
    } else if(at==EFI_ALLOCATE_MAX_ADDRESS){
        rc=arena_alloc_max44(mt,pages,requested,out);
        if(rc!=EFI_SUCCESS) rc=external_max44(mt,pages,requested,out);
    } else if(at==EFI_ALLOCATE_ADDRESS){
        u64 base=(u64)(unsigned long)arena32;
        u64 end=base+(u64)arena_pages40*EFI_PAGE_SIZE;
        u64 bytes=(u64)pages*EFI_PAGE_SIZE;
        if((requested&(EFI_PAGE_SIZE-1ULL))==0 && requested>=base &&
           requested<end && bytes<=end-requested){
            UINTN start=(UINTN)((requested-base)/EFI_PAGE_SIZE);
            rc=arena_alloc_at44(start,mt,pages,out);
        } else rc=external_exact44(mt,pages,requested,out);
    } else rc=EFI_INVALID_PARAMETER;

    log_hex44("AP44: status=",rc);
    if(rc==EFI_SUCCESS) log_hex44("AP44: result=",*out);
    return rc;
}

static EFI_STATUS EFIAPI free_pages44(EFI_PHYSICAL_ADDRESS addr,UINTN pages)
{
    u64 base=(u64)(unsigned long)arena32;
    u64 end=base+(u64)arena_pages40*EFI_PAGE_SIZE;
    u64 bytes=(u64)pages*EFI_PAGE_SIZE;
    EFI_STATUS rc;

    log44("FP44: FreePages\n");
    log_hex44("FP44: addr=",addr);
    log_hex44("FP44: pages=",pages);

    if(!pages) rc=EFI_INVALID_PARAMETER;
    else if(addr>=base && addr<end && bytes<=end-addr &&
            ((addr-base)&(EFI_PAGE_SIZE-1ULL))==0){
        UINTN start=(UINTN)((addr-base)/EFI_PAGE_SIZE),i;
        rc=EFI_SUCCESS;
        for(i=0;i<pages;i++) if(arena_type44[start+i]==0xffU){rc=EFI_INVALID_PARAMETER;break;}
        if(rc==EFI_SUCCESS){arena_unmark44(start,pages);++map_key32;}
    } else rc=free_pages40(addr,pages);

    log_hex44("FP44: status=",rc);
    return rc;
}

static UINTN arena_runs44(void)
{
    UINTN i=0,n=0;
    while(i<arena_pages40){u8 t=arena_type44[i];++n;do{++i;}while(i<arena_pages40&&arena_type44[i]==t);}
    return n;
}

static EFI_STATUS EFIAPI get_map44(UINTN*size,EFI_MEMORY_DESCRIPTOR*map,UINTN*key,UINTN*ds,u32*ver)
{
    UINTN i,n,need,pos=0;
    EFI_MEMORY_DESCRIPTOR*d;

    if(!size||!key||!ds||!ver) return EFI_INVALID_PARAMETER;
    n=arena_runs44();
    for(i=0;i<MAX_ALLOCS40;i++) if(alloc40[i].used) ++n;
    need=n*(UINTN)sizeof(EFI_MEMORY_DESCRIPTOR);
    *ds=sizeof(EFI_MEMORY_DESCRIPTOR);*ver=1;*key=map_key32;

    log44("MM44: GetMemoryMap\n");
    log_hex44("MM44: supplied_size=",*size);
    log_hex44("MM44: required_size=",need);
    log_hex44("MM44: key=",*key);

    if(!map||*size<need){*size=need;log44("MM44: EFI_BUFFER_TOO_SMALL\n");return EFI_BUFFER_TOO_SMALL;}
    mem_zero(map,(size_t)need);

    i=0;
    while(i<arena_pages40){
        UINTN start=i,count;
        u8 t=arena_type44[i];
        do{++i;}while(i<arena_pages40&&arena_type44[i]==t);
        count=i-start;
        d=(EFI_MEMORY_DESCRIPTOR*)((u8*)map+pos);
        d->Type=(t==0xffU)?EFI_CONVENTIONAL_MEMORY:(u32)t;
        d->PhysicalStart=(u64)(unsigned long)arena32+(u64)start*EFI_PAGE_SIZE;
        d->VirtualStart=0;
        d->NumberOfPages=count;
        d->Attribute=0;
        pos+=sizeof(*d);
    }

    for(i=0;i<MAX_ALLOCS40;i++) if(alloc40[i].used){
        d=(EFI_MEMORY_DESCRIPTOR*)((u8*)map+pos);
        d->Type=alloc40[i].type;
        d->PhysicalStart=(u64)(unsigned long)alloc40[i].base;
        d->VirtualStart=0;
        d->NumberOfPages=alloc40[i].pages;
        d->Attribute=0;
        pos+=sizeof(*d);
    }

    /* Windows firmware consumers are happier with monotonically sorted maps. */
    if(n>1){
        UINTN a,b;
        for(a=1;a<n;a++){
            EFI_MEMORY_DESCRIPTOR tmp=map[a];
            b=a;
            while(b>0&&map[b-1].PhysicalStart>tmp.PhysicalStart){map[b]=map[b-1];--b;}
            map[b]=tmp;
        }
    }

    *size=need;
    log44("MM44: EFI_SUCCESS\n");
    return EFI_SUCCESS;
}

static void open_trace44(void)
{
    trace_fd43=open("/mnt/usb0/PS4WL_STAGE44.LOG",O_WRONLY|O_CREAT|O_TRUNC,0666);
    if(trace_fd43>=0){
        log44("PS4 Windows Loader Stage 4.4 trace\n");
        log44("UEFI AllocatePages policy support enabled\n");
    }
}

static void install_stage44_services(void)
{
    bs32.AllocatePages=alloc_pages44;
    bs32.FreePages=free_pages44;
    bs32.GetMemoryMap=get_map44;
    refresh_crc40();
}

int main(void)
{
    u8*file=0,*image=0;size_t file_size=0;struct pe_info pe;UINTN pages;
    EFI_PHYSICAL_ADDRESS imgaddr=0;efi_entry40_fn entry;EFI_STATUS rc;

    notify("PS4 Windows Loader: Stage 4.4 started");
    build_efi_shim();
    if(build32()!=0){notify("Stage 4.4: EFI environment build FAILED");return 1;}
    install_stage35_services();
    install_stage40_services();
    build_device_path43();
    open_trace44();
    (void)try_low_arena44();
    init_arena44();
    install_stage43_services();
    install_stage44_services();

    log_hex44("ARENA44: active base=",(u64)(unsigned long)arena32);
    log_hex44("ARENA44: active pages=",arena_pages40);
    log_dp43("DP44: DeviceHandle path ready\n",(EFI_DEVICE_PATH_PROTOCOL_32*)device_path43.bytes);
    notify("Stage 4.4: AllocatePages + memory map upgrade installed");

    if(load_bootmgfw(&file,&file_size)!=0||inspect_pe(file,file_size,&pe)!=0){
        if(file)munmap(file,(size_t)READ_CAP);log44("Stage 4.4: bootmgfw read/PE FAILED\n");
        if(trace_fd43>=0)close(trace_fd43);return 1;
    }

    pages=(pe.image_size+4095U)/4096U;
    if(alloc_pages44(EFI_ALLOCATE_ANY_PAGES,EFI_LOADER_CODE,pages,&imgaddr)!=EFI_SUCCESS){
        munmap(file,(size_t)READ_CAP);log44("Stage 4.4: bootmgfw AllocatePages FAILED\n");
        if(trace_fd43>=0)close(trace_fd43);return 1;
    }

    image=(u8*)(unsigned long)imgaddr;
    if(map_sections40(file,file_size,&pe,image)!=0||apply_relocs(image,&pe)!=0){
        free_pages44(imgaddr,pages);munmap(file,(size_t)READ_CAP);log44("Stage 4.4: map/relocations FAILED\n");
        if(trace_fd43>=0)close(trace_fd43);return 1;
    }

    build_loaded_path40();
    loaded32.SystemTable=&g_system_table;
    loaded32.DeviceHandle=(EFI_HANDLE)&device_handle32;
    loaded32.FilePath=(EFI_DEVICE_PATH_PROTOCOL_32*)loaded_path40.bytes;
    loaded32.ImageBase=image;
    loaded32.ImageSize=pe.image_size;
    loaded32.ImageCodeType=EFI_LOADER_CODE;
    loaded32.ImageDataType=EFI_LOADER_DATA;
    entry=(efi_entry40_fn)(image+pe.entry_rva);
    refresh_crc40();

    log_hex44("ENTRY44: ImageHandle=",(u64)(unsigned long)&image_handle32);
    log_hex44("ENTRY44: DeviceHandle=",(u64)(unsigned long)&device_handle32);
    log_hex44("ENTRY44: ImageBase=",(u64)(unsigned long)image);
    log_hex44("ENTRY44: bootmgfw entry=",(u64)(unsigned long)entry);
    log44("ENTRY44: entering Microsoft bootmgfw.efi\n");
    notify("Stage 4.4: ENTERING Microsoft bootmgfw.efi NOW");

    rc=entry((EFI_HANDLE)&image_handle32,&g_system_table);

    log44("ENTRY44: Microsoft bootmgfw.efi returned\n");
    log_hex44("ENTRY44: status=",rc);
    notify("Stage 4.4: Microsoft bootmgfw.efi RETURNED");
    notify_status40(rc);

    if(trace_fd43>=0)close(trace_fd43);
    free_pages44(imgaddr,pages);munmap(file,(size_t)READ_CAP);
    munmap(arena32,(size_t)(arena_pages40*EFI_PAGE_SIZE));
    return 0;
}

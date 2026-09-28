#define STAGE4_1_NO_MAIN 1
#include "stage4_1.c"

/*
 * Stage 4.3
 *
 * Stage 4.2 proved that the first two Microsoft HandleProtocol calls complete:
 *   1) ImageHandle + LoadedImage
 *   2) DeviceHandle + DevicePath
 * and then execution stops after Microsoft receives the DevicePath interface.
 *
 * Stage 4.3 replaces the old end-node-only DevicePath with a real, non-empty,
 * syntactically valid UEFI hardware vendor device path and traces the next EFI
 * calls to /mnt/usb0/PS4WL_STAGE43.LOG.
 */

static int trace_fd43 = -1;
static volatile u64 hp_calls43;
static union { u64 align; u8 bytes[32]; } device_path43;
static const EFI_GUID ps4_device_guid43 = {
    0x50533457U, 0x4c44U, 0x5231U,
    {0x42,0x41,0x49,0x4b,0x41,0x4c,0x00,0x01}
};

static size_t len43(const char *s)
{
    size_t n=0; while(s && s[n]) ++n; return n;
}

static void log43(const char *s)
{
    if(trace_fd43>=0 && s){size_t n=len43(s);if(n)(void)write(trace_fd43,s,n);}
}

static void log_hex43(const char *tag,u64 value)
{
    char b[96]; static const char h[]="0123456789ABCDEF"; size_t p=0,i;
    while(tag&&*tag&&p+1<sizeof(b))b[p++]=*tag++;
    if(p+19>=sizeof(b))return;
    b[p++]='0';b[p++]='x';
    for(i=0;i<16;i++)b[p++]=h[(value>>(60U-(u32)i*4U))&0xfU];
    b[p++]='\n';b[p]=0;log43(b);
}

static int guid_equal43(const EFI_GUID *a,const EFI_GUID *b)
{
    const u8*x=(const u8*)a,*y=(const u8*)b;UINTN i;
    if(!a||!b)return 0;
    for(i=0;i<(UINTN)sizeof(EFI_GUID);i++)if(x[i]!=y[i])return 0;
    return 1;
}

static void build_device_path43(void)
{
    u8*b=device_path43.bytes;
    mem_zero(b,sizeof(device_path43.bytes));

    /* Hardware Device Path / Vendor subtype / 20-byte node. */
    b[0]=0x01;
    b[1]=0x04;
    b[2]=20;
    b[3]=0;
    mem_copy(b+4,&ps4_device_guid43,sizeof(EFI_GUID));

    /* End Entire node. */
    b[20]=0x7f;
    b[21]=0xff;
    b[22]=4;
    b[23]=0;
}

static void log_dp43(const char *tag,EFI_DEVICE_PATH_PROTOCOL_32*p)
{
    log43(tag);
    log_hex43("DP43: ptr=",(u64)(unsigned long)p);
    if(!p){log43("DP43: NULL\n");return;}
    log_hex43("DP43: type=",(u64)p->Type);
    log_hex43("DP43: subtype=",(u64)p->SubType);
    log_hex43("DP43: length=",(u64)((u16)p->Length[0]|((u16)p->Length[1]<<8)));
}

static EFI_STATUS EFIAPI handle43(EFI_HANDLE h,EFI_GUID*g,void**out)
{
    u64 n=++hp_calls43;
    log43("HP43: enter\n");
    log_hex43("HP43: call=",n);
    log_hex43("HP43: handle=",(u64)(unsigned long)h);
    log_hex43("HP43: guid_ptr=",(u64)(unsigned long)g);
    log_hex43("HP43: out_ptr=",(u64)(unsigned long)out);
    if(!g||!out){log43("HP43: invalid parameter\n");return EFI_INVALID_PARAMETER;}
    log_hex43("HP43: guid.Data1=",(u64)g->Data1);

    if(h==(EFI_HANDLE)&image_handle32){
        log43("HP43: ImageHandle\n");
        if(guid_equal43(g,&loaded_guid32)){
            log43("HP43: LoadedImage matched\n");
            if(loaded32.SystemTable!=&g_system_table||!loaded32.ImageBase||!loaded32.ImageSize||
               loaded32.DeviceHandle!=(EFI_HANDLE)&device_handle32||!loaded32.FilePath){
                log43("HP43: LoadedImage metadata invalid\n");return EFI_DEVICE_ERROR;
            }
            *out=&loaded32;
            log_hex43("HP43: LoadedImage iface=",(u64)(unsigned long)*out);
            log43("HP43: return EFI_SUCCESS\n");
            return EFI_SUCCESS;
        }
        log43("HP43: unsupported protocol on ImageHandle\n");
        return EFI_UNSUPPORTED;
    }

    if(h==(EFI_HANDLE)&device_handle32){
        log43("HP43: DeviceHandle\n");
        if(guid_equal43(g,&device_path_guid32)){
            *out=(void*)device_path43.bytes;
            log43("HP43: DevicePath matched\n");
            log_dp43("HP43: returning synthetic non-empty device path\n",(EFI_DEVICE_PATH_PROTOCOL_32*)*out);
            log43("HP43: return EFI_SUCCESS\n");
            return EFI_SUCCESS;
        }
        if(guid_equal43(g,&simple_fs_guid32)){
            *out=&fs32;
            log_hex43("HP43: SimpleFS iface=",(u64)(unsigned long)*out);
            log43("HP43: return EFI_SUCCESS\n");
            return EFI_SUCCESS;
        }
        log43("HP43: UNKNOWN protocol on DeviceHandle\n");
        log_hex43("HP43: unknown GUID Data1=",(u64)g->Data1);
        return EFI_UNSUPPORTED;
    }

    log43("HP43: unknown handle\n");
    return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI open_protocol43(EFI_HANDLE h,EFI_GUID*g,void**iface,
                                          EFI_HANDLE agent,EFI_HANDLE controller,u32 attr)
{
    EFI_STATUS rc;void*tmp=0;(void)agent;(void)controller;
    log43("OP43: enter\n");
    log_hex43("OP43: attr=",(u64)attr);
    if(!h||!g)return EFI_INVALID_PARAMETER;
    if(attr==EFI_OPEN_PROTOCOL_TEST_PROTOCOL){rc=handle43(h,g,&tmp);log_hex43("OP43: status=",rc);return rc;}
    if(attr!=EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL&&attr!=EFI_OPEN_PROTOCOL_GET_PROTOCOL)return EFI_UNSUPPORTED;
    if(!iface)return EFI_INVALID_PARAMETER;
    rc=handle43(h,g,iface);log_hex43("OP43: status=",rc);return rc;
}

static EFI_STATUS EFIAPI locate_protocol43(EFI_GUID*g,void*reg,void**out)
{
    EFI_STATUS rc;(void)reg;log43("LP43: enter\n");if(g)log_hex43("LP43: guid.Data1=",(u64)g->Data1);
    if(!g||!out)return EFI_INVALID_PARAMETER;
    if(guid_equal43(g,&loaded_guid32))*out=&loaded32;
    else if(guid_equal43(g,&device_path_guid32))*out=(void*)device_path43.bytes;
    else if(guid_equal43(g,&simple_fs_guid32))*out=&fs32;
    else {log43("LP43: unsupported\n");return EFI_NOT_FOUND;}
    rc=EFI_SUCCESS;log_hex43("LP43: iface=",(u64)(unsigned long)*out);return rc;
}

static EFI_STATUS EFIAPI locate_device_path43(EFI_GUID*g,EFI_DEVICE_PATH_PROTOCOL_32**path,EFI_HANDLE*dev)
{
    EFI_DEVICE_PATH_PROTOCOL_32*p;u16 l;
    log43("LDP43: enter\n");if(g)log_hex43("LDP43: guid.Data1=",(u64)g->Data1);
    if(!g||!path||!*path||!dev)return EFI_INVALID_PARAMETER;
    p=*path;log_dp43("LDP43: incoming path\n",p);
    if(!guid_equal43(g,&simple_fs_guid32)&&!guid_equal43(g,&device_path_guid32))return EFI_NOT_FOUND;
    if(p->Type==0x01&&p->SubType==0x04){
        l=(u16)p->Length[0]|((u16)p->Length[1]<<8);
        if(l<4)return EFI_NOT_FOUND;
        *dev=(EFI_HANDLE)&device_handle32;
        *path=(EFI_DEVICE_PATH_PROTOCOL_32*)((u8*)p+l);
        log43("LDP43: matched synthetic device prefix and advanced path\n");
        log_dp43("LDP43: remaining path\n",*path);
        return EFI_SUCCESS;
    }
    log43("LDP43: no prefix match\n");return EFI_NOT_FOUND;
}

static EFI_STATUS EFIAPI locate_handle43(u32 type,EFI_GUID*g,void*key,UINTN*sz,EFI_HANDLE*buf)
{EFI_STATUS rc;log43("LH43: enter\n");if(g)log_hex43("LH43: guid.Data1=",(u64)g->Data1);rc=locate_handle40(type,g,key,sz,buf);log_hex43("LH43: status=",rc);return rc;}
static EFI_STATUS EFIAPI locate_handle_buffer43(u32 type,EFI_GUID*g,void*key,UINTN*count,EFI_HANDLE**buf)
{EFI_STATUS rc;log43("LHB43: enter\n");if(g)log_hex43("LHB43: guid.Data1=",(u64)g->Data1);rc=locate_handle_buffer40(type,g,key,count,buf);log_hex43("LHB43: status=",rc);return rc;}
static EFI_STATUS EFIAPI protocols_per_handle43(EFI_HANDLE h,EFI_GUID***buf,UINTN*count)
{EFI_STATUS rc;log43("PPH43: enter\n");rc=protocols_per_handle40(h,buf,count);log_hex43("PPH43: status=",rc);return rc;}

static EFI_STATUS EFIAPI alloc_pages43(u32 at,u32 mt,UINTN pages,EFI_PHYSICAL_ADDRESS*out)
{EFI_STATUS rc;log43("AP43: AllocatePages\n");log_hex43("AP43: pages=",pages);rc=alloc_pages40(at,mt,pages,out);log_hex43("AP43: status=",rc);return rc;}
static EFI_STATUS EFIAPI alloc_pool43(u32 mt,UINTN size,void**out)
{EFI_STATUS rc;log43("APL43: AllocatePool\n");log_hex43("APL43: size=",size);rc=alloc_pool40(mt,size,out);log_hex43("APL43: status=",rc);return rc;}
static EFI_STATUS EFIAPI get_map43(UINTN*s,EFI_MEMORY_DESCRIPTOR*m,UINTN*k,UINTN*ds,u32*v)
{EFI_STATUS rc;log43("MM43: GetMemoryMap\n");rc=get_map40(s,m,k,ds,v);log_hex43("MM43: status=",rc);return rc;}

static EFI_STATUS EFIAPI file_open43(EFI_FILE_PROTOCOL_32*p,EFI_FILE_PROTOCOL_32**n,CHAR16*name,u64 mode,u64 attrs)
{EFI_STATUS rc;log43("FILE43: Open\n");rc=file_open40(p,n,name,mode,attrs);log_hex43("FILE43: status=",rc);if(rc==EFI_SUCCESS&&n&&*n)(*n)->Open=file_open43;return rc;}
static EFI_STATUS EFIAPI get_variable43(CHAR16*n,EFI_GUID*v,u32*a,UINTN*s,void*d)
{EFI_STATUS rc;log43("RT43: GetVariable\n");if(v)log_hex43("RT43: guid.Data1=",(u64)v->Data1);rc=rt_get_variable35(n,v,a,s,d);log_hex43("RT43: status=",rc);return rc;}
static EFI_STATUS EFIAPI get_time43(EFI_TIME_35*t,EFI_TIME_CAPABILITIES_35*c)
{EFI_STATUS rc;log43("RT43: GetTime\n");rc=get_time40(t,c);log_hex43("RT43: status=",rc);return rc;}

static void install_stage43_services(void)
{
    bs32.HandleProtocol=handle43;
    bs32.OpenProtocol=(void*)open_protocol43;
    bs32.LocateProtocol=locate_protocol43;
    bs32.LocateDevicePath=(void*)locate_device_path43;
    bs32.LocateHandle=(void*)locate_handle43;
    bs32.LocateHandleBuffer=(void*)locate_handle_buffer43;
    bs32.ProtocolsPerHandle=(void*)protocols_per_handle43;
    bs32.AllocatePages=alloc_pages43;
    bs32.AllocatePool=alloc_pool43;
    bs32.GetMemoryMap=get_map43;
    root32.proto.Open=file_open43;
    rt35.GetVariable=get_variable43;
    rt35.GetTime=get_time43;
    rt35.Hdr.CRC32=0;rt35.Hdr.CRC32=crc32_bytes(&rt35,sizeof(rt35));
    refresh_crc40();
}

static void open_trace43(void)
{
    trace_fd43=open("/mnt/usb0/PS4WL_STAGE43.LOG",O_WRONLY|O_CREAT|O_TRUNC,0666);
    if(trace_fd43>=0){
        log43("PS4 Windows Loader Stage 4.3 trace\n");
        log43("Synthetic non-empty UEFI DevicePath enabled\n");
    }
}

#ifndef STAGE4_3_NO_MAIN
int main(void)
{
    u8*file=0,*image=0;size_t file_size=0;struct pe_info pe;UINTN pages;
    EFI_PHYSICAL_ADDRESS imgaddr=0;efi_entry40_fn entry;EFI_STATUS rc;

    notify("PS4 Windows Loader: Stage 4.3 started");
    build_efi_shim();
    if(build32()!=0){notify("Stage 4.3: EFI environment build FAILED");return 1;}
    install_stage35_services();
    install_stage40_services();
    build_device_path43();
    open_trace43();
    install_stage43_services();

    log_dp43("DP43: synthetic DeviceHandle path ready\n",(EFI_DEVICE_PATH_PROTOCOL_32*)device_path43.bytes);
    notify("Stage 4.3: synthetic DevicePath + deep trace installed");

    if(load_bootmgfw(&file,&file_size)!=0||inspect_pe(file,file_size,&pe)!=0){
        if(file)munmap(file,(size_t)READ_CAP);log43("Stage 4.3: bootmgfw read/PE FAILED\n");
        if(trace_fd43>=0)close(trace_fd43);return 1;
    }

    pages=(pe.image_size+4095U)/4096U;
    if(alloc_pages43(EFI_ALLOCATE_ANY_PAGES,EFI_LOADER_CODE,pages,&imgaddr)!=EFI_SUCCESS){
        munmap(file,(size_t)READ_CAP);log43("Stage 4.3: AllocatePages FAILED\n");
        if(trace_fd43>=0)close(trace_fd43);return 1;
    }

    image=(u8*)(unsigned long)imgaddr;
    if(map_sections40(file,file_size,&pe,image)!=0||apply_relocs(image,&pe)!=0){
        free_pages40(imgaddr,pages);munmap(file,(size_t)READ_CAP);log43("Stage 4.3: map/relocations FAILED\n");
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

    log_hex43("ENTRY43: ImageHandle=",(u64)(unsigned long)&image_handle32);
    log_hex43("ENTRY43: DeviceHandle=",(u64)(unsigned long)&device_handle32);
    log_hex43("ENTRY43: DevicePath=",(u64)(unsigned long)device_path43.bytes);
    log_hex43("ENTRY43: bootmgfw entry=",(u64)(unsigned long)entry);
    log43("ENTRY43: entering Microsoft bootmgfw.efi\n");
    notify("Stage 4.3: ENTERING Microsoft bootmgfw.efi NOW");

    rc=entry((EFI_HANDLE)&image_handle32,&g_system_table);

    log43("ENTRY43: Microsoft bootmgfw.efi returned\n");
    log_hex43("ENTRY43: status=",rc);
    notify("Stage 4.3: Microsoft bootmgfw.efi RETURNED");
    notify_status40(rc);

    if(trace_fd43>=0)close(trace_fd43);
    free_pages40(imgaddr,pages);munmap(file,(size_t)READ_CAP);
    munmap(arena32,(size_t)(arena_pages40*EFI_PAGE_SIZE));
    return 0;
}
#endif

#include "ps4.h"
#include "syscall.h"
#include "pwl_native_workspace.h"
#include "pwl_ps4_reader.h"
#include "resident_fixture.h"
#ifndef PS4WL_BUILD_ID
#define PS4WL_BUILD_ID "local"
#endif
extern unsigned char _start[],__pwl_image_end[];
/* This entry prepares actual resident Boot Manager bytes and returns. It does
 * not pretend the incomplete platform inventory is a CPU activation contract. */
static const char mode[]="MODE: NATIVE_PREPARATION_ONLY; Microsoft entry not called";
static pwl_ps4_reader_t reader;
static pwl_ps4_binding_context_t binding;
static pwl_ps4_memory_api_t memory;
static pwl_native_workspace_t workspace;
static pwl_native_request_t request;
static pwl_resident_image_t image;
static volatile struct {
    int binding,prepare,audit,release;
    unsigned stage,retained;
    uint64_t physical,bytes,entry;
} result;
static int prepare_callback(struct thread *td,void *args)
{
    (void)args;uint64_t current;
    __asm__ volatile("mov %0,qword ptr gs:0":"=r"(current));
    if(current!=(uint64_t)td) {result.binding=PWL_ERR_ACCESS_DENIED;return 0;}
    result.stage=1;
    result.binding=pwl_ps4_memory_bind_checked(1352,&binding,&memory);
    if(result.binding!=PWL_OK)return 0;
    result.stage=2;
    static const uint16_t path[]={'\\','E','F','I','\\','M','I','C','R','O','S','O','F','T','\\',
        'B','O','O','T','\\','B','O','O','T','M','G','F','W','.','E','F','I',0};
    result.prepare=pwl_native_boot_prepare(&memory,&request,&image,path,&workspace);
    if(result.prepare==PWL_OK) {
        result.stage=3;result.physical=workspace.arena.physical_address;
        result.bytes=workspace.arena.size;result.entry=workspace.boot_image.entry_address;
        result.audit=pwl_native_resident_environment_validate(&workspace,&image);
    }
    if(workspace.arena.kernel_address) {
        result.stage=4;result.release=pwl_native_workspace_release(&workspace);
        result.retained=workspace.arena.kernel_address!=0;
        if(result.release!=PWL_OK)return 0;
    }
    if(result.prepare==PWL_OK && result.audit==PWL_OK)result.stage=5;
    return 0;
}
static int read_archive(void **buffer,size_t *bytes)
{
    int fd=open("/mnt/usb0/PWL_BOOT.PAK",O_RDONLY,0);
    if(fd<0)fd=open("/mnt/usb1/PWL_BOOT.PAK",O_RDONLY,0);
    if(fd<0)return -1;
    off_t length=lseek(fd,0,SEEK_END);
    if(length<512 || (uint64_t)length>64U*1024U*1024U || length%512 || lseek(fd,0,SEEK_SET)!=0) {
        close(fd);return -1;
    }
    void *p=mmap(NULL,(size_t)length,PROT_READ|PROT_WRITE,MAP_ANONYMOUS|MAP_PRIVATE,-1,0);
    if(p==MAP_FAILED) {close(fd);return -1;}
    size_t position=0;int ok=1;
    while(position<(size_t)length) {
        ssize_t count=read(fd,(unsigned char *)p+position,(size_t)length-position);
        if(count<=0 || (size_t)count>(size_t)length-position) {ok=0;break;}
        position+=(size_t)count;
    }
    unsigned char extra;
    if(ok && read(fd,&extra,1)!=0)ok=0;
    if(close(fd)!=0)ok=0;
    if(!ok || pwl_files_validate(p,(size_t)length)!=PWL_OK) {munmap(p,(size_t)length);return -1;}
    *buffer=p;*bytes=(size_t)length;return 0;
}
int _main(struct thread *td)
{
    (void)td;initKernel();initLibc();
    printf_notification("PS4WL Native Prepare: %s build %s",mode,PS4WL_BUILD_ID);
    if(get_firmware()!=1352)return 1;
    uint64_t base=get_kernel_base();
    if(base<UINT64_C(0xffff800000000000) || base%16384 || base>UINT64_MAX-0x22d1d58) {
        printf_notification("PS4WL Native Prepare: reader base unavailable");return 1;
    }
    void *archive=NULL;size_t bytes=0;
    if(read_archive(&archive,&bytes)) {
        printf_notification("PS4WL Native Prepare: valid PWL_BOOT.PAK not found");return 1;
    }
    void *bounce=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_ANONYMOUS|MAP_PRIVATE,-1,0);
    if(bounce==MAP_FAILED) {munmap(archive,bytes);return 1;}
    uintptr_t start,end;
    __asm__ volatile("lea %0,_start[rip]; lea %1,__pwl_image_end[rip]":"=r"(start),"=r"(end));
    size_t code_bytes=end>start?(size_t)(end-start):0;
    int code_locked=0,archive_locked=0,bounce_locked=0;
    if(code_bytes && mlock(_start,code_bytes)==0)code_locked=1;
    if(code_locked && mlock(archive,bytes)==0)archive_locked=1;
    if(archive_locked && mlock(bounce,4096)==0)bounce_locked=1;
    int rc=1;
    if(bounce_locked) {
        reader=(pwl_ps4_reader_t){base,bounce,4096};
        binding.read=pwl_ps4_protected_read;binding.read_context=&reader;
        binding.kernel_base=binding.thread=binding.root=0;
        image=resident_fixture();
        request=(pwl_native_request_t){image.bytes,image.size,archive,bytes,
            2U*1024U*1024U,65536,128,1,NULL,0};
        result.binding=result.prepare=result.audit=result.release=PWL_ERR_UNSUPPORTED;
        int transport=kexec(prepare_callback,NULL);
        printf_notification("PS4WL Native Prepare: rc=%d stage=%u bind=%d prep=%d audit=%d release=%d",
            transport,result.stage,result.binding,result.prepare,result.audit,result.release);
        printf_notification("PS4WL Native Prepare: PA=%llx bytes=%llu entry=%llx",
            (unsigned long long)result.physical,(unsigned long long)result.bytes,(unsigned long long)result.entry);
        rc=transport || result.stage!=5;
    } else printf_notification("PS4WL Native Prepare: resident buffers unavailable");
    if(result.retained) {
        /* Do not unpin callbacks/data while a kernel allocation still refers
         * to their binding. Keep this preparation process alive on refusal. */
        printf_notification("PS4WL Native Prepare: owner retained; preparation stopped");
        for(;;)sceKernelUsleep(1000000);
    }
    if(bounce_locked)munlock(bounce,4096);
    if(archive_locked)munlock(archive,bytes);
    if(code_locked)munlock(_start,code_bytes);
    munmap(bounce,4096);munmap(archive,bytes);return rc;
}

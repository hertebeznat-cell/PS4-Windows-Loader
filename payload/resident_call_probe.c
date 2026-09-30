#include "ps4.h"
#include "syscall.h"
#include "pwl_resident_selftest.h"
#include "resident_fixture.h"
int probe_fsync(int fd);
SYSCALL(probe_fsync,95);
#include "probe_log.h"
#ifndef PS4WL_BUILD_ID
#define PS4WL_BUILD_ID "local"
#endif
static char report_path[96];
static pwl_resident_data_t data;
static pwl_resident_call_report_t report;
static int append(const char *text) {
    int error=0,status=pwl_probe_log_append(report_path,text,strlen(text),&error);
    if(status)printf_notification("PS4WL Calls: USB stage=%d errno=%x",status,error);
    return status;
}
static int checkpoint(unsigned call,void *unused) {
    UNUSED(unused);
    char text[96];
    int n=snprintf(text,sizeof(text),"checkpoint=BEFORE_CALL call=%u mode=PROCESS_CALLBACK_TEST\n",call);
    if(n<=0 || (size_t)n>=sizeof(text))return 1;
    return append(text);
}
int _main(struct thread *unused) {
    UNUSED(unused);initKernel();initLibc();
    printf_notification("PS4WL Calls: entered %s",PS4WL_BUILD_ID);
    int statuses[2],errors[2];
    int port=pwl_probe_log_select("PS4WL_CALLS.LOG",PS4WL_BUILD_ID,report_path,
        sizeof(report_path),statuses,errors);
    if(port<0) {
        for(unsigned i=0;i<2;i++)printf_notification("PS4WL Calls: USB%u stage=%d errno=%x",i,statuses[i],errors[i]);
        printf_notification("PS4WL Calls: no writable log; test NOT started");return 1;
    }
    printf_notification("PS4WL Calls: log ready USB%d",port);
    pwl_resident_image_t image=resident_fixture();
    if(pwl_resident_image_validate(&image)!=PWL_OK) {
        append("checkpoint=STOP reason=image_integrity\n");return 1;
    }
    size_t bytes=(image.size+16383U)&~(size_t)16383U;
    unsigned char *code=mmap(NULL,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(code==(void *)-1) {
        int error=errno;
        append("checkpoint=STOP reason=code_mapping\n");
        printf_notification("PS4WL Calls: mapping failed errno=%x",error);return 1;
    }
    for(size_t i=0;i<image.size;i++)code[i]=((const unsigned char *)image.bytes)[i];
    uint64_t binding=(uint64_t)(uintptr_t)&data;
    for(unsigned i=0;i<8;i++)code[image.binding_offset+i]=(unsigned char)(binding>>(8*i));
    if(mprotect(code,bytes,PROT_READ|PROT_EXEC)!=0) {
        int error=errno;
        append("checkpoint=STOP reason=executable_mapping\n");
        printf_notification("PS4WL Calls: RX mapping refused errno=%x; stopped",error);
        munmap(code,bytes);return 1;
    }
    char text[256];
    int n=snprintf(text,sizeof(text),"checkpoint=CODE_READY code=%llx data=%llx bytes=%u mode=PROCESS_CALLBACK_TEST synthetic_map=1 cpu_switch=0\n",
        (unsigned long long)(uintptr_t)code,(unsigned long long)binding,(unsigned)image.size);
    if(n<=0 || (size_t)n>=sizeof(text) || append(text)) {munmap(code,bytes);return 1;}
    pwl_status_t status=pwl_resident_calls_test(&image,code,&data,&report,checkpoint,NULL);
    int release=munmap(code,bytes);
    n=snprintf(text,sizeof(text),"build=%s result=%d passed_mask=%x last_call=%u exit_status=%llx release=%d mode=PROCESS_CALLBACK_TEST cpu_switch=0\n",
        PS4WL_BUILD_ID,status,report.passed_mask,report.last_call,
        (unsigned long long)report.exit_status,release);
    int log_status=n<=0 || (size_t)n>=sizeof(text)?1:append(text);
    printf_notification("PS4WL Calls: result=%d passed=%x last=%u release=%d",status,report.passed_mask,report.last_call,release);
    if(!log_status)printf_notification("PS4WL Calls: report saved %s",report_path);
    return status!=PWL_OK || release || log_status;
}

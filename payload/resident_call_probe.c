#include "ps4.h"
#include "syscall.h"
#include "pwl_resident_selftest.h"
#include "resident_fixture.h"
#ifdef PWL_STACK_PROBE
#include "pwl_stack_call.h"
#define CALL_LABEL "PS4WL Stack"
#define CALL_LOG "PS4WL_STACK.LOG"
#define CALL_MODE "PROCESS_STACK_TEST"
#else
#define CALL_LABEL "PS4WL Calls"
#define CALL_LOG "PS4WL_CALLS.LOG"
#define CALL_MODE "PROCESS_CALLBACK_TEST"
#endif
int probe_fsync(int fd);
SYSCALL(probe_fsync,95);
#include "probe_log.h"
#ifndef PS4WL_BUILD_ID
#define PS4WL_BUILD_ID "local"
#endif
static char report_path[96];
static pwl_resident_data_t data;
static pwl_resident_call_report_t report;
#ifdef PWL_STACK_PROBE
static pwl_stack_report_t stack_report;
static struct {
    pwl_resident_image_t image;
    void *code;
    uintptr_t low,high,observed;
} stack_context;
static int run_on_stack(void *context) {
    UNUSED(context);
    volatile uint64_t marker=0;
    stack_context.observed=(uintptr_t)&marker;
    if(stack_context.observed<stack_context.low || stack_context.observed>=stack_context.high)
        return PWL_ERR_INVALID_ARGUMENT;
    /* No SDK/logging calls on the new stack. Before/after checkpoints are
     * persisted by the caller on the original process stack.
     */
    return pwl_resident_calls_test(&stack_context.image,stack_context.code,&data,&report,NULL,NULL);
}
#endif
static int append(const char *text) {
    int error=0,status=pwl_probe_log_append(report_path,text,strlen(text),&error);
    if(status)printf_notification(CALL_LABEL ": USB stage=%d errno=%x",status,error);
    return status;
}
#ifndef PWL_STACK_PROBE
static int checkpoint(unsigned call,void *unused) {
    UNUSED(unused);
    char text[96];
    int n=snprintf(text,sizeof(text),"checkpoint=BEFORE_CALL call=%u mode=PROCESS_CALLBACK_TEST\n",call);
    if(n<=0 || (size_t)n>=sizeof(text))return 1;
    return append(text);
}
#endif
int _main(struct thread *unused) {
    UNUSED(unused);initKernel();initLibc();
    printf_notification(CALL_LABEL ": entered %s",PS4WL_BUILD_ID);
    int statuses[2],errors[2];
    int port=pwl_probe_log_select(CALL_LOG,PS4WL_BUILD_ID,report_path,
        sizeof(report_path),statuses,errors);
    if(port<0) {
        for(unsigned i=0;i<2;i++)printf_notification(CALL_LABEL ": USB%u stage=%d errno=%x",i,statuses[i],errors[i]);
        printf_notification(CALL_LABEL ": no writable log; test NOT started");return 1;
    }
    printf_notification(CALL_LABEL ": log ready USB%d",port);
    pwl_resident_image_t image=resident_fixture();
    if(pwl_resident_image_validate(&image)!=PWL_OK) {
        append("checkpoint=STOP reason=image_integrity\n");return 1;
    }
    size_t bytes=(image.size+16383U)&~(size_t)16383U;
    unsigned char *code=mmap(NULL,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(code==(void *)-1) {
        int error=errno;
        append("checkpoint=STOP reason=code_mapping\n");
        printf_notification(CALL_LABEL ": mapping failed errno=%x",error);return 1;
    }
    for(size_t i=0;i<image.size;i++)code[i]=((const unsigned char *)image.bytes)[i];
    uint64_t binding=(uint64_t)(uintptr_t)&data;
    for(unsigned i=0;i<8;i++)code[image.binding_offset+i]=(unsigned char)(binding>>(8*i));
    if(mprotect(code,bytes,PROT_READ|PROT_EXEC)!=0) {
        int error=errno;
        append("checkpoint=STOP reason=executable_mapping\n");
        printf_notification(CALL_LABEL ": RX mapping refused errno=%x; stopped",error);
        munmap(code,bytes);return 1;
    }
    char text[256];
    int n=snprintf(text,sizeof(text),"checkpoint=CODE_READY code=%llx data=%llx bytes=%u mode=" CALL_MODE " synthetic_map=1 cpu_switch=0\n",
        (unsigned long long)(uintptr_t)code,(unsigned long long)binding,(unsigned)image.size);
    if(n<=0 || (size_t)n>=sizeof(text) || append(text)) {munmap(code,bytes);return 1;}
#ifdef PWL_STACK_PROBE
    size_t guard=16384,stack_bytes=1024*1024,owner_bytes=stack_bytes+2*guard;
    unsigned char *owner=mmap(NULL,owner_bytes,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(owner==(void *)-1) {
        append("checkpoint=STOP reason=stack_mapping\n");munmap(code,bytes);return 1;
    }
    if(mprotect(owner+guard,stack_bytes,PROT_READ|PROT_WRITE)!=0) {
        append("checkpoint=STOP reason=stack_permissions\n");
        munmap(owner,owner_bytes);munmap(code,bytes);return 1;
    }
    stack_context.image=image;stack_context.code=code;
    stack_context.low=(uintptr_t)(owner+guard);stack_context.high=stack_context.low+stack_bytes;
    n=snprintf(text,sizeof(text),"checkpoint=BEFORE_STACK low=%llx high=%llx guards=16384 mode=PROCESS_STACK_TEST cpu_switch=0\n",
        (unsigned long long)stack_context.low,(unsigned long long)stack_context.high);
    if(n<=0 || (size_t)n>=sizeof(text) || append(text)) {
        munmap(owner,owner_bytes);munmap(code,bytes);return 1;
    }
    pwl_status_t status=pwl_stack_call((void *)stack_context.high,run_on_stack,NULL,&stack_report);
    int restored=stack_report.before==stack_report.after &&
        stack_report.entered==stack_context.high && stack_context.observed>=stack_context.low &&
        stack_context.observed<stack_context.high;
    int stack_release=munmap(owner,owner_bytes);
    n=snprintf(text,sizeof(text),"checkpoint=AFTER_STACK restored=%d before=%llx entered=%llx after=%llx observed=%llx stack_release=%d\n",
        restored,(unsigned long long)stack_report.before,(unsigned long long)stack_report.entered,
        (unsigned long long)stack_report.after,(unsigned long long)stack_context.observed,stack_release);
    int stack_log=n<=0 || (size_t)n>=sizeof(text)?1:append(text);
    if(!restored || stack_release || stack_log)status=PWL_ERR_INVALID_ARGUMENT;
#else
    pwl_status_t status=pwl_resident_calls_test(&image,code,&data,&report,checkpoint,NULL);
#endif
    int release=munmap(code,bytes);
    n=snprintf(text,sizeof(text),"build=%s result=%d passed_mask=%x last_call=%u exit_status=%llx release=%d mode=" CALL_MODE " cpu_switch=0\n",
        PS4WL_BUILD_ID,status,report.passed_mask,report.last_call,
        (unsigned long long)report.exit_status,release);
    int log_status=n<=0 || (size_t)n>=sizeof(text)?1:append(text);
    printf_notification(CALL_LABEL ": result=%d passed=%x last=%u release=%d",status,report.passed_mask,report.last_call,release);
    if(!log_status)printf_notification(CALL_LABEL ": report saved %s",report_path);
    return status!=PWL_OK || release || log_status;
}

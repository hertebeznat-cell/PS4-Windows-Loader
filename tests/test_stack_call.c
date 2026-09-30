#define _GNU_SOURCE
#include "pwl_stack_call.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static uintptr_t low,high,observed;
static unsigned calls;
static int callback(void *context) {
    volatile uint64_t marker=0x12345678;
    observed=(uintptr_t)&marker;
    assert(context==(void *)0x1234 && observed>=low && observed<high);
    calls++;
    return 73;
}
static void guard_fault(unsigned char *address) {
    pid_t child=fork();assert(child>=0);
    if(!child){signal(SIGSEGV,SIG_DFL);*address=1;_exit(0);}
    int status=0;assert(waitpid(child,&status,0)==child);
    assert(WIFSIGNALED(status) && WTERMSIG(status)==SIGSEGV);
}
int main(void) {
    size_t page=(size_t)sysconf(_SC_PAGESIZE),size=1024*1024;
    unsigned char *owner=mmap(NULL,size+2*page,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(owner!=MAP_FAILED);
    assert(mprotect(owner+page,size,PROT_READ|PROT_WRITE)==0);
    low=(uintptr_t)(owner+page);high=low+size;
    for(unsigned i=0;i<3;i++) {
        pwl_stack_report_t report={0};
        assert(pwl_stack_call((void *)(high-i),callback,(void *)0x1234,&report)==73);
        assert(report.before==report.after && report.before!=report.entered);
        assert(report.entered==((high-i) & ~(uintptr_t)15));
        assert(observed>=low && observed<report.entered);
    }
    assert(calls==3);
    guard_fault(owner);guard_fault(owner+page+size);
    assert(munmap(owner,size+2*page)==0);
    puts("separate stack: aligned entry, callback result, repeated return and both guard faults passed");
}

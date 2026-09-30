#include "ps4.h"
#include "memory_probe_signatures.h"
#include "syscall.h"
int probe_fsync(int fd);
SYSCALL(probe_fsync,95);
#ifndef PS4WL_BUILD_ID
#define PS4WL_BUILD_ID "local"
#endif
#ifdef PWL_WORKSPACE_PROBE
#include "workspace_report.h"
static volatile pwl_workspace_report_t workspace_result;
#ifdef PWL_RESIDENT_PROBE
#define PWL_PROBE_LABEL "PS4WL Resident"
#else
#define PWL_PROBE_LABEL "PS4WL Workspace"
#endif
#else
#define PWL_PROBE_LABEL "PS4WL Memory"
#endif
extern unsigned char _start[], __pwl_image_end[];
typedef unsigned long long vm_u64;
typedef vm_u64 (*alloc_fn)(void *,vm_u64,int,vm_u64,vm_u64,unsigned long,unsigned long,char);
typedef void (*free_fn)(void *,vm_u64,vm_u64);
typedef vm_u64 (*extract_fn)(void *,vm_u64);
static volatile struct {uint64_t base,kva,pa,flags;uint32_t stage,error,critical;uint16_t locks,cs;} result;
static int match(uint64_t address,const unsigned char *expected,size_t length) {
  const volatile unsigned char *p=(const volatile unsigned char*)address;
  for(size_t i=0;i<length;i++)if(p[i]!=expected[i])return 0;
  return 1;
}
static int canonical(uint64_t p) {return p>=0xffff800000000000ULL;}
/* Experimental diagnostic only; never approves the production memory API. */
static int probe(struct thread *td,void *args) {
  (void)args;
  uint32_t lo,hi;uint64_t current,flags;uint16_t cs;
  result.stage=1;
  __asm__ volatile("pushfq; pop %0":"=r"(flags));
  __asm__ volatile("mov %0, cs":"=r"(cs));
  result.flags=flags;result.cs=cs;
  if((cs&3) || !(flags&0x200) || (flags&0x400)) {result.error=1;return 0;}
  __asm__ volatile("mov %0, qword ptr gs:0":"=r"(current));
  if(!canonical(current) || (uint64_t)td!=current) {result.error=2;return 0;}
  __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0xc0000082));
  uint64_t base=(((uint64_t)hi<<32)|lo)-0x1c0;result.base=base;
  if(!canonical(base) || (base&0x3fff)) {result.error=3;return 0;}
  static const unsigned char version[]="r228995/release_branches/release_13.520 Jun 11 2026 05:25:24";
  if(!match(base+0x1520080,version,sizeof(version))) {result.error=4;return 0;}
  if(!match(base+0x24d4f0,sig_alloc,sizeof(sig_alloc)) ||
     !match(base+0x466460,sig_free,sizeof(sig_free)) ||
     !match(base+0x573d0,sig_extract,sizeof(sig_extract)) ||
     !match(base+0x378a80,sig_mutex,sizeof(sig_mutex)) ||
     !match(base+0x4d6d0,sig_handler,sizeof(sig_handler))) {result.error=4;return 0;}
  result.critical=*(volatile uint32_t*)(current+0x128);
  result.locks=*(volatile uint16_t*)(current+0xfc);
  if(result.critical || result.locks) {result.error=5;return 0;}
  void *map=*(void*volatile*)(base+0x22d1d50),*pmap=(void*)(base+0x1b2c3a0);
  if(!canonical((uint64_t)map) || ((uint64_t)map&7) ||
     !canonical(*(volatile uint64_t*)((uint64_t)pmap+0x20))) {result.error=6;return 0;}
  alloc_fn allocate=(alloc_fn)(base+0x24d4f0);
  free_fn release=(free_fn)(base+0x466460);
  extract_fn extract=(extract_fn)(base+0x573d0);
#ifdef PWL_WORKSPACE_PROBE
  result.stage=2;
  int workspace_rc=pwl_workspace_experiment(map,pmap,allocate,release,extract,&workspace_result);
  result.kva=workspace_result.kva;result.pa=workspace_result.pa;
  result.stage=workspace_result.stage;result.error=workspace_rc?13:0;
  return 0;
#else
  result.stage=2;
  vm_u64 kva=allocate(map,0x4000,0x101,0x100000,1ULL<<47,0x4000,0,6);
  result.kva=kva;
  if(!kva) {result.error=7;return 0;}
  result.stage=3;
  /* Preserve the original KVA for release, even if translation is rejected. */
  if(!canonical(kva) || (kva&0x3fff) || kva>UINT64_MAX-0x4000) {
    result.error=8;return 0; /* unexpected owner: retain, never free a guessed range */
  }
  else {
    vm_u64 pa=extract(pmap,kva);result.pa=pa;
    if(pa<0x100000 || (pa&0x3fff) || pa>(1ULL<<47)-0x4000)result.error=9;
    else for(vm_u64 off=0;off<0x4000;off+=0x1000)
      if(extract(pmap,kva+off)!=pa+off || extract(pmap,kva+off+0xfff)!=pa+off+0xfff)result.error=10;
  }
  if(!result.error) {
    volatile unsigned char *memory=(volatile unsigned char*)kva;
    for(size_t i=0;i<0x4000;i++)if(memory[i]) {result.error=11;break;}
    if(!result.error) {
      memory[0]=0xa5;memory[0x3fff]=0x5a;
      if(memory[0]!=0xa5 || memory[0x3fff]!=0x5a)result.error=12;
    }
  }
  result.stage=4;
  release(map,kva,0x4000);
  result.stage=5; /* void free returned; not independent proof of reclamation */
  return 0;
#endif
}
int _main(struct thread *unused) {
  UNUSED(unused);initKernel();initLibc();
  printf_notification(PWL_PROBE_LABEL ": entered %s",PS4WL_BUILD_ID);
  if(get_firmware()!=1352 || !is_jailbroken()) {printf_notification(PWL_PROBE_LABEL ": requires 13.52/HEN");return 1;}
  size_t bytes=(size_t)(__pwl_image_end-_start);
  if(!bytes || mlock(_start,bytes)!=0) {printf_notification(PWL_PROBE_LABEL ": payload lock failed; stopped");return 1;}
  printf_notification(PWL_PROBE_LABEL ": experiment starting");
  int rc=kexec(probe,NULL);
  int unlock_rc=munlock(_start,bytes);
  printf_notification(PWL_PROBE_LABEL ": returned rc=%d stage=%u error=%u",rc,result.stage,result.error);
  printf_notification(PWL_PROBE_LABEL ": critical=%u locks=%u flags=%llx",result.critical,result.locks,(unsigned long long)result.flags);
  printf_notification(PWL_PROBE_LABEL ": KVA=%llx PA=%llx",(unsigned long long)result.kva,(unsigned long long)result.pa);
#ifdef PWL_WORKSPACE_PROBE
  printf_notification(PWL_PROBE_LABEL ": prep=%d tables=%d release=%d",workspace_result.prepare_status,workspace_result.table_status,workspace_result.release_status);
  printf_notification(PWL_PROBE_LABEL ": bytes=%llu tables=%u copies=%u",workspace_result.bytes,workspace_result.tables,workspace_result.copy_ok);
#ifdef PWL_RESIDENT_PROBE
  printf_notification(PWL_PROBE_LABEL ": EFI=%d; code not called",workspace_result.efi_status);
#endif
#endif
  char log[1024];
  int n=snprintf(log,sizeof(log),"build=%s rc=%d stage=%u error=%u critical=%u locks=%u cs=%x flags=%llx base=%llx kva=%llx pa=%llx unlock_rc=%d\n",
    PS4WL_BUILD_ID,rc,result.stage,result.error,result.critical,result.locks,result.cs,
    (unsigned long long)result.flags,(unsigned long long)result.base,
    (unsigned long long)result.kva,(unsigned long long)result.pa,unlock_rc);
#ifdef PWL_WORKSPACE_PROBE
  if(n>0 && (size_t)n<sizeof(log)) {
    int extra=snprintf(log+n,sizeof(log)-(size_t)n,"workspace prep=%d tables_status=%d release=%d bytes=%llu root=%llx tables=%u regions=%u copies=%u\n",
      workspace_result.prepare_status,workspace_result.table_status,workspace_result.release_status,workspace_result.bytes,
      workspace_result.root,workspace_result.tables,workspace_result.regions,workspace_result.copy_ok);
    if(extra<0 || (size_t)extra>=sizeof(log)-(size_t)n)n=-1;else n+=extra;
  }
#ifdef PWL_RESIDENT_PROBE
  if(n>0 && (size_t)n<sizeof(log)) {
    int extra=snprintf(log+n,sizeof(log)-(size_t)n,"resident efi_status=%d mode=PREPARATION_ONLY code_called=0\n",workspace_result.efi_status);
    if(extra<0 || (size_t)extra>=sizeof(log)-(size_t)n)n=-1;else n+=extra;
  }
  const char *log_path="/mnt/usb0/PS4WL_RESIDENT.LOG";
#else
  const char *log_path="/mnt/usb0/PS4WL_WORKSPACE.LOG";
#endif
#else
  const char *log_path="/mnt/usb0/PS4WL_MEMORY.LOG";
#endif
  int fd=open(log_path,O_WRONLY|O_CREAT|O_APPEND,0600);
  if(fd>=0) {
    int good=n>0 && (size_t)n<sizeof(log);size_t done=0;
    while(good && done<(size_t)n) {ssize_t w=write(fd,log+done,(size_t)n-done);if(w<=0 || (size_t)w>(size_t)n-done)good=0;else done+=(size_t)w;}
    if(probe_fsync(fd)!=0)good=0;
    if(close(fd)!=0)good=0;
    printf_notification(good?PWL_PROBE_LABEL ": report on USB0":PWL_PROBE_LABEL ": USB report incomplete");
  } else printf_notification(PWL_PROBE_LABEL ": no USB log; photograph notifications");
  if(unlock_rc)printf_notification(PWL_PROBE_LABEL ": payload unlock failed");
  return rc || unlock_rc || result.error || result.stage!=5;
}

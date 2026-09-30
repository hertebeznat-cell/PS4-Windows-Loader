#include "ps4.h"
#include "memory_probe_signatures.h"
#include "syscall.h"
int probe_fsync(int fd);
SYSCALL(probe_fsync,95);
#include "probe_log.h"
#ifdef PWL_ROOT_EFI_PROBE
#include "verified_usb_log.h"
#endif
#ifndef PS4WL_BUILD_ID
#define PS4WL_BUILD_ID "local"
#endif
#ifdef PWL_ROOT_CLONE_PROBE
#include "root_clone_report.h"
#ifndef PWL_ROOT_EFI_PROBE
#include "raw_journal.h"
#endif
static volatile pwl_root_clone_report_t root_result;
#ifdef PWL_ROOT_EFI_PROBE
#include "pwl_resident_selftest.h"
#include "resident_fixture.h"

#define PWL_PROBE_LABEL "PS4WL Root EFI"
#define PWL_RETURN_MODE "IDENTICAL_ROOT_EFI"
static pwl_resident_data_t root_efi_data;
static pwl_resident_call_report_t root_efi_report;
static pwl_resident_image_t root_efi_image;
static void *root_efi_code;
static size_t root_efi_bytes;
static uint64_t root_efi_observed;
int pwl_root_efi_callback(void *context) {
  volatile uint64_t marker=0;
  const volatile pwl_root_clone_report_t *r=context;
  root_efi_observed=(uint64_t)(uintptr_t)&marker;
  if(root_efi_observed<r->kva+16384 || root_efi_observed>=r->kva+32768)
    return PWL_ERR_INVALID_ARGUMENT;
  return pwl_resident_calls_test(&root_efi_image,root_efi_code,&root_efi_data,
      &root_efi_report,NULL,NULL);
}
static int prepare_root_efi(void) {
  root_efi_image=resident_fixture();
  if(pwl_resident_image_validate(&root_efi_image)!=PWL_OK)return 1;
  root_efi_bytes=(root_efi_image.size+16383U)&~(size_t)16383U;
  root_efi_code=mmap(NULL,root_efi_bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  if(root_efi_code==(void *)-1){root_efi_code=NULL;return 1;}
  unsigned char *code=root_efi_code;
  for(size_t i=0;i<root_efi_image.size;i++)code[i]=((const unsigned char *)root_efi_image.bytes)[i];
  uint64_t binding=(uint64_t)(uintptr_t)&root_efi_data;
  for(unsigned i=0;i<8;i++)code[root_efi_image.binding_offset+i]=(unsigned char)(binding>>(8*i));
  if(mprotect(code,root_efi_bytes,PROT_READ|PROT_EXEC)!=0 || mlock(code,root_efi_bytes)!=0) {
    munmap(code,root_efi_bytes);root_efi_code=NULL;return 1;
  }
  return 0;
}
#else
#define PWL_PROBE_LABEL "PS4WL Root Clone"
#define PWL_RETURN_MODE "IDENTICAL_ROOT_CLONE"
#endif
#elif defined(PWL_WORKSPACE_PROBE)
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
static char report_path[96];
#ifdef PWL_ROOT_CLONE_PROBE
/* Root diagnostics remain usable when removable storage is unavailable. */
static int report_optional_status(int status) { (void)status; return 0; }
#else
static int report_optional_status(int status) { return status; }
#endif
static int report_append(const char *text) {
  int error=0;
#ifdef PWL_ROOT_EFI_PROBE
  int status=pwl_verified_usb_append(report_path,text,strlen(text),&error);
#else
  int status=pwl_probe_log_append(report_path,text,strlen(text),&error);
#endif
  if(status)printf_notification(PWL_PROBE_LABEL ": USB stage=%d errno=%x",status,error);
  return report_optional_status(status);
}
static int report_begin(void) {
#ifdef PWL_ROOT_CLONE_PROBE
  const char *name="PS4WL_TRANSITION.LOG";
#elif defined(PWL_RESIDENT_PROBE)
  const char *name="PS4WL_RESIDENT.LOG";
#elif defined(PWL_WORKSPACE_PROBE)
  const char *name="PS4WL_WORKSPACE.LOG";
#else
  const char *name="PS4WL_MEMORY.LOG";
#endif
  int statuses[2],errors[2];
#ifdef PWL_ROOT_EFI_PROBE
  UNUSED(name);
  int port=pwl_verified_usb_begin(PS4WL_BUILD_ID,report_path,sizeof(report_path),statuses,errors);
#else
  int port=pwl_probe_log_select(name,PS4WL_BUILD_ID,report_path,sizeof(report_path),statuses,errors);
#endif
  if(port>=0){printf_notification(PWL_PROBE_LABEL ": log ready USB%d",port);return 0;}
  for(unsigned i=0;i<2;i++)
    printf_notification(PWL_PROBE_LABEL ": USB%u stage=%d errno=%x",i,statuses[i],errors[i]);
  #ifdef PWL_ROOT_CLONE_PROBE
  printf_notification(PWL_PROBE_LABEL ": USB log unavailable; results in notifications");
#else
  printf_notification(PWL_PROBE_LABEL ": no writable USB log; test NOT started");
#endif
  return report_optional_status(1);
}
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
#ifdef PWL_ROOT_CLONE_PROBE
  if(!match(base+0x57410,sig_direct_map,sizeof(sig_direct_map))) {
    result.error=32;return 0;
  }
  uint32_t dm_pml4=*(volatile uint32_t *)(base+0x1b2c394);
  uint32_t dm_pdpt=*(volatile uint32_t *)(base+0x1b2c398);
  int clone_rc=pwl_root_clone_experiment(map,pmap,dm_pml4,dm_pdpt,allocate,release,extract,&root_result);
  result.kva=root_result.kva;result.pa=root_result.pa;
  result.stage=root_result.stage;result.error=clone_rc?root_result.error+20:0;
  return 0;
#elif defined(PWL_WORKSPACE_PROBE)
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
static __attribute__((noinline)) int run_test(void) {
  printf_notification(PWL_PROBE_LABEL ": entered %s",PS4WL_BUILD_ID);
  if(report_begin())return 1;
  if(get_firmware()!=1352 || !is_jailbroken()) {
    report_append("checkpoint=STOP reason=environment_check\n");
    printf_notification(PWL_PROBE_LABEL ": environment check failed; stopped");return 1;
  }
  uintptr_t image_start,image_end;
  __asm__ volatile("lea %0, _start[rip]; lea %1, __pwl_image_end[rip]":"=r"(image_start),"=r"(image_end));
  size_t bytes=(size_t)(image_end-image_start);
  if(!bytes || mlock(_start,bytes)!=0) {
    report_append("checkpoint=STOP reason=memory_lock\n");
    printf_notification(PWL_PROBE_LABEL ": memory lock failed; stopped");return 1;
  }
#ifdef PWL_ROOT_EFI_PROBE
  if(prepare_root_efi()) {
    munlock(_start,bytes);report_append("checkpoint=STOP reason=resident_preparation\n");return 1;
  }
#endif
  if(report_append("checkpoint=STARTING_TEST\n")) {munlock(_start,bytes);return 1;}
  printf_notification(PWL_PROBE_LABEL ": experiment starting");
  int rc=kexec(probe,NULL);
  int unlock_rc=munlock(_start,bytes);
  printf_notification(PWL_PROBE_LABEL ": returned rc=%d stage=%u error=%u",rc,result.stage,result.error);
  printf_notification(PWL_PROBE_LABEL ": critical=%u locks=%u flags=%llx",result.critical,result.locks,(unsigned long long)result.flags);
  printf_notification(PWL_PROBE_LABEL ": KVA=%llx PA=%llx",(unsigned long long)result.kva,(unsigned long long)result.pa);
#ifdef PWL_ROOT_CLONE_PROBE
  printf_notification(PWL_PROBE_LABEL ": active root=%llx source=%llx source PA=%llx",
    (unsigned long long)root_result.cr3,(unsigned long long)root_result.source,
    (unsigned long long)root_result.source_pa);
  printf_notification(PWL_PROBE_LABEL ": kernel root=%llx PA=%llx direct=%llx indices=%u/%u",
    (unsigned long long)root_result.kernel_source,(unsigned long long)root_result.kernel_source_pa,
    (unsigned long long)root_result.direct_base,root_result.direct_pml4,root_result.direct_pdpt);
  printf_notification(PWL_PROBE_LABEL ": CR0=%llx CR4=%llx EFER=%llx",
    (unsigned long long)root_result.cr0,(unsigned long long)root_result.cr4,
    (unsigned long long)root_result.efer);
  printf_notification(PWL_PROBE_LABEL ": status=%d switched=%u restored=%u released=%u",
    root_result.transition_status,root_result.switched,root_result.restored,root_result.released);
  printf_notification(PWL_PROBE_LABEL ": roots=%llx/%llx/%llx stacks=%llx/%llx/%llx",
    (unsigned long long)root_result.root_before,(unsigned long long)root_result.root_entered,
    (unsigned long long)root_result.root_after,(unsigned long long)root_result.stack_before,
    (unsigned long long)root_result.stack_entered,(unsigned long long)root_result.stack_after);
#endif
#ifdef PWL_WORKSPACE_PROBE
  printf_notification(PWL_PROBE_LABEL ": prep=%d tables=%d release=%d",workspace_result.prepare_status,workspace_result.table_status,workspace_result.release_status);
  printf_notification(PWL_PROBE_LABEL ": bytes=%llu tables=%u copies=%u",workspace_result.bytes,workspace_result.tables,workspace_result.copy_ok);
#ifdef PWL_RESIDENT_PROBE
  printf_notification(PWL_PROBE_LABEL ": EFI=%d; code not called",workspace_result.efi_status);
#endif
#endif
#ifdef PWL_ROOT_EFI_PROBE
  int code_unlock=munlock(root_efi_code,root_efi_bytes);
  int code_release=munmap(root_efi_code,root_efi_bytes);
  printf_notification(PWL_PROBE_LABEL ": EFI passed=%x last=%u code_release=%d",root_efi_report.passed_mask,root_efi_report.last_call,code_release);
#endif
  char log[1536];
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
#endif
#endif
#ifdef PWL_ROOT_CLONE_PROBE
  if(n>0 && (size_t)n<sizeof(log)) {
    int extra=snprintf(log+n,sizeof(log)-(size_t)n,
      "transition status=%d error=%u switched=%u restored=%u released=%u cr0=%llx cr3=%llx cr4=%llx efer=%llx source=%llx source_pa=%llx root_before=%llx root_entered=%llx root_after=%llx stack_before=%llx stack_entered=%llx stack_after=%llx kernel_source=%llx kernel_pa=%llx direct=%llx indices=%u/%u mode=" PWL_RETURN_MODE " windows_called=0\n",
      root_result.transition_status,root_result.error,root_result.switched,root_result.restored,root_result.released,
      (unsigned long long)root_result.cr0,(unsigned long long)root_result.cr3,
      (unsigned long long)root_result.cr4,(unsigned long long)root_result.efer,
      (unsigned long long)root_result.source,(unsigned long long)root_result.source_pa,
      (unsigned long long)root_result.root_before,(unsigned long long)root_result.root_entered,
      (unsigned long long)root_result.root_after,(unsigned long long)root_result.stack_before,
      (unsigned long long)root_result.stack_entered,(unsigned long long)root_result.stack_after,
      (unsigned long long)root_result.kernel_source,(unsigned long long)root_result.kernel_source_pa,
      (unsigned long long)root_result.direct_base,root_result.direct_pml4,root_result.direct_pdpt);
    if(extra<0 || (size_t)extra>=sizeof(log)-(size_t)n)n=-1;else n+=extra;
  }
#endif
#ifdef PWL_ROOT_EFI_PROBE
  if(n>0 && (size_t)n<sizeof(log)) {
    int extra=snprintf(log+n,sizeof(log)-(size_t)n,
      "efi passed_mask=%x last_call=%u exit_status=%llx observed=%llx code_unlock=%d code_release=%d mode=IDENTICAL_ROOT_EFI synthetic_map=1 windows_called=0\n",
      root_efi_report.passed_mask,root_efi_report.last_call,(unsigned long long)root_efi_report.exit_status,
      (unsigned long long)root_efi_observed,code_unlock,code_release);
    if(extra<0 || (size_t)extra>=sizeof(log)-(size_t)n)n=-1;else n+=extra;
  }
#endif
  int log_error=0;
  int log_status=n>0 && (size_t)n<sizeof(log)?
    #ifdef PWL_ROOT_EFI_PROBE
    pwl_verified_usb_append(report_path,log,(size_t)n,&log_error):PWL_LOG_FORMAT;
#else
    pwl_probe_log_append(report_path,log,(size_t)n,&log_error):PWL_LOG_FORMAT;
#endif
  if(log_status)printf_notification(PWL_PROBE_LABEL ": result log stage=%d errno=%x",log_status,log_error);
  else printf_notification(PWL_PROBE_LABEL ": report saved %s",report_path);
  if(unlock_rc)printf_notification(PWL_PROBE_LABEL ": payload unlock failed");
#ifdef PWL_ROOT_EFI_PROBE
  if(code_unlock || code_release || root_efi_report.passed_mask!=0x1ff || root_efi_report.last_call!=9)return 1;
#endif
  return rc || unlock_rc || result.error || result.stage!=5 || report_optional_status(log_status);
}

#if defined(PWL_ROOT_CLONE_PROBE) && !defined(PWL_ROOT_EFI_PROBE)
static __attribute__((noinline)) void raw_journal_failure(int s0,long e0,int s1,long e1) {
 printf_notification(PWL_PROBE_LABEL ": early USB0 stage=%d errno=%lld; notifications continue",s0,-e0);
 printf_notification(PWL_PROBE_LABEL ": early USB1 stage=%d errno=%lld; notifications continue",s1,-e1);
}
#endif
int _main(struct thread *unused) {
  UNUSED(unused);
#if defined(PWL_ROOT_CLONE_PROBE) && !defined(PWL_ROOT_EFI_PROBE)
  static const char paths[2][40]={"/mnt/usb0/PS4WL_TRANSITION.LOG","/mnt/usb1/PS4WL_TRANSITION.LOG"};
  static const char entered[]="build=" PS4WL_BUILD_ID " checkpoint=RAW_ENTERED mode=DIRECT_USB_JOURNAL\n";
  static const char kernel_ready[]="checkpoint=KERNEL_LIBRARY_READY\n";
  static const char libc_ready[]="checkpoint=LIBC_READY\n";
  int selected=-1,statuses[2];long errors[2];
  for(unsigned i=0;i<2;i++) {
    statuses[i]=pwl_raw_journal_append(paths[i],entered,sizeof(entered)-1,&errors[i]);
    if(!statuses[i]){selected=(int)i;break;}
  }
  initKernel();
  if(selected>=0) {
    long error;
    (void)pwl_raw_journal_append(paths[selected],kernel_ready,sizeof(kernel_ready)-1,&error);
  }
  initLibc();
  if(selected<0) {
    raw_journal_failure(statuses[0],errors[0],statuses[1],errors[1]);
  }
  if(selected>=0) {
    long error;
    (void)pwl_raw_journal_append(paths[selected],libc_ready,sizeof(libc_ready)-1,&error);
  }
#else
  initKernel();initLibc();
#endif
  return run_test();
}

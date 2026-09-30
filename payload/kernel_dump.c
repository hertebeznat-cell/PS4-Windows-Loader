#include "ps4.h"
#define PWL_DUMP_SDK
#include "dump_checks.h"
#include "syscall.h"
int pwl_fsync(int fd);
SYSCALL(pwl_fsync, 95);
#ifndef PS4WL_BUILD_ID
#define PS4WL_BUILD_ID "local"
#endif
/* Experimental external SDK callback ABI and copyout; no allocator or CPU switch. */
int _main(struct thread *td) {
  UNUSED(td);
  initKernel(); initLibc();
  printf_notification("PS4WL Dump: entered, build %s",PS4WL_BUILD_ID);
  if(get_firmware()!=1352 || !is_jailbroken()) { printf_notification("PS4WL Dump: requires 13.52 and active HEN"); return 1; }
  printf_notification("PS4WL Dump: build %s",PS4WL_BUILD_ID);
  uint64_t base=get_kernel_base();
  if(base==UINT64_MAX || base<0xffff800000000000ULL || (base&0x3fff)) { printf_notification("PS4WL Dump: invalid base; stopped"); return 1; }
  unsigned char *buf=mmap(NULL,0x4000,PROT_READ|PROT_WRITE,MAP_ANONYMOUS|MAP_PRIVATE,-1,0);
  if(buf==MAP_FAILED) {printf_notification("PS4WL Dump: buffer unavailable");return 1;}
  int fd=-1,ok=0; uint64_t total=0,pos=0;
  /* Opening before kernel copying also checks USB write access. Never overwrite a prior file. */
  if(file_exists("/mnt/usb0/PS4WL_KERNEL.bin")) {printf_notification("PS4WL Dump: prior dump exists; stopped");goto done;}
  fd=open("/mnt/usb0/PS4WL_KERNEL.partial",O_WRONLY|O_CREAT|O_EXCL,0600);
  if(fd<0) {printf_notification("PS4WL Dump: USB0 unavailable or partial exists");goto done;}
  memset(buf,0,0x4000);
  if(get_memory_dump(base,(uint64_t*)buf,0x4000)!=0 || pwl_dump_extent(buf,0x4000,base,&total)!=0) {
    printf_notification("PS4WL Dump: header/read rejected; stopped");goto done;
  }
  if(base>UINT64_MAX-total) goto done;
  while(pos<total) {
    size_t count=(total-pos>0x4000)?0x4000:(size_t)(total-pos);
    memset(buf,0,0x4000);
    if(get_memory_dump(base+pos,(uint64_t*)buf,count)!=0) {printf_notification("PS4WL Dump: read failed at %llu",(unsigned long long)pos);goto done;}
    size_t written=0;
    while(written<count) { ssize_t n=write(fd,buf+written,count-written); if(n<=0 || (size_t)n>count-written) {printf_notification("PS4WL Dump: USB write failed");goto done;} written+=(size_t)n; }
    pos+=count;
  }
  if(pwl_fsync(fd)!=0) {printf_notification("PS4WL Dump: sync failed; partial retained");goto done;}
  if(close(fd)!=0) {fd=-1;printf_notification("PS4WL Dump: close failed");goto done;} fd=-1;
  if(rename("/mnt/usb0/PS4WL_KERNEL.partial","/mnt/usb0/PS4WL_KERNEL.bin")!=0) {printf_notification("PS4WL Dump: rename failed; partial retained");goto done;}
  ok=1;printf_notification("PS4WL Dump: complete, %llu bytes; PS4WL_KERNEL.bin",(unsigned long long)total);
done:
  if(fd>=0)close(fd);
  munmap(buf,0x4000);
  return ok?0:1;
}

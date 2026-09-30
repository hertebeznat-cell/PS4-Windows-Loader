#include "ps4.h"
#include "syscall.h"
int probe_fsync(int fd);
SYSCALL(probe_fsync,95);
#include "usb_identity.h"
#include "log_readback.h"
#ifndef PS4WL_BUILD_ID
#define PS4WL_BUILD_ID "local"
#endif
/* USB-only existing-file check. No kernel callback, CPU switch or EFI call. */
static int append_existing(const char *path,const char *text,size_t size,int *error) {
 *error=0;
 int fd=open(path,O_WRONLY|O_APPEND,0); /* Must already exist; never create. */
 if(fd<0){*error=errno;return -1;}
 size_t done=0;int status=0;
 while(done<size) {
  ssize_t n=write(fd,text+done,size-done);
  if(n<0 && errno==4)continue;
  if(n<=0 || (size_t)n>size-done){*error=n<0?errno:0;status=-2;break;}
  done+=(size_t)n;
 }
 if(probe_fsync(fd)!=0 && !status){*error=errno;status=-3;}
 if(close(fd)!=0 && !status){*error=errno;status=-4;}
 if(!status)status=pwl_log_readback(path,text,size,error);
 return status;
}
int _main(struct thread *unused) {
 UNUSED(unused);initKernel();initLibc();
 static const char identity[]="DAVID_USB_E_20260930";
 for(unsigned port=0;port<2;port++) {
  char path[64],text[256];int error=0;
  snprintf(path,sizeof(path),"/mnt/usb%u/PWL_USB.TXT",port);
  int status=pwl_usb_identity(path,identity,sizeof(identity)-1,&error);
  if(status){printf_notification("PS4WL USB existing: USB%u marker=%d errno=%d",port,status,error);continue;}
  int n=snprintf(text,sizeof(text),"\r\nbuild=%s result=USB_EXISTING_FILE_WRITTEN port=%u cpu_switch=0 efi_called=0\r\n",PS4WL_BUILD_ID,port);
  if(n<=0 || (size_t)n>=sizeof(text))return 1;
  status=append_existing(path,text,(size_t)n,&error);
  printf_notification("PS4WL USB existing: status=%d errno=%d path=%s",status,error,path);
  return status!=0;
 }
 printf_notification("PS4WL USB existing: Windows marker not found; no file changed");
 return 1;
}

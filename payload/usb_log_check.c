#include "ps4.h"
#include "syscall.h"
int probe_fsync(int fd);
SYSCALL(probe_fsync,95);
#include "probe_log.h"
#include "log_readback.h"
#include "usb_identity.h"
#ifndef PS4WL_BUILD_ID
#define PS4WL_BUILD_ID "local"
#endif
int _main(struct thread *unused)
{
 UNUSED(unused);initKernel();initLibc();
 printf_notification("PS4WL USB log check: entered %s",PS4WL_BUILD_ID);
 struct stat parent={0};
 if(stat("/mnt",&parent)!=0){printf_notification("PS4WL USB log: /mnt stat errno=%d; stopped",errno);return 1;}
 int passed=0;
 for(unsigned port=0;port<2;port++) {
  char directory[32],path[64],text[256];struct stat usb={0};
  snprintf(directory,sizeof(directory),"/mnt/usb%u",port);
  if(stat(directory,&usb)!=0){printf_notification("PS4WL USB%u: stat errno=%d",port,errno);continue;}
  printf_notification("PS4WL USB%u: dev=%x parent=%x directory=%d",port,usb.st_dev,parent.st_dev,S_ISDIR(usb.st_mode));
  if(!S_ISDIR(usb.st_mode) || usb.st_dev==parent.st_dev) {
   printf_notification("PS4WL USB%u: separate USB filesystem not confirmed; no write",port);continue;
  }
  snprintf(path,sizeof(path),"%s/PWL_USB.TXT",directory);
  static const char identity[]="DAVID_USB_E_20260930";
  int identity_error=0,identity_status=pwl_usb_identity(path,identity,sizeof(identity)-1,&identity_error);
  printf_notification("PS4WL USB%u: identity=%d errno=%d expected=DAVID_USB_E_20260930",port,identity_status,identity_error);
  if(identity_status){printf_notification("PS4WL USB%u: Windows marker NOT matched; no write",port);continue;}
  printf_notification("PS4WL USB%u: Windows marker MATCHED",port);
  snprintf(path,sizeof(path),"%s/PWL.LOG",directory);
  int length=snprintf(text,sizeof(text),"build=%s checkpoint=USB_IDENTITY_CHECK marker=DAVID_USB_E_20260930 port=%u usb_dev=%x parent_dev=%x cpu_switch=0\n",PS4WL_BUILD_ID,port,usb.st_dev,parent.st_dev);
  if(length<=0 || (size_t)length>=sizeof(text))return 1;
  int error=0,status=pwl_probe_log_append(path,text,(size_t)length,&error);
  printf_notification("PS4WL USB%u: write stage=%d errno=%d",port,status,error);
  if(status)continue;
  int directory_fd=open(directory,O_RDONLY,0),flush=-1,flush_error=errno;
  if(directory_fd>=0){flush=probe_fsync(directory_fd);flush_error=flush?errno:0;close(directory_fd);}
  status=pwl_log_readback(path,text,(size_t)length,&error);
  printf_notification("PS4WL USB%u: reopen/read/compare=%d errno=%d dir_sync=%d errno=%d",port,status,error,flush,flush_error);
  if(!status){passed=1;printf_notification("PS4WL USB log: verified %s",path);}
 }
 printf_notification("PS4WL USB log: finished verified=%d; no CPU transition",passed);
 return !passed;
}

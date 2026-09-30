#ifndef PWL_VERIFIED_USB_LOG_H
#define PWL_VERIFIED_USB_LOG_H
#include "usb_identity.h"
#include "log_readback.h"
static char pwl_verified_directory[32];
static int pwl_verified_usb_flush(int *error) {
 int fd=open(pwl_verified_directory,O_RDONLY,0);
 if(fd<0){*error=errno;return PWL_LOG_OPEN;}
 int status=probe_fsync(fd)?PWL_LOG_SYNC:0;
 if(status)*error=errno;
 if(close(fd)!=0 && !status){*error=errno;status=PWL_LOG_CLOSE;}
 return status;
}
static int pwl_verified_usb_append(const char *path,const char *text,size_t size,int *error) {
 if(!path || !*path || !pwl_verified_directory[0]){*error=0;return PWL_LOG_FORMAT;}
 int status=pwl_probe_log_append(path,text,size,error);
 if(!status)status=pwl_verified_usb_flush(error);
 if(!status)status=pwl_log_readback(path,text,size,error);
 return status;
}
static int pwl_verified_usb_begin(const char *build,char *path,size_t capacity,int statuses[2],int errors[2]) {
 path[0]=0;pwl_verified_directory[0]=0;
 statuses[0]=statuses[1]=PWL_LOG_FORMAT;errors[0]=errors[1]=0;
 struct stat parent={0};
 if(stat("/mnt",&parent)!=0){errors[0]=errors[1]=errno;return -1;}
 for(unsigned port=0;port<2;port++) {
  char directory[32],marker[64],temporary[64],final[64],text[256];struct stat usb={0};
  snprintf(directory,sizeof(directory),"/mnt/usb%u",port);
  if(stat(directory,&usb)!=0){errors[port]=errno;continue;}
  if(!S_ISDIR(usb.st_mode) || usb.st_dev==parent.st_dev){statuses[port]=-10;continue;}
  snprintf(marker,sizeof(marker),"%s/PWL_USB.TXT",directory);
  static const char identity[]="DAVID_USB_E_20260930";
  statuses[port]=pwl_usb_identity(marker,identity,sizeof(identity)-1,&errors[port]);
  if(statuses[port])continue;
  snprintf(temporary,sizeof(temporary),"%s/PWL_EFI.TMP",directory);
  snprintf(final,sizeof(final),"%s/PWL_EFI.TXT",directory);
  if(strlen(final)>=capacity)return -1;
  int fd=open(temporary,O_WRONLY|O_CREAT|O_TRUNC,0600);
  if(fd<0){statuses[port]=PWL_LOG_OPEN;errors[port]=errno;continue;}
  if(close(fd)!=0){statuses[port]=PWL_LOG_CLOSE;errors[port]=errno;continue;}
  int n=snprintf(text,sizeof(text),"build=%s checkpoint=ENTERED mode=IDENTICAL_ROOT_EFI marker=%s expected_windows_label=WINDOWS label_verified=0\n",build,identity);
  if(n<=0 || (size_t)n>=sizeof(text))return -1;
  statuses[port]=pwl_probe_log_append(temporary,text,(size_t)n,&errors[port]);
  if(statuses[port])continue;
  if(rename(temporary,final)!=0){statuses[port]=PWL_LOG_WRITE;errors[port]=errno;continue;}
  strcpy(pwl_verified_directory,directory);
  statuses[port]=pwl_verified_usb_flush(&errors[port]);
  if(!statuses[port])statuses[port]=pwl_log_readback(final,text,(size_t)n,&errors[port]);
  if(statuses[port]){pwl_verified_directory[0]=0;continue;}
  strcpy(path,final);return (int)port;
 }
 return -1;
}
#endif

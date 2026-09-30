#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
static const char *content;static size_t size,offset;
static int missing,broken,close_error;static unsigned reads,closes;
static int mock_open(const char *p,int f,int m){assert(!strcmp(p,"PWL_USB.TXT") && f==O_RDONLY && !m);if(missing){errno=ENOENT;return -1;}return 8;}
static ssize_t mock_read(int fd,void *out,size_t n){assert(fd==8);reads++;if(broken){errno=EIO;return -1;}if(reads==1){errno=EINTR;return -1;}if(n>3)n=3;if(n>size-offset)n=size-offset;memcpy(out,content+offset,n);offset+=n;return (ssize_t)n;}
static int mock_close(int fd){assert(fd==8);closes++;errno=EIO;return close_error?-1:0;}
#define open mock_open
#define read mock_read
#define close mock_close
#include "../payload/usb_identity.h"
static void reset(const char *s){content=s;size=strlen(s);offset=reads=closes=0;missing=broken=close_error=0;}
int main(void){
 const char marker[]="DAVID_USB_E_20260930";int error;
 reset(marker);assert(!pwl_usb_identity("PWL_USB.TXT",marker,sizeof(marker)-1,&error) && closes==1);
 reset("DAVID_USB_E_20260930\r\n");assert(!pwl_usb_identity("PWL_USB.TXT",marker,sizeof(marker)-1,&error));
 reset("DAVID_USB_E_20260930\n");assert(!pwl_usb_identity("PWL_USB.TXT",marker,sizeof(marker)-1,&error));
 reset("OTHER_USB");assert(pwl_usb_identity("PWL_USB.TXT",marker,sizeof(marker)-1,&error)==-4);
 reset("DAVID_USB_E_20260930xx");assert(pwl_usb_identity("PWL_USB.TXT",marker,sizeof(marker)-1,&error)==-4);
 reset("");assert(pwl_usb_identity("PWL_USB.TXT",marker,sizeof(marker)-1,&error)==-4);
 reset(marker);missing=1;assert(pwl_usb_identity("PWL_USB.TXT",marker,sizeof(marker)-1,&error)==-1 && error==ENOENT && !closes);
 reset(marker);broken=1;assert(pwl_usb_identity("PWL_USB.TXT",marker,sizeof(marker)-1,&error)==-2 && error==EIO && closes==1);
 reset(marker);close_error=1;assert(pwl_usb_identity("PWL_USB.TXT",marker,sizeof(marker)-1,&error)==-5);
 return 0;
}

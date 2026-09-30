#ifndef PWL_USB_IDENTITY_H
#define PWL_USB_IDENTITY_H
/* Exact marker with optional LF/CRLF; caller supplies POSIX-style I/O. */
static int pwl_usb_identity(const char *path,const char *expected,size_t length,int *error)
{
 *error=0;
 if(!expected || !length || length>62)return -4;
 int fd=open(path,O_RDONLY,0);
 if(fd<0){*error=errno;return -1;}
 char bytes[64];size_t used=0;int status=0;
 for(;;) {
  if(used==sizeof(bytes)){status=-3;break;}
  ssize_t n=read(fd,bytes+used,sizeof(bytes)-used);
  if(n<0 && errno==4)continue;
  if(n<0){*error=errno;status=-2;break;}
  if(!n)break;
  if((size_t)n>sizeof(bytes)-used){status=-3;break;}
  used+=(size_t)n;
 }
 if(!status) {
  if(used<length || used>length+2)status=-4;
  else {
   for(size_t i=0;i<length;i++)if(bytes[i]!=expected[i])status=-4;
   if(used==length+1 && bytes[length]!='\n')status=-4;
   if(used==length+2 && (bytes[length]!='\r' || bytes[length+1]!='\n'))status=-4;
  }
 }
 if(close(fd)!=0 && !status){*error=errno;status=-5;}
 return status;
}
#endif

#ifndef PWL_LOG_READBACK_H
#define PWL_LOG_READBACK_H
/* Caller supplies open/read/lseek/close, errno and off_t. */
static int pwl_log_readback(const char *path,const char *expected,size_t size,int *error)
{
 *error=0;
 int fd=open(path,O_RDONLY,0);
 if(fd<0){*error=errno;return -1;}
 int status=0;
 off_t end=lseek(fd,0,2);
 if(end<0){*error=errno;status=-2;}
 else if((uint64_t)end<size){status=-2;}
 else if(lseek(fd,end-(off_t)size,0)<0){*error=errno;status=-2;}
 else {
  char block[64];size_t done=0;
  while(done<size) {
   size_t want=size-done;if(want>sizeof(block))want=sizeof(block);
   ssize_t n=read(fd,block,want);
   if(n<0 && errno==4)continue;
   if(n<=0 || (size_t)n>want){*error=n<0?errno:0;status=-3;break;}
   for(size_t i=0;i<(size_t)n;i++)if(block[i]!=expected[done+i])status=-4;
   if(status)break;
   done+=(size_t)n;
  }
 }
 if(close(fd)!=0 && !status){*error=errno;status=-5;}
 return status;
}
#endif

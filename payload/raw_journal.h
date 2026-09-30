#ifndef PWL_RAW_JOURNAL_H
#define PWL_RAW_JOURNAL_H
/* Direct syscall functions return negative errno; no SDK/libc state needed. */
long pwl_raw_open(const char *,int,int);
long pwl_raw_write(int,const void *,unsigned long);
long pwl_raw_fsync(int);
long pwl_raw_close(int);
static int pwl_raw_journal_append(const char *path,const char *text,unsigned long size,long *error)
{
 *error=0;
 long fd=pwl_raw_open(path,0x209,0600); /* FreeBSD WRONLY|CREAT|APPEND */
 if(fd<0){*error=fd;return -1;}
 int status=0;
 unsigned long done=0;
 while(done<size) {
  long n=pwl_raw_write((int)fd,text+done,size-done);
  if(n==-4)continue;
  if(n<=0 || (unsigned long)n>size-done){status=-2;*error=n;break;}
  done+=(unsigned long)n;
 }
 long result=pwl_raw_fsync((int)fd);
 if(result<0 && !status){status=-3;*error=result;}
 result=pwl_raw_close((int)fd);
 if(result<0 && !status){status=-4;*error=result;}
 return status;
}
#endif

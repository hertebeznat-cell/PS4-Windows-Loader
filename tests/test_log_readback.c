#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
static const char contents[]="old records\nnew record\n";
static size_t position;static unsigned reads,closes;
static int missing,seek_error,read_error,wrong,close_error;
static int mock_open(const char *path,int flags,int mode) {
 assert(!strcmp(path,"PWL.LOG") && flags==O_RDONLY && !mode);
 if(missing){errno=ENOENT;return -1;}return 7;
}
static off_t mock_seek(int fd,off_t offset,int whence) {
 assert(fd==7);
 if(seek_error==1 || (seek_error==2 && !whence)){errno=EIO;return -1;}
 if(whence==2)return sizeof(contents)-1;
 assert(!whence && offset>=0);position=(size_t)offset;return offset;
}
static ssize_t mock_read(int fd,void *block,size_t n) {
 assert(fd==7);reads++;
 if(read_error){errno=EIO;return -1;}
 if(reads==1){errno=EINTR;return -1;}
 if(n>3)n=3;
 if(position+n>sizeof(contents)-1)n=sizeof(contents)-1-position;
 memcpy(block,contents+position,n);position+=n;
 if(wrong && n)((char *)block)[0]^=1;
 return (ssize_t)n;
}
static int mock_close(int fd){assert(fd==7);closes++;errno=EIO;return close_error?-1:0;}
#define open mock_open
#define lseek mock_seek
#define read mock_read
#define close mock_close
#include "../payload/log_readback.h"
static void reset(void){position=reads=closes=0;missing=seek_error=read_error=wrong=close_error=0;}
int main(void) {
 int error;const char expected[]="new record\n";
 reset();assert(!pwl_log_readback("PWL.LOG",expected,sizeof(expected)-1,&error));assert(closes==1 && reads==5);
 reset();missing=1;assert(pwl_log_readback("PWL.LOG",expected,sizeof(expected)-1,&error)==-1 && !closes);
 reset();seek_error=1;assert(pwl_log_readback("PWL.LOG",expected,sizeof(expected)-1,&error)==-2 && closes==1);
 reset();seek_error=2;assert(pwl_log_readback("PWL.LOG",expected,sizeof(expected)-1,&error)==-2 && error==EIO && closes==1);
 reset();read_error=1;assert(pwl_log_readback("PWL.LOG",expected,sizeof(expected)-1,&error)==-3 && closes==1);
 reset();wrong=1;assert(pwl_log_readback("PWL.LOG",expected,sizeof(expected)-1,&error)==-4 && closes==1);
 reset();close_error=1;assert(pwl_log_readback("PWL.LOG",expected,sizeof(expected)-1,&error)==-5);
 reset();assert(pwl_log_readback("PWL.LOG",expected,100,&error)==-2 && !reads && closes==1);
 return 0;
}

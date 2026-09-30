#include <assert.h>
#include <string.h>
static long open_error,write_error,sync_error,close_error;
static unsigned writes,syncs,closes;
static char output[64];static unsigned long used;
static long mock_open(const char *path,int flags,int mode) {
 assert(!strcmp(path,"/mnt/usb0/test") && flags==0x209 && mode==0600);
 return open_error?open_error:9;
}
static long mock_write(int fd,const void *text,unsigned long size) {
 assert(fd==9);writes++;
 if(write_error) return write_error;
 if(writes==1)return -4;
 if(size>2)size=2;
 memcpy(output+used,text,size);used+=size;return (long)size;
}
static long mock_sync(int fd) {assert(fd==9);syncs++;return sync_error;}
static long mock_close(int fd) {assert(fd==9);closes++;return close_error;}
#define pwl_raw_open mock_open
#define pwl_raw_write mock_write
#define pwl_raw_fsync mock_sync
#define pwl_raw_close mock_close
#include "../payload/raw_journal.h"
static void reset(void) {
 open_error=write_error=sync_error=close_error=0;
 writes=syncs=closes=0;used=0;memset(output,0,sizeof(output));
}
int main(void) {
 long error;
 reset();assert(!pwl_raw_journal_append("/mnt/usb0/test","hello",5,&error));
 assert(!strcmp(output,"hello") && writes==4 && syncs==1 && closes==1);
 reset();open_error=-2;
 assert(pwl_raw_journal_append("/mnt/usb0/test","hello",5,&error)==-1);
 assert(error==-2 && !writes && !closes);
 reset();write_error=-5;sync_error=-22;close_error=-9;
 assert(pwl_raw_journal_append("/mnt/usb0/test","hello",5,&error)==-2);
 assert(error==-5 && syncs==1 && closes==1);
 reset();sync_error=-5;
 assert(pwl_raw_journal_append("/mnt/usb0/test","hello",5,&error)==-3);
 assert(error==-5 && closes==1);
 reset();close_error=-9;
 assert(pwl_raw_journal_append("/mnt/usb0/test","hello",5,&error)==-4);
 assert(error==-9);
 return 0;
}

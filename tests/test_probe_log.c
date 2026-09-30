#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

static int absent0,absent1,write_mode,sync_error,close_error;
static unsigned opens,closes,syncs,writes;
static char output[1024];static size_t used;
static int mock_open(const char *path,int flags,int mode) {
    opens++;assert(flags==(O_WRONLY|O_CREAT|O_APPEND) && mode==0600);
    if((strstr(path,"usb0") && absent0) || (strstr(path,"usb1") && absent1))
        {errno=ENOENT;return -1;}
    return 7;
}
static ssize_t mock_write(int fd,const void *bytes,size_t size) {
    assert(fd==7);writes++;
    if(write_mode==1 && writes==1){errno=EINTR;return -1;}
    if(write_mode==2){errno=EIO;return -1;}
    if(write_mode==3)return 0;
    if(size>3)size=3; /* short writes must be completed */
    assert(used+size<sizeof(output));memcpy(output+used,bytes,size);used+=size;
    return (ssize_t)size;
}
static int mock_sync(int fd){assert(fd==7);syncs++;errno=EIO;return sync_error?-1:0;}
static int mock_close(int fd){assert(fd==7);closes++;errno=EBADF;return close_error?-1:0;}
#define open mock_open
#define write mock_write
#define probe_fsync mock_sync
#define close mock_close
#include "../payload/probe_log.h"
static void reset(void) {
    absent0=absent1=write_mode=sync_error=close_error=0;
    opens=closes=syncs=writes=0;used=0;memset(output,0,sizeof(output));
}
int main(void) {
    char path[96];int errors[2],statuses[2],error;
    reset();absent0=1;
    assert(pwl_probe_log_select("PS4WL_RESIDENT.LOG","test",path,sizeof(path),statuses,errors)==1);
    assert(strcmp(path,"/mnt/usb1/PS4WL_RESIDENT.LOG")==0);
    assert(strstr(output,"checkpoint=ENTERED") && opens==2 && closes==1 && syncs==1);
    assert(statuses[0]==PWL_LOG_OPEN && errors[0]==ENOENT);
    reset();absent0=absent1=1;
    assert(pwl_probe_log_select("PS4WL_RESIDENT.LOG","test",path,sizeof(path),statuses,errors)==-1);
    assert(opens==2 && !writes && !closes);
    reset();write_mode=1;
    assert(pwl_probe_log_append("/mnt/usb0/test","hello",5,&error)==0);
    assert(strcmp(output,"hello")==0 && writes==3 && syncs==1 && closes==1);
    reset();write_mode=2;close_error=1;
    assert(pwl_probe_log_append("test","hello",5,&error)==PWL_LOG_WRITE && error==EIO);
    assert(closes==1 && syncs==1);
    reset();write_mode=3;
    assert(pwl_probe_log_append("test","hello",5,&error)==PWL_LOG_WRITE);
    reset();sync_error=1;
    assert(pwl_probe_log_append("test","hello",5,&error)==PWL_LOG_SYNC && error==EIO);
    assert(closes==1);
    reset();close_error=1;
    assert(pwl_probe_log_append("test","hello",5,&error)==PWL_LOG_CLOSE && error==EBADF);
    reset();
    assert(pwl_probe_log_select("PS4WL_RESIDENT.LOG","test",path,4,statuses,errors)==-1);
    assert(opens==0);
    assert(pwl_probe_log_append("test",NULL,0,&error)==PWL_LOG_FORMAT);
    puts("USB journal: fallback, durable entry, short writes, interruption and failure reporting passed");
}

#ifndef PWL_PROBE_LOG_H
#define PWL_PROBE_LOG_H
/* Caller supplies open/write/close/probe_fsync and errno (SDK or host mocks).
 * Every checkpoint closes and syncs independently, before entering the test.
 */
enum { PWL_LOG_OK=0, PWL_LOG_OPEN=-1, PWL_LOG_WRITE=-2,
       PWL_LOG_SYNC=-3, PWL_LOG_CLOSE=-4, PWL_LOG_FORMAT=-5 };
static int pwl_probe_log_append(const char *path,const char *text,size_t size,int *error)
{
    *error=0;
    if(!text || !size)return PWL_LOG_FORMAT;
    int fd=open(path,O_WRONLY|O_CREAT|O_APPEND,0600);
    if(fd<0){*error=errno;return PWL_LOG_OPEN;}
    int status=PWL_LOG_OK;
    size_t done=0;
    while(done<size) {
        ssize_t count=write(fd,text+done,size-done);
        if(count<0 && errno==4)continue; /* EINTR on FreeBSD and host fixture. */
        if(count<=0 || (size_t)count>size-done) {
            *error=count<0?errno:0;status=PWL_LOG_WRITE;break;
        }
        done+=(size_t)count;
    }
    if(probe_fsync(fd)!=0 && status==PWL_LOG_OK){*error=errno;status=PWL_LOG_SYNC;}
    if(close(fd)!=0 && status==PWL_LOG_OK){*error=errno;status=PWL_LOG_CLOSE;}
    return status;
}
static int pwl_probe_log_select(const char *name,const char *build,char *path,size_t capacity,
                                int statuses[2],int errors[2])
{
    char text[192];
    int n=snprintf(text,sizeof(text),"build=%s checkpoint=ENTERED mode=USB_JOURNAL_V2\n",build);
    statuses[0]=statuses[1]=PWL_LOG_FORMAT;errors[0]=errors[1]=0;
    if(n<=0 || (size_t)n>=sizeof(text))return -1;
    for(unsigned port=0;port<2;port++) {
        int length=snprintf(path,capacity,"/mnt/usb%u/%s",port,name);
        if(length<=0 || (size_t)length>=capacity)return -1;
        statuses[port]=pwl_probe_log_append(path,text,(size_t)n,&errors[port]);
        if(statuses[port]==PWL_LOG_OK)return (int)port;
    }
    return -1;
}
#endif

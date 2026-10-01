#include "ps4.h"
#include "boot_source_io.h"
/* Caller selects the mounted path explicitly. Volume labels are not inferred
 * from usb0/usb1. No mount, write, retry on another device, or CPU entry occurs. */
static pwl_status_t source_open(void *context,const char *path,uint64_t *stream,uint64_t *bytes)
{
    (void)context;
    int fd=open(path,O_RDONLY,0);
    if (fd<0) return errno==2 ? PWL_ERR_NOT_FOUND : PWL_ERR_IO;
    struct stat st;
    if (fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<=0) {
        close(fd);return PWL_ERR_IO;
    }
    *stream=(uint64_t)fd;*bytes=(uint64_t)st.st_size;
    return PWL_OK;
}
static pwl_status_t source_read(void *context,uint64_t stream,void *buffer,size_t bytes,size_t *count)
{
    (void)context;
    ssize_t n;
    do { n=read((int)stream,buffer,bytes); } while (n<0 && errno==4);
    if (n<0) return PWL_ERR_IO;
    *count=(size_t)n;return PWL_OK;
}
static pwl_status_t source_close(void *context,uint64_t stream)
{
    (void)context;return close((int)stream)==0 ? PWL_OK : PWL_ERR_IO;
}
static void *source_allocate(void *context,size_t bytes)
{ (void)context;return malloc(bytes); }
static void source_release(void *context,void *buffer)
{ (void)context;free(buffer); }
const pwl_boot_source_io_t pwl_console_boot_source_io={NULL,source_open,source_read,
    source_close,source_allocate,source_release};

/* Minimal PS4 user-process CPU report. No EFI setup or Microsoft image load. */
#include <sys/types.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <stddef.h>

typedef unsigned int u32;
typedef unsigned long long u64;

#ifndef PS4WL_BUILD_ID
#define PS4WL_BUILD_ID "local-unversioned"
#endif
#ifndef PS4WL_CPU_LOG_PATH
#define PS4WL_CPU_LOG_PATH "/mnt/usb0/PS4WL_CPU.LOG"
#endif

static int probe_fd=-1;

static void write_line(const char *line)
{
    size_t length=0,offset=0;
    while(line[length])++length;
    while(offset<length){
        ssize_t written=write(probe_fd,line+offset,length-offset);
        if(written<=0)break;
        offset+=(size_t)written;
    }
}

static void write_hex(const char *tag,u64 value)
{
    static const char digits[]="0123456789ABCDEF";
    char buffer[96];size_t i=0,j;
    while(*tag&&i+20U<sizeof(buffer))buffer[i++]=*tag++;
    buffer[i++]='0';buffer[i++]='x';
    for(j=0;j<16;j++)buffer[i++]=digits[(value>>(60U-(u32)j*4U))&15U];
    buffer[i++]='\n';buffer[i]=0;
    write_line(buffer);
}

#include "cpuid_log.h"

int main(void)
{
    probe_fd=open(PS4WL_CPU_LOG_PATH,O_WRONLY|O_CREAT|O_TRUNC,0666);
    if(probe_fd<0)return 1;
    write_line("PS4 Windows Loader standalone CPU probe\n");
    write_line("BUILD: " PS4WL_BUILD_ID "\n");
    write_line("MODE: CPU_ONLY; no EFI or Boot Manager entry\n");
    pwl_probe_cpu(write_line,write_hex);
    write_line("CPU probe finished\n");
    if(close(probe_fd)!=0)return 2;
    return 0;
}

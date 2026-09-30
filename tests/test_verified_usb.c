#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static char root[256];
static int writes,rename_fail,directory_fail,corrupt_read;
static void mapped(const char *path,char out[512]) {
 assert(!strncmp(path,"/mnt/usb0",9));
 snprintf(out,512,"%s%s",root,path+4);
}
static int mock_open(const char *path,int flags,int mode) {
 char out[512];mapped(path,out);return open(out,flags,mode);
}
static ssize_t mock_write(int fd,const void *bytes,size_t size) {
 writes++;return write(fd,bytes,size);
}
static ssize_t mock_read(int fd,void *bytes,size_t size) {
 ssize_t n=read(fd,bytes,size);
 if(corrupt_read && n>0)((unsigned char *)bytes)[0]^=1;
 return n;
}
static int mock_stat(const char *path,struct stat *st) {
 memset(st,0,sizeof(*st));st->st_mode=S_IFDIR;
 st->st_dev=!strcmp(path,"/mnt/usb0")?2:1;return 0;
}
static int mock_rename(const char *from,const char *to) {
 char a[512],b[512];mapped(from,a);mapped(to,b);
 if(rename_fail){errno=EIO;return -1;}
 return rename(a,b);
}
static int mock_sync(int fd) {
 struct stat st;assert(!fstat(fd,&st));
 if(directory_fail && S_ISDIR(st.st_mode)){errno=EIO;return -1;}
 return fsync(fd);
}
static void marker(const char *text) {
 char p[512];snprintf(p,sizeof(p),"%s/usb0/PWL_USB.TXT",root);
 int fd=open(p,O_WRONLY|O_CREAT|O_TRUNC,0600);assert(fd>=0);
 assert(write(fd,text,strlen(text))==(ssize_t)strlen(text));assert(!close(fd));
}
#define open mock_open
#define write mock_write
#define read mock_read
#define stat mock_stat
#define rename mock_rename
#define probe_fsync mock_sync
/* Keep the struct tag independent of the stat function-like macro. */
#undef stat
#define stat(path,out) mock_stat(path,out)
#include "../payload/probe_log.h"
#include "../payload/verified_usb_log.h"
int main(void) {
 strcpy(root,"/tmp/pwl-usb-test-XXXXXX");assert(mkdtemp(root));
 char folder[512];snprintf(folder,sizeof(folder),"%s/usb0",root);assert(!mkdir(folder,0700));
 char path[96];int statuses[2],errors[2],error;
 marker("DAVID_USB_E_20260930\r\n");
 assert(pwl_verified_usb_begin("test",path,sizeof(path),statuses,errors)==0);
 assert(!strcmp(path,"/mnt/usb0/PWL_EFI.TXT"));
 assert(!pwl_verified_usb_append(path,"final result=0\n",15,&error));
 writes=0;marker("wrong USB\n");
 assert(pwl_verified_usb_begin("test",path,sizeof(path),statuses,errors)<0);
 assert(!writes && !path[0]);
 marker("DAVID_USB_E_20260930\n");rename_fail=1;
 assert(pwl_verified_usb_begin("test",path,sizeof(path),statuses,errors)<0);
 assert(statuses[0]==PWL_LOG_WRITE && errors[0]==EIO && !path[0]);
 rename_fail=0;directory_fail=1;
 assert(pwl_verified_usb_begin("test",path,sizeof(path),statuses,errors)<0);
 assert(statuses[0]==PWL_LOG_SYNC && errors[0]==EIO && !path[0]);
 directory_fail=0;
 assert(pwl_verified_usb_begin("test",path,sizeof(path),statuses,errors)==0);
 corrupt_read=1;
 assert(pwl_verified_usb_append(path,"final result=0\n",15,&error)==-4);
 corrupt_read=0;
 assert(!pwl_verified_usb_append(path,"final result=0\n",15,&error));
 /* Clean only this test's temporary tree. */
 char p[512];const char *names[]={"PWL_USB.TXT","PWL_EFI.TMP","PWL_EFI.TXT"};
 for(unsigned i=0;i<3;i++){snprintf(p,sizeof(p),"%s/usb0/%s",root,names[i]);unlink(p);}
 assert(!rmdir(folder));assert(!rmdir(root));
 return 0;
}

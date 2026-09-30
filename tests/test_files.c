#include "pwl_files.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char archive[4096];
static void put32(unsigned char *p,uint32_t n) { for (unsigned i=0;i<4;i++) p[i]=(unsigned char)(n>>(8*i)); }
static void put64(unsigned char *p,uint64_t n) { for (unsigned i=0;i<8;i++) p[i]=(unsigned char)(n>>(8*i)); }
static unsigned char *entry(unsigned n) { return archive+24+n*536; }
static void add(unsigned n,const char *path,unsigned directory,uint64_t offset,uint64_t bytes)
{
    unsigned char *r=entry(n);
    for (size_t i=0;path[i];i++) r[i*2]=(unsigned char)path[i];
    put64(r+512,offset);put64(r+520,bytes);put32(r+528,directory);
}
static void fixture(void)
{
    memset(archive,0,sizeof(archive));memcpy(archive,"PWLFILES",8);
    put32(archive+8,1);put32(archive+12,5);put64(archive+16,sizeof(archive));
    add(0,"\\",1,2800,0);add(1,"\\EFI",1,2800,0);
    add(2,"\\EFI\\Microsoft",1,2800,0);
    add(3,"\\EFI\\Microsoft\\Bootmgfw.efi",0,2800,5);
    add(4,"\\BCD",0,2805,3);memcpy(archive+2800,"HELLOBCD",8);
}
int main(void)
{
    fixture();assert(pwl_files_validate(archive,sizeof(archive))==PWL_OK);
    uint16_t path[]={'\\','e','f','i','\\','m','i','c','r','o','s','o','f','t','\\',
                     'b','o','o','t','m','g','f','w','.','e','f','i',0};
    pwl_file_view_t f={0};
    assert(pwl_files_open(archive,sizeof(archive),path,&f)==0 && f.size==5 && !f.directory);
    char output[16]={0};uint64_t position=0;size_t bytes=2;
    assert(pwl_files_read(archive,sizeof(archive),&f,&position,&bytes,output)==0);
    assert(position==2 && bytes==2 && !memcmp(output,"HE",2));
    bytes=16;
    assert(pwl_files_read(archive,sizeof(archive),&f,&position,&bytes,output)==0);
    assert(position==5 && bytes==3 && !memcmp(output,"LLO",3));
    position=UINT64_MAX;bytes=16;
    assert(pwl_files_read(archive,sizeof(archive),&f,&position,&bytes,output)==0 && bytes==0);
    f.size=UINT64_MAX;bytes=1;
    assert(pwl_files_read(archive,sizeof(archive),&f,&position,&bytes,output)==PWL_EFI_INVALID_PARAMETER);
    fixture();put64(entry(3)+512,UINT64_MAX);
    assert(pwl_files_validate(archive,sizeof(archive))==PWL_ERR_BAD_IMAGE);
    fixture();put64(entry(4)+512,2802);
    assert(pwl_files_validate(archive,sizeof(archive))==PWL_ERR_BAD_IMAGE);
    fixture();entry(2)[0]='X';
    assert(pwl_files_validate(archive,sizeof(archive))==PWL_ERR_BAD_IMAGE);
    fixture();entry(1)[0]='\\';entry(1)[2]='.';entry(1)[4]='.';entry(1)[6]=0;
    assert(pwl_files_validate(archive,sizeof(archive))==PWL_ERR_BAD_IMAGE);
    fixture();memcpy(entry(4),entry(3),512);
    assert(pwl_files_validate(archive,sizeof(archive))==PWL_ERR_BAD_IMAGE);
    fixture();put32(archive+12,UINT32_MAX);
    assert(pwl_files_validate(archive,sizeof(archive))==PWL_ERR_BAD_IMAGE);
    fixture();
    for (size_t i=0;i<sizeof(archive);i++) assert(pwl_files_validate(archive,i)!=PWL_OK);
    puts("resident files: hierarchy, case lookup, partial reads, EOF and malformed archives passed");
}

#include <assert.h>
#include <string.h>
#include "../payload/dump_checks.h"
static void put(unsigned char*p,uint64_t x,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(unsigned char)(x>>(8*i));}
int main(void){unsigned char h[176]={0};uint64_t n=0,base=0xffffffff82200000ULL;
memcpy(h,"\177ELF",4);h[4]=2;h[5]=1;h[6]=1;put(h+18,62,2);put(h+52,64,2);put(h+54,56,2);put(h+56,2,2);put(h+32,64,8);
put(h+64,1,4);put(h+80,base,8);put(h+104,0x4000,8);put(h+112,0x4000,8);
put(h+120,1,4);put(h+136,base+0x8000,8);put(h+160,123,8);put(h+168,0x4000,8);
assert(pwl_dump_extent(h,sizeof h,base+0x100000,&n)==0 && n==0x807b);
assert(pwl_dump_extent(h,175,base,&n)!=0);
h[0]=0;assert(pwl_dump_extent(h,sizeof h,base,&n)!=0);h[0]=127;
put(h+56,129,2);assert(pwl_dump_extent(h,sizeof h,base,&n)!=0);put(h+56,2,2);
put(h+168,3,8);assert(pwl_dump_extent(h,sizeof h,base,&n)!=0);put(h+168,0x4000,8);
put(h+160,PWL_DUMP_LIMIT,8);assert(pwl_dump_extent(h,sizeof h,base,&n)!=0);
put(h+136,UINT64_MAX-3,8);put(h+160,8,8);assert(pwl_dump_extent(h,sizeof h,base,&n)!=0);
put(h+136,base+0x8000,8);put(h+160,123,8);put(h+80,0x1000,8);assert(pwl_dump_extent(h,sizeof h,base,&n)!=0);
return 0;}

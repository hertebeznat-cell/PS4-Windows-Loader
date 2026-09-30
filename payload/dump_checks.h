#ifndef PWL_DUMP_CHECKS_H
#define PWL_DUMP_CHECKS_H
#include <stdint.h>
#ifndef PWL_DUMP_SDK
#include <stddef.h>
#endif
#define PWL_DUMP_LIMIT (64ULL * 1024 * 1024)
static uint64_t pwl_le(const unsigned char *p, unsigned n) {
  uint64_t x = 0; for (unsigned i=0;i<n;i++) x |= (uint64_t)p[i] << (8*i); return x;
}
static int pwl_dump_extent(const unsigned char *h, size_t bytes, uint64_t live, uint64_t *extent) {
  if (!h || !extent || bytes < 64 || h[0]!=0x7f || h[1]!='E' || h[2]!='L' || h[3]!='F' ||
      h[4]!=2 || h[5]!=1 || h[6]!=1 || pwl_le(h+18,2)!=62 || pwl_le(h+52,2)!=64 || pwl_le(h+54,2)!=56) return -1;
  uint64_t off=pwl_le(h+32,8), num=pwl_le(h+56,2), lo=UINT64_MAX, hi=0;
  if (!num || num>128 || off<64 || off>bytes || num>(bytes-off)/56) return -1;
  for(uint64_t i=0;i<num;i++) {
    const unsigned char *p=h+off+i*56;
    if(pwl_le(p,4)!=1) continue;
    uint64_t va=pwl_le(p+16,8), size=pwl_le(p+40,8), align=pwl_le(p+48,8);
    if(!size) continue;
    if(size>PWL_DUMP_LIMIT || va>UINT64_MAX-size || (align && (align&(align-1)))) return -1;
    if(va<lo)lo=va;
    if(va+size>hi)hi=va+size;
  }
  if(lo!=live && lo!=0xffffffff82200000ULL) return -1;
  if(hi<=lo || hi-lo>PWL_DUMP_LIMIT) return -1;
  *extent=hi-lo; return 0;
}
#endif

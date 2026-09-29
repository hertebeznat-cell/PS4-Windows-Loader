#ifndef TEST_PE_FIXTURE_H
#define TEST_PE_FIXTURE_H
#include <stdint.h>
#include <string.h>
#define PE_FIXTURE_BYTES 0x800U
#define PE_OPT 0x98U
#define PE_SEC 0x188U
#define PE_BASE UINT64_C(0x140000000)
static void pe16(unsigned char *p, uint16_t n)
{ p[0] = (unsigned char)n; p[1] = (unsigned char)(n >> 8); }
static void pe32(unsigned char *p, uint32_t n)
{ pe16(p, (uint16_t)n); pe16(p + 2, (uint16_t)(n >> 16)); }
static void pe64(unsigned char *p, uint64_t n)
{ pe32(p, (uint32_t)n); pe32(p + 4, (uint32_t)(n >> 32)); }
static uint64_t pe_get64(const unsigned char *p)
{ uint64_t n = 0; unsigned i; for (i = 0; i < 8; ++i) n |= (uint64_t)p[i] << (8 * i); return n; }
static void pe_fixture(unsigned char *p)
{
    unsigned i;
    memset(p, 0, PE_FIXTURE_BYTES);
    p[0] = 'M'; p[1] = 'Z'; pe32(p + 0x3c, 0x80);
    p[0x80] = 'P'; p[0x81] = 'E';
    pe16(p + 0x84, 0x8664); pe16(p + 0x86, 3);
    pe16(p + 0x94, 0xf0); pe16(p + 0x96, 0x22);
    pe16(p + PE_OPT, 0x20b); pe32(p + PE_OPT + 16, 0x1000);
    pe64(p + PE_OPT + 24, PE_BASE);
    pe32(p + PE_OPT + 32, 0x1000); pe32(p + PE_OPT + 36, 0x200);
    pe32(p + PE_OPT + 56, 0x4000); pe32(p + PE_OPT + 60, 0x200);
    pe16(p + PE_OPT + 68, 10); pe32(p + PE_OPT + 108, 16);
    pe32(p + PE_OPT + 112 + 5 * 8, 0x3000);
    pe32(p + PE_OPT + 116 + 5 * 8, 12);
    for (i = 0; i < 3; ++i) {
        unsigned char *s = p + PE_SEC + i * 40;
        pe32(s + 8, i == 1 ? 0x400 : 0x200);
        pe32(s + 12, (i + 1) * 0x1000); pe32(s + 16, 0x200);
        pe32(s + 20, (i + 1) * 0x200);
        pe32(s + 36, i == 0 ? 0x60000020 : i == 1 ? 0xc0000040 : 0x42000040);
    }
    p[0x200] = 0xc3; /* Never executed. */
    pe64(p + 0x400, PE_BASE + 0x1010);
    pe32(p + 0x600, 0x2000); pe32(p + 0x604, 12);
    pe16(p + 0x608, 0xa000); /* DIR64; next entry is ABSOLUTE padding. */
}
#endif

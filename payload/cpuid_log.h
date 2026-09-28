#ifndef PS4WL_CPUID_LOG_H
#define PS4WL_CPUID_LOG_H

/* Requires u32, and caller-provided log sinks. CPU48 is the existing log schema. */
static void pwl_cpuid(u32 leaf,u32 subleaf,u32 *a,u32 *b,u32 *c,u32 *d)
{
    __asm__ volatile("cpuid" : "=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d)
                     : "a"(leaf),"c"(subleaf));
}

static void pwl_log_cpuid(u32 leaf,u32 subleaf,void (*hex)(const char*,u64))
{
    u32 a,b,c,d;
    hex("CPU48: leaf=",leaf);
    hex("CPU48: subleaf=",subleaf);
    pwl_cpuid(leaf,subleaf,&a,&b,&c,&d);
    hex("CPU48: eax=",a);
    hex("CPU48: ebx=",b);
    hex("CPU48: ecx=",c);
    hex("CPU48: edx=",d);
}

static void pwl_probe_cpu(void (*line)(const char*),
                          void (*hex)(const char*,u64))
{
    u32 a,b,c,d,basic_max,extended_max;
    pwl_cpuid(0,0,&a,&b,&c,&d);
    basic_max=a;
    line("CPU48: CPUID raw registers begin\n");
    pwl_log_cpuid(0,0,hex);
    if(basic_max>=1U)pwl_log_cpuid(1,0,hex);
    if(basic_max>=7U)pwl_log_cpuid(7,0,hex);
    pwl_cpuid(0x80000000U,0,&a,&b,&c,&d);
    extended_max=a;
    pwl_log_cpuid(0x80000000U,0,hex);
    if(extended_max>=0x80000001U)pwl_log_cpuid(0x80000001U,0,hex);
    if(extended_max>=0x8000000aU)pwl_log_cpuid(0x8000000aU,0,hex);
    line("CPU48: CPUID raw registers end\n");
}

#endif

/* Compiler-generated aggregate copies also resolve inside the resident core. */
#include <stddef.h>
#include <stdint.h>

void *memset(void *destination, int value, size_t count)
{
    unsigned char *d = destination;
    size_t i;
    for (i = 0; i < count; ++i) d[i] = (unsigned char)value;
    return destination;
}

void *memcpy(void *destination, const void *source, size_t count)
{
    unsigned char *d = destination;
    const unsigned char *s = source;
    size_t i;
    for (i = 0; i < count; ++i) d[i] = s[i];
    return destination;
}

void *memmove(void *destination, const void *source, size_t count)
{
    unsigned char *d = destination;
    const unsigned char *s = source;
    size_t i;
    if ((uintptr_t)d > (uintptr_t)s && (uintptr_t)d - (uintptr_t)s < count)
        for (i = count; i != 0; --i) d[i - 1] = s[i - 1];
    else
        for (i = 0; i < count; ++i) d[i] = s[i];
    return destination;
}

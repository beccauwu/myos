#ifndef _STDLIB_H
#define _STDLIB_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);

void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
[[noreturn]] void abort(void);

#endif // _STDLIB_H
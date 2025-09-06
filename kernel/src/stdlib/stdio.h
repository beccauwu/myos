#ifndef STDIO_H
#define STDIO_H

#include <limine.h>
#include <printf.h>

[[noreturn]] void __assert_fail(const char *assertion, const char *file, int line, const char *func);

#undef assert
#define assert(x) ((void)((x) || (__assert_fail(#x, __FILE__, __LINE__, __func__),0)))

#define kerror(fmt, ...) printf("\e[31m[kernel] [error]\e[0m: " fmt, __VA_ARGS__)
#define kwarn(fmt, ...) printf("\e[33m[kernel] [warning]\e[0m: " fmt, __VA_ARGS__)
#define kinfo(fmt, ...) printf("\e[36m[kernel] [info]\e[0m: " fmt, __VA_ARGS__)
#define kpanic(fmt, ...) (printf("\e[31m[kernel] [panic]\e[0m: " fmt, __VA_ARGS__), abort())

#define COLOUR(val) ((((uint64_t)(val) << 24) | ((val) >> 8)) & 0xffffffff)

void init_io(struct limine_framebuffer *);

#endif // STDIO_H
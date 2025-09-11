#ifndef STDIO_H
#define STDIO_H

#include <limine.h>
#include <stddef.h>

[[noreturn]] void     __assert_fail(const char *assertion, const char *file,
                                    int line, const char *func);
static const uint32_t UTF32_MAX = 0x10FFFF;
#undef assert
#define assert(x) \
  ((void)((x) || (__assert_fail(#x, __FILE__, __LINE__, __func__), 0)))

#define kerror(fmt, ...) \
  printf("\e[31m[kernel] [error]\e[0m: " fmt, __VA_ARGS__)
#define kwarn(fmt, ...) \
  printf("\e[33m[kernel] [warning]\e[0m: " fmt, __VA_ARGS__)
#define kinfo(fmt, ...) printf("\e[36m[kernel] [info]\e[0m: " fmt, __VA_ARGS__)
#define kpanic(fmt, ...) \
  (printf("\e[31m[kernel] [panic]\e[0m: " fmt, __VA_ARGS__), abort())

#define COLOUR(val) ((((uint64_t)(val) << 24) | ((val) >> 8)) & 0xffffffff)
void printf(const char *format, ...);
void printw(const wchar_t *buf);
void print(const char *buf);
void putwchar(wchar_t c);
void _putchar(char c);
#define putchar(c) _Generic((c), char: _putchar, wchar_t: putwchar)((c))

int  utf8_to_utf32(uint32_t *out_char32, const char *utf8_buf, int *opt_error);
void init_io(struct limine_framebuffer *);
#endif  // STDIO_H
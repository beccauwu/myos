#include "stdlib.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#define DOLIVECDEF extern
#define OLIVEC_NO_SSE
#include "fonts/noto_bold.h"
#include "fonts/noto_reg.h"
#define STB_SPRINTF_NOFLOAT
#include "stb_sprintf.h"
#include "stdio.h"
#include <olive.c>

/*
`c_utf8_buf_to_utf32_char_b()` in https://github.com/iboB/c-utf8 by Borislav
Stanimirov
*/
static_assert(sizeof(wchar_t) == sizeof(uint32_t),
              "we assume wchar_t is utf32");
int utf8_to_utf32(uint32_t *out_char32, const char *utf8_buf, int *opt_error) {
  static const char lengths[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                  1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
                                  0, 0, 2, 2, 2, 2, 3, 3, 4, 0 };
  static const int  masks[]   = { 0x00, 0x7f, 0x1f, 0x0f, 0x07 };
  static const int  shiftc[]  = { 0, 18, 12, 6, 0 };

  const uint8_t *const s   = (const uint8_t *)utf8_buf;
  int                  len = lengths[s[0] >> 3];

  /* Assume a four-byte character and load four bytes. Unused bits are
   * shifted out.
   */
  *out_char32 = (uint32_t)(s[0] & masks[len]) << 18;
  *out_char32 |= (uint32_t)(s[1] & 0x3f) << 12;
  *out_char32 |= (uint32_t)(s[2] & 0x3f) << 6;
  *out_char32 |= (uint32_t)(s[3] & 0x3f) << 0;
  *out_char32 >>= shiftc[len];

  if(opt_error) {
    static const uint32_t mins[]   = { 4194304, 0, 128, 2048, 65536 };
    static const int      shifte[] = { 0, 6, 4, 2, 0 };
    int *const            e        = opt_error;
    const uint32_t        c        = *out_char32;
    /* Accumulate the various error conditions. */
    *e = (c < mins[len]) << 6;       // non-canonical encoding
    *e |= ((c >> 11) == 0x1b) << 7;  // surrogate half?
    *e |= (c > UTF32_MAX) << 8;      // out of range?
    *e |= (s[1] & 0xc0) >> 2;
    *e |= (s[2] & 0xc0) >> 4;
    *e |= (s[3]) >> 6;
    *e ^= 0x2a;  // top two bits of each tail byte correct?
    *e >>= shifte[len];
  }

  return len + !len;
}

typedef struct console_state {
  Olivec_Canvas fb;
  Olivec_Canvas canvas;
  size_t        font_size;
  uint32_t      fg;
  uint32_t      bg;
  enum {
    CONSOLE_DEFAULT = 0,
    CONSOLE_ESCAPE,
    CONSOLE_FOREGROUND,
    CONSOLE_BACKGROUND
  } state;
  const Font *font;
  struct {
    size_t x, y;
  } cursor;
};

static struct console_state console = { 0 };

#define OLIVEC_RED(color)   (((color) & 0x000000FF) >> (8 * 0))
#define OLIVEC_GREEN(color) (((color) & 0x0000FF00) >> (8 * 1))
#define OLIVEC_BLUE(color)  (((color) & 0x00FF0000) >> (8 * 2))
#define OLIVEC_ALPHA(color) (((color) & 0xFF000000) >> (8 * 3))
#define OLIVEC_RGBA(r, g, b, a)                            \
  ((((r) & 0xFF) << (8 * 0)) | (((g) & 0xFF) << (8 * 1)) | \
   (((b) & 0xFF) << (8 * 2)) | (((a) & 0xFF) << (8 * 3)))

static const uint32_t ansi_colours[8] = {
  [0] = COLOUR(0x181818ff), [1] = COLOUR(0xff5794ff), [2] = COLOUR(0x66ffa8ff),
  [3] = COLOUR(0xf1ff99ff), [4] = COLOUR(0x61a0ffff), [5] = COLOUR(0xd966ffff),
  [6] = COLOUR(0x66e0ffff), [7] = COLOUR(0xe3e3e3ff)
};

#define SCREEN_PAD 40

void putwchar(wchar_t c) {
  size_t glyph_height = console.font->glyph_height;
  size_t ny           = console.cursor.y;
  size_t nx           = console.cursor.x;
  switch(console.state) {
    case CONSOLE_ESCAPE: {
      switch(c) {
        case '[':
        case ';': return;  // we ignore these
        case '1': console.font = &noto_bold; return;
        case '3': console.state = CONSOLE_FOREGROUND; return;
        case '4': console.state = CONSOLE_BACKGROUND; return;
        case '0': {
          // reset
          console.fg   = COLOUR(0xe3e3e3ff);
          console.bg   = COLOUR(0x181818ff);
          console.font = &noto_reg;
          return;
        }
        case 'J': {
          olivec_fill(console.fb, console.bg);
          console.cursor.y = 0;
          console.cursor.x = 0;
        }
        case 'm':
        default: {
          console.state =
            CONSOLE_DEFAULT;  // we don't want to be stuck in a loop
          return;
        }
      }
    } break;
    case CONSOLE_FOREGROUND: {
      if(c >= '0' && c <= '7')
        console.fg = ansi_colours[c - '0'];
      console.state = CONSOLE_ESCAPE;
      return;
    }
    case CONSOLE_BACKGROUND: {
      if(c >= '0' && c <= '7')
        console.bg = ansi_colours[c - '0'];
      console.state = CONSOLE_ESCAPE;
      return;
    }
    default: break;
  }
  if(c == '\e') {
    console.state = CONSOLE_ESCAPE;
    return;
  }
  if(c == '\n') {
    ny += glyph_height;
    if(console.cursor.y > (console.canvas.height - glyph_height)) {
      olivec_fill(console.fb, console.bg);
      ny = 0;
    }
    console.cursor.y = ny;
    console.cursor.x = 0;
    return;
  }
  if(c == '\r') {
    console.cursor.x = 0;
    olivec_rect(console.canvas,
                0,
                console.cursor.y,
                console.canvas.width,
                console.canvas.height,
                console.bg);
    return;
  }
  if(c == '\t') {
    print("  ");
    return;
  }

  if(nx + console.font->glyph_height >= console.canvas.width) {
    nx = 0;
    ny += glyph_height;
    if(ny > (console.canvas.height - glyph_height)) {
      // TODO: allow user to scroll (keep a buffer of stuff)
      olivec_fill(console.fb, console.bg);
      ny = 0;
    }
  }
  size_t     idx   = c - console.font->first_char;
  baked_char cdata = console.font->cdata[idx];
  for(size_t dy = cdata.y0; dy < cdata.y1; ++dy) {
    for(size_t dx = cdata.x0; dx < cdata.x1; ++dx) {
      unsigned char intensity =
        console.font->bitmap[dy * console.font->bitmap_width + dx];
      // NOTE: padding is required, xoff/yoff can be < 0 and we don't want OS to
      // crash. Maybe find a better way to do this?? (instead of magic value :b)
      uint64_t x = SCREEN_PAD + console.cursor.x + dx - cdata.x0 + cdata.xoff;
      uint64_t y = SCREEN_PAD + console.cursor.y + dy - cdata.y0 + cdata.yoff;
      uint32_t colour = console.bg;
      uint32_t fg     = (console.fg & 0xffffff) | (intensity << (8 * 3));

      olivec_blend_color(&colour, fg);
      OLIVEC_PIXEL(console.canvas, x, y) = colour;
    }
  }
  nx += cdata.xadvance;
  console.cursor.x = nx;
  console.cursor.y = ny;
}

void _putchar(char c) {
  putwchar((wchar_t)c);
}

void printf(const char *format, ...) {
  static char printf_buf[1 << 12];
  va_list     args;
  va_start(args, format);
  stbsp_vsnprintf(
    printf_buf, sizeof(printf_buf) / sizeof(printf_buf[0]), format, args);
  va_end(args);
  for(size_t i = 0; printf_buf[i] != '\0';) {
    uint32_t c;
    int      n = utf8_to_utf32(&c, &printf_buf[i], NULL);
    putwchar(c);
    i += n;
  }
}

void printw(const wchar_t *buf) {
  size_t i;
  for(i = 0; buf[i] != '\0'; ++i) putchar(buf[i]);
};
void print(const char *buf) {
  size_t i;
  for(i = 0; buf[i] != '\0'; ++i) putchar(buf[i]);
};

[[noreturn]] void __assert_fail(const char *assertion, const char *file,
                                int line, const char *func) {
  printf(
    "\e[31m[ERROR]\e[0m assertion '%s' failed\n"
    "\t\t%s:%d (in function %s())\n",
    assertion,
    file,
    line,
    func);
  abort();
}

void init_io(struct limine_framebuffer *framebuffer) {
  Olivec_Canvas fb = olivec_canvas(framebuffer->address,
                                   framebuffer->width,
                                   framebuffer->height,
                                   (framebuffer->pitch / sizeof(uint32_t)));
  // Olivec_Canvas oc  = olivec_subcanvas(fb,
  //                                     SCREEN_PAD,
  //                                     SCREEN_PAD,
  //                                     fb.width - 2 * SCREEN_PAD,
  //                                     fb.height - 2 * SCREEN_PAD);
  console.fb        = fb;
  console.canvas    = fb;
  console.font_size = 1;
  console.font      = &noto_reg;
  console.bg        = COLOUR(0x181818ff);
  console.fg        = COLOUR(0xe3e3e3ff);
  olivec_fill(fb, console.bg);
}
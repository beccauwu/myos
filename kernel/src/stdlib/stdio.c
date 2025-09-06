#include "stdlib.h"
#include <stdint.h>
#include <olive.c>
#include "stdio.h"
#include "fonts/courier.h"

typedef struct console_state {
  Olivec_Canvas fb;
  Olivec_Canvas canvas;
  size_t font_size;
  uint32_t fg;
  uint32_t bg;
  enum {
    CONSOLE_DEFAULT = 0,
    CONSOLE_ESCAPE,
    CONSOLE_FOREGROUND,
    CONSOLE_BACKGROUND
  } state;
  Olivec_Font font;
  struct {
    size_t x,y;
  } cursor;
};

static struct console_state console = {0};

/* inner loop from olivec_text */
OLIVECDEF void olivec_putchar(Olivec_Canvas oc, char character, int tx, int ty, Olivec_Font font, size_t glyph_size, uint32_t color)
{
    int gx = tx;
    int gy = ty;
    const char *glyph = &font.glyphs[(character)*sizeof(char)*font.width*font.height];
    for (int dy = 0; (size_t) dy < font.height; ++dy) {
        for (int dx = 0; (size_t) dx < font.width; ++dx) {
            int px = gx + dx*glyph_size;
            int py = gy + dy*glyph_size;
            if (0 <= px && px < (int) oc.width && 0 <= py && py < (int) oc.height) {
                if (glyph[dy*font.width + dx]) {
                    olivec_rect(oc, px, py, glyph_size, glyph_size, color);
                }
            }
        }
    }
}

static const uint32_t ansi_colours[8] = {
  [0] = COLOUR(0x181818ff),
  [1] = COLOUR(0xff5794ff),
  [2] = COLOUR(0x66ffa8ff),
  [3] = COLOUR(0xf1ff99ff),
  [4] = COLOUR(0x61a0ffff),
  [5] = COLOUR(0xd966ffff),
  [6] = COLOUR(0x66e0ffff),
  [7] = COLOUR(0xe3e3e3ff)
};

void _putchar(char c) {
  size_t glyph_height = console.font.height * console.font_size;
  size_t glyph_width = console.font.width * console.font_size;
  size_t nx = console.cursor.x + glyph_width;
  size_t ny = console.cursor.y;
  
  switch(console.state) {
    case CONSOLE_ESCAPE: {
      switch(c) {
        case '[': case ';': return; //we ignore these
        case '1': console.font = courier.bold; return;
        case '3': console.state = CONSOLE_FOREGROUND; return;
        case '4': console.state = CONSOLE_BACKGROUND; return;
        case '0': {
          // reset
          console.fg = COLOUR(0xe3e3e3ff);
          console.bg = COLOUR(0x181818ff);
          console.font = courier.regular;
          return;
        }
        case 'J': {
          olivec_fill(console.fb,console.bg);
          console.cursor.y = 0;
          console.cursor.x = 0;
        }
        case 'm':
        default: {
          console.state = CONSOLE_DEFAULT; // we don't want to be stuck in a loop
          return;
        }
      }
    } break;
    case CONSOLE_FOREGROUND: {
      if(c >= '0' && c <= '7') console.fg = ansi_colours[c - '0'];
      console.state = CONSOLE_ESCAPE;
      return;
    }
    case CONSOLE_BACKGROUND: {
      if(c >= '0' && c <= '7') console.bg = ansi_colours[c - '0'];
      console.state = CONSOLE_ESCAPE;
      return;
    }
    default: break;
  }
  if(c == '\e') {
    console.state = CONSOLE_ESCAPE;
    return;
  }
  if (c == '\n') {
    ny += glyph_height;
    if(console.cursor.y > (console.canvas.height - glyph_height)) {
      olivec_fill(console.fb,console.bg);
      ny = 0;
    }
    console.cursor.y = ny;
    console.cursor.x = 0;
    return;
  }
  if(c == '\r') {
    console.cursor.x = 0;
    olivec_rect(console.canvas, 0,console.cursor.y, console.canvas.width, console.canvas.height, console.bg);
    return;
  }
  if(c == '\t') {
    console.cursor.x += 2*glyph_width;
    return;
  }
  
  
  if(nx > console.canvas.width - glyph_width) {
    nx = 0;
    ny += glyph_height;
    if(ny > (console.canvas.height - glyph_height)) {
      olivec_fill(console.fb,console.bg);
      ny = 0;
    }
  }
  olivec_rect(console.canvas, console.cursor.x, console.cursor.y, glyph_width, glyph_height, console.bg);
  olivec_putchar(console.canvas, c, console.cursor.x, console.cursor.y, console.font, console.font_size, console.fg);
  console.cursor.x = nx;
  console.cursor.y = ny;
}
[[noreturn]] void __assert_fail(const char *assertion, const char *file, int line, const char *func) {
  printf(
    "\e[31m[ERROR]\e[0m assertion '%s' failed\n"
    "\t\t%s:%d (in function %s())\n",
    assertion, file, line, func
  );
  abort();
}
#define SCREEN_PAD 4
void init_io(struct limine_framebuffer *framebuffer) {
  Olivec_Canvas fb = olivec_canvas(framebuffer->address, framebuffer->width, framebuffer->height, (framebuffer->pitch / sizeof(uint32_t)));
  Olivec_Canvas oc = olivec_subcanvas(fb, SCREEN_PAD, SCREEN_PAD, fb.width-2*SCREEN_PAD, fb.height-2*SCREEN_PAD);
  console.fb = fb;
  console.canvas = oc;
  console.font_size = 1;
  console.font = courier.regular;
  console.bg = COLOUR(0x181818ff);
  console.fg = COLOUR(0xe3e3e3ff);
  olivec_fill(fb, console.bg);
}
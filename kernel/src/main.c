#include "stdlib.h"
#include <limine.h>
#include <stddef.h>
#include <printf.h>
#include <stdint.h>
// Set the base revision to 3, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.  

__attribute__((
    used, section(".limine_requests"))) static volatile LIMINE_BASE_REVISION(3);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((
    used,
    section(
        ".limine_requests"))) static volatile struct limine_framebuffer_request
    framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used,
               section(".limine_requests_"
                       "start"))) static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((
    used,
    section(
        ".limine_requests_end"))) static volatile LIMINE_REQUESTS_END_MARKER;

// from https://github.com/azmr/blit-fonts/blob/master/blit32.h
unsigned long font_glyphs[95] = {
    /* all chars up to 32 are non-printable */
    0x00000000, 0x08021084, 0x0000294a, 0x15f52bea, 0x08fa38be, 0x33a22e60,
    0x2e94d8a6, 0x00001084, 0x10421088, 0x04421082, 0x00a23880, 0x00471000,
    0x04420000, 0x00070000, 0x0c600000, 0x02222200, 0x1d3ad72e, 0x3e4214c4,
    0x3e22222e, 0x1d18320f, 0x210fc888, 0x1d183c3f, 0x1d17844c, 0x0222221f,
    0x1d18ba2e, 0x210f463e, 0x0c6018c0, 0x04401000, 0x10411100, 0x00e03800,
    0x04441040, 0x0802322e, 0x3c1ef62e, 0x231fc544, 0x1f18be2f, 0x3c10862e,
    0x1f18c62f, 0x3e10bc3f, 0x0210bc3f, 0x1d1c843e, 0x2318fe31, 0x3e42109f,
    0x0c94211f, 0x23149d31, 0x3e108421, 0x231ad6bb, 0x239cd671, 0x1d18c62e,
    0x0217c62f, 0x30eac62e, 0x2297c62f, 0x1d141a2e, 0x0842109f, 0x1d18c631,
    0x08454631, 0x375ad631, 0x22a21151, 0x08421151, 0x3e22221f, 0x1842108c,
    0x20820820, 0x0c421086, 0x00004544, 0xbe000000, 0x00000082, 0x1c97b000,
    0x0e949c21, 0x1c10b800, 0x1c94b908, 0x3c1fc5c0, 0x42211c4c, 0x4e87252e,
    0x12949c21, 0x0c210040, 0x8c421004, 0x12519521, 0x0c210842, 0x235aac00,
    0x12949c00, 0x0c949800, 0x4213a526, 0x7087252e, 0x02149800, 0x0e837000,
    0x0c213c42, 0x0e94a400, 0x0464a400, 0x155ac400, 0x36426c00, 0x4e872529,
    0x1e223c00, 0x1843188c, 0x08421084, 0x0c463086, 0x0006d800,
};

typedef struct fb_state {
  struct limine_framebuffer *framebuffer;
  size_t col;
  size_t row;
};

#define FB_AT(fb, row, col) ((uint32_t*)(fb->address))[(row)*(fb->pitch / sizeof(uint32_t)) + (col)]

static struct fb_state fb_curr = {0};
#define FONT_SCALE 2
void _putchar(char c) {
  struct limine_framebuffer *fb_ptr = fb_curr.framebuffer;
  // i * (framebuffer->pitch / 4)
  if (c == '\n') {
    fb_curr.row += 6 * FONT_SCALE + 3;
    fb_curr.col = 0;
    return;
  }
  if(c == '\r') {
    fb_curr.col = 0;
    return;
  }
  unsigned long glyph = font_glyphs[c - ' '];
  size_t row_scaled = 0;
  for (size_t glyph_row = 0; glyph_row < 6; ++glyph_row) {
    size_t col_scaled = 0;
    for (size_t glyph_col = 0; glyph_col < 5; ++glyph_col) {
      // uint32_t colour = (glyph >> (glyph_row*5 + col) & 1) ? 0xffffff :
      // 0x000000;
      if (glyph >> (glyph_row * 5 + glyph_col) & 1) {
        for (size_t i = 0; i < FONT_SCALE; ++i) {
          for (size_t j = 0; j < FONT_SCALE; ++j) {
            size_t row = fb_curr.row + glyph_row + i + row_scaled;
            size_t col = fb_curr.col + glyph_col + j + col_scaled;
            FB_AT(fb_ptr, row, col) = 0xffffff;
          }
        }
      }
      col_scaled += FONT_SCALE-1;
    }
    row_scaled += FONT_SCALE-1;
  }
  fb_curr.col += FONT_SCALE*5 + 2;
}
#define putchar _putchar

void print(const char *s) {
  for (size_t i = 0; s[i] != '\0'; ++i)
    putchar(s[i]);
}

#define WIDTH 1280
#define HEIGHT 800


#define COLOUR(val) ((((uint64_t)(val) << 24) | ((val) >> 8)) & 0xffffffff)

#define OLIVEC_IMPLEMENTATION
#include <olive.c>
#include "lotion_font.h"
// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void) {
  // Ensure the bootloader actually understands our base revision (see
  // spec).
  if (LIMINE_BASE_REVISION_SUPPORTED == false) {
    hcf();
  }

  // Ensure we got a framebuffer.
  if (framebuffer_request.response == NULL ||
      framebuffer_request.response->framebuffer_count < 1) {
    hcf();
  }

  // Fetch the first framebuffer.
  struct limine_framebuffer *framebuffer =
      framebuffer_request.response->framebuffers[0];

  fb_curr.framebuffer = framebuffer;
  // printf("hello, world\n");
  // printf("width: %ld, height: %ld\n", framebuffer->width, framebuffer->height);

  Olivec_Canvas oc = olivec_canvas(framebuffer->address, framebuffer->width, framebuffer->height, (framebuffer->pitch / sizeof(uint32_t)));
  //printf("0x%x -> 0x%x", 0xFF2D00BC, RGBA2ARGB(0xFF2D00BC));
  olivec_fill(oc, COLOUR(0x181818FF));
  olivec_text(oc, " !\"#$%&'()*+,-./"
"0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
"abcdefghijklmnopqrstuvwxyz{|}~", 0,0,lotion_font, 1, 0xffffffff);
  
  // Note: we assume the framebuffer model is RGB with 32-bit pixels.
  // for (size_t i = 0; i < 100; i++) {
  //   volatile uint32_t *fb_ptr = framebuffer->address;
  //   fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
  // }

  // We're done, just hang...
  hcf();
}

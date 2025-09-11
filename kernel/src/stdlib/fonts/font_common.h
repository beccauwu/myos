#ifndef _FONT_COMMON_H
#define _FONT_COMMON_H

#include <stdint.h>

/*
`stbtt_bakedchar` from stb_truetype.h

with `xoff`, `yoff`, and `xadvance` changed from float to int
(to avoid sse registers)
*/
typedef struct {
  uint16_t x0,y0,x1,y1; // coordinates of bbox in bitmap
  int32_t  xoff,yoff,xadvance;
} baked_char;

typedef struct {
  const baked_char     *cdata;
  const unsigned char  *bitmap;
  const uint64_t        bitmap_height;
  const uint64_t        bitmap_width;
  const uint64_t        first_char;
  const uint64_t        nchars;
  const uint64_t        glyph_height;
} Font;

#endif // _FONT_COMMON_H


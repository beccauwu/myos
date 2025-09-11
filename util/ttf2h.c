#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#define FLAG_IMPLEMENTATION
#include "flag.h"
#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "nob.h"
#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate
                                     // implementation
#include "stb_truetype.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static struct {
  char *name;
  char *name_upper;
  char *infile;
  char *outfile;
  int   first_char;
  int   nchars;
  int   height;
  char *header;
} config = { 0 };

// 0x0020 - 0x007F 	C0 Controls and Basic Latin (Basic Latin)
// 0x0080 - 0x00FF 	C1 Controls and Latin-1 Supplement (Latin-1 Supplement)
// 0x0100 - 0x017F 	Latin Extended-A
// 0x0180 - 0x024F 	Latin Extended-B

#define DEFAULT_GLYPH_HEIGHT 16
#define GLYPHS_PER_ROW       4
#define DEFAULT_N_CHARS      (0x024F - 0x0020)
// #define HEIGHT                                                \
//   (((DEFAULT_N_CHARS +                                        \
//      (GLYPHS_PER_ROW - (DEFAULT_N_CHARS % GLYPHS_PER_ROW))) / \
//     GLYPHS_PER_ROW) *                                         \
//    DEFAULT_GLYPH_HEIGHT)
// #define WIDTH (DEFAULT_GLYPH_HEIGHT * GLYPHS_PER_ROW)
const wchar_t *codepoints  = L"abcdefghijklmnopqrstuvwxyzåäöĳæø×";
unsigned char *temp_bitmap = NULL;
// 143360
stbtt_bakedchar *cdata = NULL;  // ASCII 32..126 is 95 glyphs

static char *_program_name = NULL;

static inline char *program_name(void) {
  if(_program_name == NULL) {
    _program_name = realpath(flag_program_name(), NULL);
  }
  return _program_name;
}

void print_usage(FILE *stream) {
  fprintf(stream,
          "\nGenerate header with bitmap and glyph data from a truetype font "
          "file\n\n");
  fprintf(stream, "Usage: %s [OPTIONS] <input.ttf>\n\n", program_name());
  fprintf(stream, "OPTIONS:\n");
  flag_print_options(stream);
}

static inline void stringify_and_add_name(const char *name) {
  String_Builder sb = { 0 };
  size_t         i  = 0;
  while(!isalpha(name[i])) i++;
  for(; name[i] != '\0'; ++i) {
    if(isalpha(name[i])) {
      da_append(&sb, tolower(name[i]));
      continue;
    }
    switch(name[i]) {
      case '.':
      case ' ':
      case '-': da_append(&sb, '_'); break;
      default: da_append(&sb, name[i]); break;
    }
  }
  sb_append_null(&sb);
  config.name = strdup(sb.items);
  for(i = 0; i < strlen(sb.items); ++i) sb.items[i] = toupper(sb.items[i]);
  config.name_upper = strdup(sb.items);
  sb_free(sb);
}
bool parse_options(int argc, char **argv) {
  char **outfile_opt = flag_str("o",
                                NULL,
                                "output filename\n"
                                "         (defaults to <input_ttf>.h)");
  char **header =
    flag_str("oh", NULL, "separate outfile for common definitions");
  char    **name_opt = flag_str("name",
                             NULL,
                             "name to use as prefix for variables/macros\n"
                                "         (defaults to <input_ttf>)");
  uint64_t *first    = flag_uint64("f",
                                0x0020,
                                "first character of font to output\n"
                                   "         by default U+0020 (space)");
  uint64_t *nchars =
    flag_uint64("n",
                DEFAULT_N_CHARS,
                "number of characters from n to output\n"
                "         by default from Basic Latin (U+0020)\n"
                "         to Latin Extended-B (U+024F)");
  uint64_t *height =
    flag_uint64("h", DEFAULT_GLYPH_HEIGHT, "height of a character (in pixels)");
  bool *help = flag_bool("help", false, "Print this help message");
  if(!flag_parse(argc, argv)) {
    print_usage(stderr);
    flag_print_error(stderr);
    return false;
  }
  if(*help) {
    print_usage(stderr);
    exit(0);
  }
  int    rest_argc = flag_rest_argc();
  char **rest_argv = flag_rest_argv();
  if(rest_argc <= 0) {
    print_usage(stderr);
    fprintf(stderr, "ERROR: no input file provided.\n");
    return false;
  }
  char *infile = shift(rest_argv, rest_argc);
  stringify_and_add_name(*name_opt == NULL ? infile : *name_opt);
  char *outfile = *outfile_opt;
  if(outfile == NULL) {
    outfile = temp_sprintf("%s.h", config.name);
  }
  config.infile     = infile;
  config.outfile    = outfile;
  config.first_char = *first;
  config.nchars     = *nchars;
  config.height     = *height;
  config.header     = *header;
  return true;
}

void render_header(String_Builder *sb) {
  sb_append_cstr(sb,
                 "#ifndef _FONT_COMMON_H\n"
                 "#define _FONT_COMMON_H\n\n");
  sb_append_cstr(sb, "#include <stdint.h>\n\n");
  sb_append_cstr(
    sb,
    "/*\n"
    "`stbtt_bakedchar` from stb_truetype.h\n\n"
    "with `xoff`, `yoff`, and `xadvance` changed from float to int\n"
    "(to avoid sse registers)\n"
    "*/\n"
    "typedef struct {\n"
    "  uint16_t x0,y0,x1,y1; // coordinates of bbox in bitmap\n"
    "  int32_t  xoff,yoff,xadvance;\n"
    "} baked_char;\n\n");
  sb_appendf(sb,
             "typedef struct {\n"
             "  const baked_char     *cdata;\n"
             "  const unsigned char  *bitmap;\n"
             "  const uint64_t        bitmap_height;\n"
             "  const uint64_t        bitmap_width;\n"
             "  const uint64_t        first_char;\n"
             "  const uint64_t        nchars;\n"
             "  const uint64_t        glyph_height;\n"
             "} Font;\n\n");
  sb_append_cstr(sb, "#endif // _FONT_COMMON_H\n\n");
}

int main(int argc, char **argv) {
  unsigned char   *temp_bitmap;
  stbtt_bakedchar *cdata;

  if(!parse_options(argc, argv))
    return 1;
  stbtt_fontinfo font;
  unsigned char *bitmap;
  int            w, h, i, j, c = (argc > 1 ? atoi(argv[1]) : 'a'),
                  s = (argc > 2 ? atoi(argv[2]) : 20);

  String_Builder ttf = { 0 };
  if(!read_entire_file(config.infile, &ttf))
    return 1;

  stbtt_InitFont(&font,
                 (unsigned char *)ttf.items,
                 stbtt_GetFontOffsetForIndex((unsigned char *)ttf.items, 0));
  int pw = config.height * GLYPHS_PER_ROW;
  int ph =
    ((config.nchars + (GLYPHS_PER_ROW - (config.nchars % GLYPHS_PER_ROW))) /
     GLYPHS_PER_ROW) *
    config.height;
  cdata       = malloc(sizeof(*cdata) * config.nchars);
  temp_bitmap = malloc(sizeof(*temp_bitmap) * pw * ph);
  assert(cdata != NULL);
  assert(temp_bitmap != NULL);
  int res = stbtt_BakeFontBitmap((unsigned char *)ttf.items,
                                 0,
                                 config.height,
                                 temp_bitmap,
                                 pw,
                                 ph,
                                 config.first_char,
                                 config.nchars,
                                 cdata);
  assert(res > 0);
  String_Builder sb = { 0 };
  sb_append_cstr(&sb,
                 "/*\n"
                 "separate header guard for common definitions\n"
                 "so multiple fonts may be included into same file\n"
                 "*/\n");
  render_header(&sb);
  // sb_appendf("#define SF_PRO_")
  sb_appendf(&sb,
             "#ifndef _%s_H\n"
             "#define _%s_H\n\n",
             config.name_upper,
             config.name_upper);
  sb_appendf(&sb, "#define %s_BITMAP_HEIGHT %d\n", config.name_upper, res);
  sb_appendf(&sb, "#define %s_BITMAP_WIDTH  %d\n\n", config.name_upper, pw);
  sb_appendf(
    &sb, "#define %s_GLYPH_HEIGHT  %d\n", config.name_upper, config.height);
  sb_appendf(&sb,
             "#define %s_FIRST_CHAR    0x%04X\n",
             config.name_upper,
             config.first_char);
  sb_appendf(&sb,
             "#define %s_LAST_CHAR     0x%04X\n",
             config.name_upper,
             config.nchars + config.first_char);
  sb_appendf(
    &sb, "#define %s_NCHARS        %d\n\n", config.name_upper, config.nchars);
  sb_appendf(&sb,
             "const baked_char %s_cdata[%s_NCHARS] = {\n",
             config.name,
             config.name_upper);
  for(size_t i = 0; i < config.nchars; ++i) {
    stbtt_bakedchar it       = cdata[i];
    int             xoff     = floorf(it.xoff);
    int             yoff     = floorf(it.yoff);
    int             xadvance = floorf(it.xadvance);
    sb_appendf(&sb,
               "  {%u,%u,%u,%u,%d,%d,%d},\n",
               it.x0,
               it.y0,
               it.x1,
               it.y1,
               xoff,
               yoff,
               xadvance);
  }
  sb_append_cstr(&sb, "};\n\n");
  sb_appendf(
    &sb, "const unsigned char %s_bitmap[%d] = {\n", config.name, res * pw);
  for(size_t row = 0; row < res; ++row) {
    sb_append_cstr(&sb, "  ");
    for(size_t col = 0; col < pw; ++col) {
      sb_appendf(&sb, "0x%-2X, ", temp_bitmap[row * pw + col]);
    }
    sb_append_cstr(&sb, "\n");
  }
  sb_append_cstr(&sb, "};\n\n");
  sb_appendf(&sb,
             "const Font %s = {\n"
             "  .cdata         = %s_cdata,\n"
             "  .bitmap        = %s_bitmap,\n"
             "  .bitmap_height = %s_BITMAP_HEIGHT,\n"
             "  .bitmap_width  = %s_BITMAP_WIDTH,\n"
             "  .first_char    = %s_FIRST_CHAR,\n"
             "  .nchars        = %s_NCHARS,\n"
             "  .glyph_height  = %d,\n"
             "};\n\n",
             config.name,
             config.name,
             config.name,
             config.name_upper,
             config.name_upper,
             config.name_upper,
             config.name_upper,
             config.height);
  sb_appendf(&sb, "#endif // _%s_H\n", config.name_upper);
  if(!write_entire_file(config.outfile, sb.items, sb.count))
    return 1;
  printf("wrote %.2fK to '%s'!\n", (float)sb.count / 1024, config.outfile);

  if(config.header != NULL) {
    sb.count = 0;
    render_header(&sb);
    if(!write_entire_file(config.header, sb.items, sb.count))
      return 1;
    printf("wrote %zuB to '%s'!\n", sb.count, config.outfile);
  }
  free(config.name);
  free(config.name_upper);
  free(cdata);
  free(temp_bitmap);
  sb_free(sb);
  return 0;
}
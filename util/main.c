#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/* 
generates font for Olive.c (https://github.com/tsoding/olive.c)
using bitmap png (i used https://stmn.itch.io/font2bitmap )
assumes image is 1 char wide and n chars high where n = sizeof(chars) - 1

simply configure below and run `gcc -o main main.c && main > file.h`
*/


/* characters in the order they appear in font (top->bottom) */
static const char chars[] = " !\"#$%&'()*+,-./"
"0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
"abcdefghijklmnopqrstuvwxyz{|}~";

#define CHAR_WIDE 14       // width of one character in pixels
#define CHAR_HIGH 14       // .. and height
#define FONT_NAME "lotion" // name to use as prefix in header
#define LEFT_MARGIN 0      // how many columns to shave off from left
#define RIGHT_MARGIN 0     // .. or right
#define TOP_MARGIN 0       // .. or rows from top
#define BOTTOM_MARGIN 0    /* .. or bottom (i found the site i used to 
                                 generate bitmap sqeezed font if it wasn't 
                                 rectangular there but it left huge anyway) */
#define ALPHA_THRESH 40    /* threshold for alpha channel 
                               (pixels lower than this will be left empty) */
#define FONT_FILE "lotion_14x14.png"

// comment this out if you want to use alpha threshold for all characters
//#define CASE_BY_CASE


typedef struct __attribute__((__packed__)) fontcfg {
  char *name;
  size_t w, h, alpha;
  size_t margins[4]; // ml,mr,mt,mb
};

void dump_cfg(struct fontcfg *cfg) {
  fprintf(stderr,
    "fontconfig {\n"
    "  font name    = %s\n"
    "  alpha thresh = %zu\n"
    "  size:\n"
    "    width  = %zu\n"
    "    height = %zu\n"
    "  margins:\n"
    "    left   = %zu\n"
    "    right  = %zu\n"
    "    top    = %zu\n"
    "    bottom = %zu\n"
    "};\n"
  ,cfg->name,cfg->alpha,cfg->w,cfg->h,
  cfg->margins[0],cfg->margins[1],cfg->margins[2],cfg->margins[3]);
}

void save_cfg(struct fontcfg *cfg, char *savefile) {
  FILE *f = fopen(savefile, "w+");

  size_t name_len = strlen(cfg->name);
  fwrite(&name_len, sizeof(size_t), 1, f);
  fwrite(cfg->name, 1, name_len, f);
  fwrite(&cfg->w, sizeof(size_t), 7, f);
  fflush(f);
  fclose(f);
  printf("config saved to %s\n", savefile);
}
void load_cfg(struct fontcfg *cfg, const char *file) {
  FILE *f = fopen(file, "r");
  size_t name_len;
  fread(&name_len, sizeof(size_t), 1, f);
  printf("%zu\n", name_len);
  cfg->name = malloc(name_len*sizeof(char)+1); // memory leak
  fread(cfg->name, 1, name_len, f);
  cfg->name[name_len+1] = '\0';
  fread(&cfg->w, sizeof(size_t), 7, f);
  fclose(f);
  fprintf(stderr,"config loaded from %s\n", file);
}

char *strupper(const char *s) {
  char *res = strdup(s);
  for(size_t i = 0; s[i] != '\0'; ++i) {
    if(s[i] >= 'a' && s[i] <= 'z') res[i] = s[i] - ('a' - 'A');
  }
  return res;
}

static_assert(LEFT_MARGIN+RIGHT_MARGIN < CHAR_WIDE);
static_assert(TOP_MARGIN+BOTTOM_MARGIN < CHAR_HIGH);
static_assert(ALPHA_THRESH < 256);
#define shift(arr,nmemb) ((nmemb)--,*((arr)++))
#define streq(s1,s2) (strcmp(s1,s2) == 0)

#include <time.h>

int main(int argc, char **argv) {
  srand(time(0));
  int x, y, n;
  struct fontcfg cfg = {
    .w = CHAR_WIDE,
    .h = CHAR_HIGH,
    .margins = {LEFT_MARGIN,RIGHT_MARGIN,TOP_MARGIN,BOTTOM_MARGIN},
    .name = FONT_NAME, 
    .alpha = ALPHA_THRESH,
  };
  const char *program_name = shift(argv, argc);
  char *fname = NULL;
  char *outfile = NULL;
  char *savefile = NULL;
  FILE *ostream = stdout;
  bool close_on_exit = false;
  if(argc > 0) {
    fname = shift(argv, argc);
    if(streq(fname, "help")) {
      fprintf(stderr,
        "Usage: %s your_bitmap_WxH.png [options]\n"
        "  Options\n"
        "   -m <left,right,top,bottom> (NO SPACE) (default 0,0,0,0)\n"
        "      margins to shave off from each character\n"
        "   -o <path> file to write to (default stdout)\n"
        "   -n <name> (default is the bit before '_' in file name)\n"
        "      font name\n"
        "   -a <number> alpha threshold (default 40)\n"
        "   -s <file> save config to file\n"
        "   -c <file> use config from file\n"
      );
      return 0;
    }
    char *tmp = strdup(fname);
    size_t i = 0;
    for( ;*tmp && *tmp != '_'; ++tmp,++i );
    char *font_name = strndup(fname, i);
    while(!isalpha(*font_name)) font_name++;
    cfg.name = font_name;
    char *endptr;
    size_t w = strtoul(++tmp, &endptr, 10);
    assert(w > 0 && endptr != NULL);
    tmp = endptr+1;
    size_t h = strtoul(tmp, &endptr, 10);
    assert(h > 0);
    cfg.w = w;
    cfg.h = h;
    while(argc > 1) { // ml,mr,mt,mb
      char *flag = shift(argv,argc);
      char *val = shift(argv, argc);
      
      if(streq(flag, "-m")) {
        for(i = 0; i < 4; ++i) {
          size_t m = strtoul(val, &endptr, 10);
          cfg.margins[i] = m;
          val = endptr+1;
        }
      }
      if(streq(flag, "-c")) {
        load_cfg(&cfg, val);
      }
      else if(streq(flag, "-o")) {
        ostream = fopen(val, "w+");
        outfile = val;
        assert(ostream != NULL);
        close_on_exit = true;
      }
      else if(streq(flag, "-n")) {
        cfg.name = val;
      }
      else if(streq(flag, "-a")) {
        cfg.alpha = strtoul(val, &endptr, 10);
      }
      else if(streq(flag, "-s")) {
        savefile = val;
      }
    }
  }
  if(savefile) save_cfg(&cfg,savefile);
  dump_cfg(&cfg);
  uint32_t *data =
      (uint32_t *)stbi_load(fname, &x, &y, &n, STBI_rgb_alpha);
  char *font_upper = strupper(cfg.name);
  fprintf(ostream,"#ifndef _%s_H\n", font_upper);
  fprintf(ostream,"#define _%s_H\n", font_upper);
  fprintf(ostream,"#define %s_GLYPH_WIDTH %d\n", font_upper, cfg.w - cfg.margins[0] - cfg.margins[1]);
  fprintf(ostream,"#define %s_GLYPH_HEIGHT %d\n",font_upper, cfg.h - cfg.margins[2] - cfg.margins[3]);
  fprintf(ostream,"static char %s_glyphs[128][%s_GLYPH_HEIGHT][%s_GLYPH_WIDTH] = {\n",cfg.name,font_upper,font_upper);
  size_t nchars = sizeof(chars) - 1;
  assert(x == cfg.w && "width mismatch?");
  assert(y == cfg.h * nchars && "height mismatch?");
  for (size_t i = 0; i < nchars; ++i) {
    if(chars[i] == '\'' || chars[i] == '\\') {
      fprintf(ostream,"\t['\\%c'] = {\n", chars[i]);
    } else {
      fprintf(ostream,"\t['%c'] = {\n", chars[i]);
    }
    for (size_t r = 0; r < cfg.h; ++r) {
      if(r < cfg.margins[2] || r > cfg.h-cfg.margins[3]-1) {
        for(size_t a = 0; a < cfg.w; ++a) *(data++); continue;
      } else *(data+=cfg.margins[0]);
      fprintf(ostream,"\t\t{");
      for (size_t c = 0; c < cfg.w-(cfg.margins[0]+cfg.margins[1]); ++c) {
        uint32_t val = *(data++);
        uint8_t abgr[4] = {
          [3] = val & 0xff,
          [2] = (val >> 8) & 0xff,
          [1] = (val >> 16) & 0xff,
          [0] = (val >> 24) & 0xff,
        };
        bool painted = false;
#ifdef CASE_BY_CASE
        switch(chars[i]) {
          case '*': case '%': painted = abgr[0] > 110; break;
          case 'a': case '~': painted = abgr[0] > 49; break;
          case 's': case '!': painted = abgr[0] > 94; break;
          default: painted = abgr[0] > cfg.alpha; break;
        }
#else
        painted = abgr[0] > cfg.alpha;
#endif //CASE_BY_CASE
        fprintf(ostream,painted
                   ? "1, "
                   : "0, ",val);
      }
      *(data+=cfg.margins[1]);
      fprintf(ostream,"},\n");
    }
    fprintf(ostream,"\t},\n");
  }
  fprintf(ostream,"};\n");
  fprintf(ostream,"static Olivec_Font %s_font = {\n", cfg.name);
  fprintf(ostream,"\t.glyphs = &%s_glyphs[0][0][0],\n", cfg.name);
  fprintf(ostream,"\t.width  = %s_GLYPH_WIDTH,\n", font_upper);
  fprintf(ostream,"\t.height = %s_GLYPH_HEIGHT,\n", font_upper);
  fprintf(ostream,"};\n");
  fprintf(ostream,"#endif // _%s_H\n", font_upper);
done:
  if(close_on_exit) {
    fflush(ostream);
    fclose(ostream);
    printf("wrote font '%s' to %s!\n", cfg.name, outfile);
  }
  return 0;
}
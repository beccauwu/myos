#ifndef _COURIER_H
#define _COURIER_H
#include "courier_15.h"
#include "courier_bold_15.h"

static struct {
  Olivec_Font bold;
  Olivec_Font regular;
} courier = {
  .regular = courier_font,
  .bold = courier_bold_font
};

#endif // _COURIER_H
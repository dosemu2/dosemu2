/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/***********************************************************************
  Set emulation type
 ***********************************************************************/

#include "emulation.h"
#include <stdio.h>
#include <stdlib.h>

int *imap;

/* default instrument mapping */
int imap_default[128] =
{
   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,
  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,
  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,
  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,
  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,
  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,
  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,
  91,  92,  93,  94,  95,  96,  97,  98,  99, 100, 101, 102, 103,
 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116,
 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127
};
/* MT32 -> GM instrument mapping (from playmidi-2.3/emumidi.h) */
int imap_mt2gm[128] =
{
   0,   1,   2,   4,   4,   5,   5,   3,  16,  16,  16,  16,  19,
  19,  19,  21,   6,   6,   6,   7,   7,   7,   8,   8,  62,  57,
  63,  58,  38,  38,  39,  39,  88,  33,  52,  35,  97, 100,  38,
  39,  14, 102,  68, 103,  44,  92,  46,  80,  48,  49,  51,  45,
  40,  40,  42,  42,  43,  46,  46,  24,  25,  28,  27, 104,  32,
  32,  34,  33,  36,  37,  39,  35,  79,  73,  76,  72,  74,  75,
  64,  65,  66,  67,  71,  71,  69,  70,  60,  22,  56,  59,  57,
  63,  60,  60,  58,  61,  61,  11,  11,  99, 100,   9,  14,  13,
  12, 107, 106,  77,  78,  78,  76, 111,  47, 117, 127, 115, 118,
 116, 118, 126, 121, 121,  55, 124, 120, 125, 126, 127
};

/* Sets emulation mode to new_mode */
void emulation_set(Device *dev, Emumode new_mode)
{
  if (dev->setmode(new_mode)) {
    /* Driver can handle out new mode; no need for a new map */
    imap=imap_default;
  } else {
    /* Driver can't handle new mode; find correct mapping */
    switch (new_mode) {
      case EMUMODE_MT32:
        /* We can map this to GM */
	imap=imap_mt2gm;
	break;
      default:
        /* FIXME: GS is not implemented */
	imap=imap_default;
    }
    /* Mapping to GM installed, driver must handle this */
    if (! dev->setmode(EMUMODE_GM)) {
      fprintf(stderr, "Driver %s doesn't support GM mode!\n", dev->name);
      exit(1);
    }
  }
}

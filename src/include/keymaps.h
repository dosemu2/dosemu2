/*
 * include/keymaps.h - This file contains prototypes for all
 *                     implemented keyboard-layouts.
 */

#ifndef _EMU_KEYMAPS_H
#define _EMU_KEYMAPS_H

#include "emu.h"

/* Keyboard-Layout for the alphanumeric part of the keyboard */

#define KEYB_FINNISH           0
#define KEYB_FINNISH_LATIN1    1
#define KEYB_US                2
#define KEYB_UK                3
#define KEYB_DE                4
#define KEYB_DE_LATIN1         5
#define KEYB_FR                6
#define KEYB_FR_LATIN1         7
#define KEYB_DK                8
#define KEYB_DK_LATIN1         9
#define KEYB_DVORAK           10
#define KEYB_SG               11
#define KEYB_SG_LATIN1        12
#define KEYB_NO               13
#define KEYB_SF               15
#define KEYB_SF_LATIN1        16
#define KEYB_ES               17
#define KEYB_ES_LATIN1        18
#define KEYB_BE               19
#define KEYB_PO               20
#define KEYB_IT               21
#define KEYB_SW		      22

#define CONST

extern unsigned char key_map_finnish[];
extern unsigned char shift_map_finnish[];
extern unsigned char alt_map_finnish[];

extern unsigned char key_map_finnish_latin1[];
extern unsigned char shift_map_finnish_latin1[];
extern unsigned char alt_map_finnish_latin1[];

extern unsigned char key_map_us[];
extern unsigned char shift_map_us[];
extern unsigned char alt_map_us[];

extern unsigned char key_map_uk[];
extern unsigned char shift_map_uk[];
extern unsigned char alt_map_uk[];

extern unsigned char key_map_de[];
extern unsigned char shift_map_de[];
extern unsigned char alt_map_de[];

extern unsigned char key_map_de_latin1[];
extern unsigned char shift_map_de_latin1[];
extern unsigned char alt_map_de_latin1[];

extern unsigned char key_map_fr[];
extern unsigned char shift_map_fr[];
extern unsigned char alt_map_fr[];

extern unsigned char key_map_fr_latin1[];
extern unsigned char shift_map_fr_latin1[];
extern unsigned char alt_map_fr_latin1[];

extern unsigned char key_map_dk[];
extern unsigned char shift_map_dk[];
extern unsigned char alt_map_dk[];

extern unsigned char key_map_dk_latin1[];
extern unsigned char shift_map_dk_latin1[];
extern unsigned char alt_map_dk_latin1[];

extern unsigned char key_map_dvorak[];
extern unsigned char shift_map_dvorak[];
extern unsigned char alt_map_dvorak[];

extern unsigned char key_map_sg[];
extern unsigned char shift_map_sg[];
extern unsigned char alt_map_sg[];

extern unsigned char key_map_sg_latin1[];
extern unsigned char shift_map_sg_latin1[];
extern unsigned char alt_map_sg_latin1[];

extern unsigned char key_map_no[];
extern unsigned char shift_map_no[];
extern unsigned char alt_map_no[];

extern unsigned char key_map_no_latin1[];
extern unsigned char shift_map_no_latin1[];
extern unsigned char alt_map_no_latin1[];

extern unsigned char key_map_sf[];
extern unsigned char shift_map_sf[];
extern unsigned char alt_map_sf[];

extern unsigned char key_map_sf_latin1[];
extern unsigned char shift_map_sf_latin1[];
extern unsigned char alt_map_sf_latin1[];

extern unsigned char key_map_es[]; 
extern unsigned char shift_map_es[]; 
extern unsigned char alt_map_es[]; 

extern unsigned char key_map_es_latin1[]; 
extern unsigned char shift_map_es_latin1[]; 
extern unsigned char alt_map_es_latin1[]; 

extern unsigned char key_map_be[];
extern unsigned char shift_map_be[];
extern unsigned char alt_map_be[];

extern unsigned char key_map_po[];
extern unsigned char shift_map_po[];
extern unsigned char alt_map_po[];

extern unsigned char key_map_it[];
extern unsigned char shift_map_it[];
extern unsigned char alt_map_it[];

extern unsigned char key_map_sw[];
extern unsigned char shift_map_sw[];
extern unsigned char alt_map_sw[];

extern unsigned char key_map_hu[];
extern unsigned char shift_map_hu[];
extern unsigned char alt_map_hu[];

extern unsigned char key_map_hu_cwi[];
extern unsigned char shift_map_hu_cwi[];
extern unsigned char alt_map_hu_cwi[];

extern unsigned char key_map_hu_latin2[];
extern unsigned char shift_map_hu_latin2[];
extern unsigned char alt_map_hu_latin2[];

/* Keyboard-Layout for the numeric part of the keyboard */

extern unsigned char num_table_comma[];
extern unsigned char num_table_dot[];

extern struct dos_dead_key dos850_dead_map[];
extern unsigned char dead_key_table[];
 


/*  dead keys for accents */
#define DEAD_GRAVE         1
#define DEAD_ACUTE         2
#define DEAD_CIRCUMFLEX    3
#define DEAD_TILDE         4
#define DEAD_BREVE         5
#define DEAD_ABOVEDOT      6
#define DEAD_DIAERESIS     7
#define DEAD_ABOVERING     8
#define DEAD_DOUBLEACUTE   10
#define DEAD_CEDILLA       11
#define DEAD_IOTA          12

struct dos_dead_key {
  unsigned char d_key;
  unsigned char in_key;
  unsigned char out_key;
};


#endif /* _EMU_KEYMAPS_H */

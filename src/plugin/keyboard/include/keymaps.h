/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

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
#define KEYB_NO_LATIN1        14
#define KEYB_SF               15
#define KEYB_SF_LATIN1        16
#define KEYB_ES               17
#define KEYB_ES_LATIN1        18
#define KEYB_BE               19
#define KEYB_PO               20
#define KEYB_IT               21
#define KEYB_SW		      22
#define KEYB_HU		      23
#define KEYB_HU_CWI	      24
#define KEYB_HU_LATIN2	      25
#define KEYB_USER	      26
#define KEYB_JP106	      27
#define KEYB_PL               28
#define KEYB_HR_CP852	      29
#define KEYB_HR_LATIN2	      30
#define KEYB_CZ_QWERTY        31
#define KEYB_CZ_QWERTZ        32
#define KEYB_AUTO             33
#define KEYB_RU               34
#define CONST


extern unsigned char key_map_us[];
extern unsigned char shift_map_us[];
extern unsigned char alt_map_us[];


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
#define DEAD_OGONEK        14
#define DEAD_CARON         15
#define DEAD_STROKE        16

#define FULL_DEADKEY_LIST  DEAD_GRAVE, DEAD_ACUTE, DEAD_CIRCUMFLEX, \
  DEAD_TILDE, DEAD_BREVE, DEAD_ABOVEDOT, DEAD_DIAERESIS, DEAD_ABOVERING, \
  DEAD_DOUBLEACUTE, DEAD_CEDILLA, DEAD_IOTA, DEAD_OGONEK, DEAD_CARON, DEAD_STROKE

struct dos_dead_key {
  unsigned char d_key;
  unsigned char in_key;
  unsigned char out_key;
};

struct keytable_entry {
  char *name;
  int keyboard;
  int flags;
#define KT_USES_ALTMAP     1
#define KT_ALTERNATE       2
  int sizemap;
  int sizepad;
  unsigned char *key_map, *shift_map, *alt_map, *num_table;
  unsigned char *dead_key_table;
  struct dos_dead_key *dead_map;
};

extern struct keytable_entry keytable_list[];

int setup_default_keytable(void);
void dump_keytable(FILE *f, struct keytable_entry *kt);

#endif /* _EMU_KEYMAPS_H */

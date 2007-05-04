/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * include/keymaps.h - This file contains prototypes for all
 *                     implemented keyboard-layouts.
 */

#ifndef _EMU_KEYMAPS_H
#define _EMU_KEYMAPS_H

#include "config.h"

#include "emu.h"
#include "keyboard.h"

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
#define KEYB_TR               35
#define CONST

#define KT_ALTERNATE	1

struct keytable_entry {
  char *name;
  int keyboard;
  int flags;
  int sizemap;
  int sizepad;
  t_keysym *key_map, *shift_map, *alt_map, *num_table;
  t_keysym *ctrl_map, *shift_alt_map, *ctrl_alt_map;
};

extern struct keytable_entry keytable_list[];

void setup_default_keytable(void);
void dump_keytable(FILE *f, struct keytable_entry *kt);

#endif /* _EMU_KEYMAPS_H */

/* 
 * (C) Copyright 1992, ..., 2002 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/kd.h>
#include <linux/keyboard.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "config.h"
#include "keymaps.h"
#include "keyb_clients.h"
#include "keynum.h"

static int is_a_console(int);
static int open_a_console(char *);
static int getfd(void);
static char *dosemu_val(unsigned, t_keysym *);
static int read_kbd_table(struct keytable_entry *);


/* DANG_BEGIN_MODULE
 * 
 * REMARK
 * These are definitions, giving which key is related to which scancode in
 * raw keyboard mode. Basically, the code of 'x' on a US keyboard may be that
 * of a 'Y' on a German keyboard. This way, all types of keyboard can be
 * represented under DOSEMU. Also, the right ALT-key is often a function
 * key in it's own right.
 *
 * Note:
 * These keymaps have all been converted into dosemu's own internal
 * superset of unicode.
 * Things particular to this superset are:
 * U_VOID is used to represent the lack of a value:
 * The range 0xE000 - 0xEFFF is the unicode private space (so dosemu
 * makes use of it.
 * 0xEF00 - 0xEFFF is used as a pass through to the locally configured
 * character set.
 * While the rest of the range is used for representing symbols that
 * only appear on keyboards and not in alphabets.
 *
 * Note: unicode is also a superset of ascii, and iso8859-1 (i.e. latin1).
 *
 * /REMARK
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * 1999/06/16: Added support for automatic keyboard configuration.
 * Latin-1 keyboards should work. Latin-2 probably not.
 * -- sw (Steffen Winterfeldt <wfeldt@suse.de>)
 *
 * DANG_END_CHANGELOG
 *
 */


CONST t_keysym key_map_finnish[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '+', '\'', 127, 9,
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
  'o', 'p', '}', U_VOID, 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', '|',
  '{', U_VOID, U_VOID, '\'', 'z', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_finnish[] =
{
  U_VOID, 27, '!', '"', '#', '$', '%', '&',
  '/', '(', ')', '=', '?', '`', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
  'O', 'P', ']', '^', 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', '\\',
  '[', U_VOID, U_VOID, '*', 'Z', 'X', 'C', 'V',
  'B', 'N', 'M', ';', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_finnish[] =
{
  U_VOID, U_VOID, U_VOID, '@', 0xefa3, '$', U_VOID, U_VOID,
  '{', '[', ']', '}', '\\', U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, '~', 13, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '|', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_finnish_latin1[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '+', 0xefb4, 127, 9,
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
  'o', 'p', 0xef86, 0xefa8, 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xef94,
  0xef84, 0xefa7, U_VOID, '\'', 'z', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_finnish_latin1[] =
{
  U_VOID, 27, '!', '"', '#', '$', '%', '&',
  '/', '(', ')', '=', '?', '`', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
  'O', 'P', 0xef8f, '^', 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xef99,
  0xef8e, 0xefab, U_VOID, '*', 'Z', 'X', 'C', 'V',
  'B', 'N', 'M', ';', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_finnish_latin1[] =
{
  U_VOID, U_VOID, U_VOID, '@', 0xef9c, '$', U_VOID, U_VOID,
  '{', '[', ']', '}', '\\', U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, '~', 13, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '|', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_us[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '-', '=', 127, 9,
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
  'o', 'p', '[', ']', 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
  '\'', '`', U_VOID, '\\', 'z', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '/', U_VOID, '*',
  U_VOID, ' ', U_VOID, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
  KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', KEY_F11,
  KEY_F12, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_us[] =
{
  U_VOID, 27, '!', '@', '#', '$', '%', '^',
  '&', '*', '(', ')', '_', '+', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
  'O', 'P', '{', '}', 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
  '"', '~', U_VOID, '|', 'Z', 'X', 'C', 'V',
  'B', 'N', 'M', '<', '>', '?', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_us[] =
{
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym ctrl_map_us[] =
{
  U_VOID, 0x1b,   U_VOID, 0x00,   U_VOID, U_VOID, U_VOID, 0x1e,
  U_VOID, U_VOID, U_VOID, U_VOID, 0x1f,   U_VOID, 0x7f,   U_VOID,
  0x11,   0x17,   0x05,   0x12,   0x14,   0x19,   0x15,   0x09,
  0x0f,   0x10,   0x1b,   0x1d,   0x0a,   U_VOID, 0x01,   0x13,
  0x04,   0x06,   0x07,   0x08,   0x0a,   0x0b,   0x0c,   U_VOID,
  U_VOID, U_VOID, U_VOID, 0x1c,   0x1a,   0x18,   0x03,   0x16,
  0x02,   0x0e,   0x0d,   U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, 0x20,   U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_uk[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '-', '=', 127, 9,
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
  'o', 'p', '[', ']', 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
  '\'', '`', U_VOID, '#', 'z', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '/', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '\\', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_uk[] =
{
  U_VOID, 27, '!', '"', 0xef9c, '$', '%', '^',
  '&', '*', '(', ')', '_', '+', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
  'O', 'P', '{', '}', 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
  '@', '~', '0', '~', 'Z', 'X', 'C', 'V',
  'B', 'N', 'M', '<', '>', '?', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '|', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_uk[] =
{
  U_VOID, U_VOID, U_VOID, '@', U_VOID, '$', U_VOID, U_VOID,
  '{', '[', ']', '}', '\\', U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, '~', 13, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  KEY_DEAD_ACUTE, KEY_DEAD_GRAVE, U_VOID, KEY_DEAD_TILDE, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '|', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

/* Watch this keymap carefully.
 * It produces a keymap that is virtually identical to that produces by keyb.exe
 * in for both character sets cp437 & cp850.
 * Further this keymap has been converted into unicode, the first real conversion.
 * To get exactly the same translations as keyb.exe for two characters
 * sets cp437 & cp850. The code keysym_approximations is relied upon to do the
 * right thing.
 * --EB 11 Feb 2000
 */
CONST t_keysym key_map_de[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', 0x00df, 0x00b4, 127, 9,
  'q', 'w', 'e', 'r', 't', 'z', 'u', 'i',
  'o', 'p', 0x00fc, '+', 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0x00f6,
  0x00e4, '^', U_VOID, '#', 'y', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_de[] =
{
  U_VOID, 27, '!', '"', 0x00a7, '$', '%', '&',
  '/', '(', ')', '=', '?', '`', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I',
  'O', 'P', 0x00dc, '*', 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 0x00d6,
  0x00c4, 0x00b0, U_VOID, '\'', 'Y', 'X', 'C', 'V',
  'B', 'N', 'M', ';', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_de[] =
{
  U_VOID, U_VOID, U_VOID, 0x00b2, 0x207f, U_VOID, U_VOID, U_VOID,
  '{', '[', ']', '}', '\\', U_VOID, U_VOID, U_VOID,
  '@', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, '~', U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, 0x00b5, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '|', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym ctrl_map_de[] =
{
  U_VOID, 0x1b,   U_VOID, 0x00,   U_VOID, U_VOID, U_VOID, 0x1e,
  U_VOID, U_VOID, U_VOID, U_VOID, 0x1c,   U_VOID, 0x7f,   U_VOID,
  0x11,   0x17,   0x05,   0x12,   0x14,   0x1a,   0x15,   0x09,
  0x0f,   0x10,   0x1b,   0x1d,   0x0a,   U_VOID, 0x01,   0x13,
  0x04,   0x06,   0x07,   0x08,   0x0a,   0x0b,   0x0c,   U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, 0x19,   0x18,   0x03,   0x16,
  0x02,   0x0e,   0x0d,   U_VOID, U_VOID, 0x1f,   U_VOID, U_VOID,
  U_VOID, 0x20,   U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_de_latin1[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', 0xefe1, KEY_DEAD_ACUTE, 127, 9,
  'q', 'w', 'e', 'r', 't', 'z', 'u', 'i',
  'o', 'p', 0xef81, '+', 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xef94,
  0xef84, KEY_DEAD_CIRCUMFLEX, U_VOID, '#', 'y', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_de_latin1[] =
{
  U_VOID, 27, '!', '"', 21, '$', '%', '&',
  '/', '(', ')', '=', '?', KEY_DEAD_GRAVE, 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I',
  'O', 'P', 0xef9a, '*', 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xef99,
  0xef8e, 0xeff8, U_VOID, '\'', 'Y', 'X', 'C', 'V',
  'B', 'N', 'M', ';', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_de_latin1[] =
{
  U_VOID, U_VOID, U_VOID, 0xeffd, 0xeffc, '$', U_VOID, U_VOID,
  '{', '[', ']', '}', '\\', U_VOID, U_VOID, U_VOID,
  '@', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, '~', 13, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, 0xefe6, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '|', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_fr[] =
{
  U_VOID, 27, '&', '{', '"', '\'', '(', '-',
  '}', '_', '/', '@', ')', '=', 127, 9,
  'a', 'z', 'e', 'r', 't', 'y', 'u', 'i',
  'o', 'p', '^', '$', 13, U_VOID, 'q', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 'm',
  '|', '`', U_VOID, '*', 'w', 'x', 'c', 'v',
  'b', 'n', ',', ';', ':', '!', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_fr[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', ']', '+', 127, 9,
  'A', 'Z', 'E', 'R', 'T', 'Y', 'U', 'I',
  'O', 'P', '<', '>', 13, U_VOID, 'Q', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 'M',
  '%', '~', U_VOID, '#', 'W', 'X', 'C', 'V',
  'B', 'N', '?', '.', '/', '\\', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_fr[] =
{
  U_VOID, U_VOID, U_VOID, '~', '#', '{', '[', '|',
  '`', '\\', '^', '@', ']', '}', U_VOID, U_VOID,
  '@', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, '~', 13, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '|', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_fr_latin1[] =
{
  U_VOID, 27, '&', 0xef82, '"', '\'', '(', '-',
  0xef8a, '_', 0xef87, 0xef85, ')', '=', 127, 9,
  'a', 'z', 'e', 'r', 't', 'y', 'u', 'i',
  'o', 'p', KEY_DEAD_CIRCUMFLEX, '$', 13, U_VOID, 'q', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 'm',
  0xef97, 0xeffd, U_VOID, '*', 'w', 'x', 'c', 'v',
  'b', 'n', ',', ';', ':', '!', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_fr_latin1[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', 0xeff8, '+', 127, 9,
  'A', 'Z', 'E', 'R', 'T', 'Y', 'U', 'I',
  'O', 'P', KEY_DEAD_DIAERESIS, 0xef9c, 13, U_VOID, 'Q', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 'M',
  '%', '~', U_VOID, 0xefe6, 'W', 'X', 'C', 'V',
  'B', 'N', '?', '.', '/', 0xefa7, U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_fr_latin1[] =
{
  U_VOID, U_VOID, U_VOID, '~', '#', '{', '[', '|',
  '`', '\\', '^', '@', ']', '}', U_VOID, U_VOID,
  '@', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, 0xefa4, 13, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '|', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_dk[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '+', '\'', 127, 9,
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
  'o', 'p', 0xef86, U_VOID, 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xef91,
  0xef9b, U_VOID, U_VOID, '\'', 'z', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};
 
CONST t_keysym shift_map_dk[] =
{
  U_VOID, 27, '!', '"', '#', '$', '%', '&',
  '/', '(', ')', '=', '?', '`', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
  'O', 'P', 0xef8f, '^', 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xef92,
  0xef9d, U_VOID, U_VOID, '*', 'Z', 'X', 'C', 'V',
  'B', 'N', 'M', ';', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_dk[] =
{
  U_VOID, U_VOID, U_VOID, '@', 0xefa3, '$', U_VOID, U_VOID,
  '{', '[', ']', '}', U_VOID, '|', U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, '~', 13, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '\\', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_dk_latin1[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '+', 0xefb4, 127, 9,
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
  'o', 'p', 0xefe5, 0xefa8, 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xefe6,
  0xefa2, 0xefbd, U_VOID, '\'', 'z', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_dk_latin1[] =
{
  U_VOID, 27, '!', '"', '#', '$', '%', '&',
  '/', '(', ')', '=', '?', '`', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
  'O', 'P', 0xefc5, '^', 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xefc6,
  0xefa5, 0xefa7, U_VOID, '*', 'Z', 'X', 'C', 'V',
  'B', 'N', 'M', ';', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_dk_latin1[] =
{
  U_VOID, U_VOID, U_VOID, '@', 0xefa3, '$', U_VOID, U_VOID,
  '{', '[', ']', '}', U_VOID, '|', U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, '~', 13, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '\\', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_dvorak[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '\\', '=', 127, 9,
  '\'', ',', '.', 'p', 'y', 'f', 'g', 'c',
  'r', 'l', '/', ']', 13, U_VOID, 'a', 'o',
  'e', 'u', 'i', 'd', 'h', 't', 'n', 's',
  '-', '`', U_VOID, '[', ';', 'q', 'j', 'k',
  'x', 'b', 'm', 'w', 'v', 'z', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_dvorak[] =
{
  U_VOID, 27, '!', '@', '#', '$', '%', '^',
  '&', '*', '(', ')', '|', '+', 127, 9,
  '"', '<', '>', 'P', 'Y', 'F', 'G', 'C',
  'R', 'L', '?', '}', 13, U_VOID, 'A', 'O',
  'E', 'U', 'I', 'D', 'H', 'T', 'N', 'S',
  '_', '~', U_VOID, '{', ':', 'Q', 'J', 'K',
  'X', 'B', 'M', 'W', 'V', 'Z', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_dvorak[] =
{
  U_VOID, U_VOID, U_VOID, '@', U_VOID, '$', U_VOID, U_VOID,
  '{', '[', ']', '}', '\\', U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, '~', 13, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '|', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_sg[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '\'', '^', 127, 9,
  'q', 'w', 'e', 'r', 't', 'z', 'u', 'i',
  'o', 'p', U_VOID, U_VOID, 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', U_VOID,
  U_VOID, U_VOID, U_VOID, '$', 'y', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_sg[] =
{
  U_VOID, 27, '+', '"', '*', U_VOID, '%', '&',
  '/', '(', ')', '=', '?', '`', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I',
  'O', 'P', U_VOID, '!', 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, 'Y', 'X', 'C', 'V',
  'B', 'N', 'M', ';', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_sg[] =
{
  U_VOID, U_VOID, U_VOID, '@', '#', U_VOID, U_VOID, U_VOID,
  '|', U_VOID, U_VOID, U_VOID, '\'', '~', U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '[', ']', 13, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  '{', U_VOID, U_VOID, '}', U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '\\', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_sg_latin1[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '\'', '^', 127, 9,
  'q', 'w', 'e', 'r', 't', 'z', 'u', 'i',
  'o', 'p', 0xefc0, U_VOID, 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xef94,
  0xef84, 0xefa7, U_VOID, '$', 'y', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_sg_latin1[] =
{
  U_VOID, 27, '+', '"', '*', 0xef80, '%', '&',
  '/', '(', ')', '=', '?', '`', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I',
  'O', 'P', 0xef9a, '!', 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xef99,
  0xef8e, 0xefb0, U_VOID, 0xefa3, 'Y', 'X', 'C', 'V',
  'B', 'N', 'M', ';', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_sg_latin1[] =
{
  U_VOID, U_VOID, 0xefb3, '@', '#', U_VOID, U_VOID, 0xefaa,
  '|', 0xefa2, U_VOID, U_VOID, '\'', '~', U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '[', ']', 13, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, 0xefe9,
  '{', U_VOID, U_VOID, '}', U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '\\', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_no[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '+', '\\', 127, 9,
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
  'o', 'p', '}', '~', 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', '|',
  '{', '|', U_VOID, '\'', 'z', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_no[] =
{
  U_VOID, 27, '!', '"', '#', '$', '%', '&',
  '/', '(', ')', '=', '?', '`', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
  'O', 'P', ']', '^', 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', '\\',
  '[', U_VOID, U_VOID, '*', 'Z', 'X', 'C', 'V',
  'B', 'N', 'M', ';', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_no[] =
{
  U_VOID, U_VOID, U_VOID, '@', U_VOID, '$', U_VOID, U_VOID,
  '{', '[', ']', '}', U_VOID, '\'', U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, '~', 13, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_no_latin1[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '+', '\\', 127, 9,
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
  'o', 'p', 0xef86, KEY_DEAD_DIAERESIS, 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xef9b,
  0xef91, '|', U_VOID, '\'', 'z', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_no_latin1[] =
{
  U_VOID, 27, '!', '"', '#', '$', '%', '&',
  '/', '(', ')', '=', '?', KEY_DEAD_GRAVE, 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
  'O', 'P', 0xef8f, KEY_DEAD_CIRCUMFLEX, 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xef9d,
  0xef92, 0xeff5, U_VOID, '*', 'Z', 'X', 'C', 'V',
  'B', 'N', 'M', ';', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_no_latin1[] =
{
  U_VOID, U_VOID, U_VOID, '@', 0xef9c, 0xefcf, U_VOID, U_VOID,
  '{', '[', ']', '}', U_VOID, KEY_DEAD_ACUTE, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, KEY_DEAD_TILDE, 13, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_sf[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '\'', '^', 127, 9,
  'q', 'w', 'e', 'r', 't', 'z', 'u', 'i',
  'o', 'p', U_VOID, U_VOID, 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', U_VOID,
  U_VOID, U_VOID, U_VOID, '$', 'y', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_sf[] =
{
  U_VOID, 27, '+', '"', '*', U_VOID, '%', '&',
  '/', '(', ')', '=', '?', '`', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I',
  'O', 'P', U_VOID, '!', 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, 'Y', 'X', 'C', 'V',
  'B', 'N', 'M', ';', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_sf[] =
{
  U_VOID, U_VOID, U_VOID, '@', '#', U_VOID, U_VOID, U_VOID,
  '|', U_VOID, U_VOID, U_VOID, '\'', '~', U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '[', ']', 13, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  '{', U_VOID, U_VOID, '}', U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '\\', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_sf_latin1[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '\'', '^', 127, 9,
  'q', 'w', 'e', 'r', 't', 'z', 'u', 'i',
  'o', 'p', 0xefe8, 0xefa8, 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xefe9,
  0xefe0, 0xefa7, U_VOID, '$', 'y', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_sf_latin1[] =
{
  U_VOID, 27, '+', '"', '*', 0xefe7, '%', '&',
  '/', '(', ')', '=', '?', '`', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I',
  'O', 'P', 0xeffc, '!', 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xeff6,
  0xefe4, 0xefb0, U_VOID, 0xefa3, 'Y', 'X', 'C', 'V',
  'B', 'N', 'M', ';', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_sf_latin1[] =
{
  U_VOID, U_VOID, U_VOID, '@', '#', U_VOID, U_VOID, 0xefac,
  '|', 0xefa2, U_VOID, U_VOID, 0xefb4, '~', U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '[', ']', 13, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  '{', U_VOID, U_VOID, '}', U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '\\', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_es[] = {
 U_VOID,     27,    '1',    '2',    '3',    '4',    '5',    '6',
    '7',    '8',    '9',    '0',   '\'', U_VOID,    127,      9,
    'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
    'o',    'p',    '`',    '+',     13, U_VOID,    'a',    's',
    'd',    'f',    'g',    'h',    'j',    'k',    'l', U_VOID,
   '\'',    '`', U_VOID, U_VOID,    'z',    'x',    'c',    'v',
    'b',    'n',    'm',    ',',    '.',    '-', U_VOID,    '*',
 U_VOID,    ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID,    '-', U_VOID, U_VOID, U_VOID,    '+', U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,    '<', U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID
};

CONST t_keysym shift_map_es[] = {
 U_VOID,     27,    '!',    '"',    '#',    '$',    '%',    '&',
    '/',    '(',    ')',    '=',    '?', U_VOID,    127,      9,
    'Q',    'W',    'E',    'R',    'T',    'Y',    'U',    'I',
    'O',    'P',    '^',    '*',     13, U_VOID,    'A',    'S',
    'D',    'F',    'G',    'H',    'J',    'K',    'L', U_VOID,
 U_VOID,    '~', U_VOID, U_VOID,    'Z',    'X',    'C',    'V',
    'B',    'N',    'M',    ';',    ':',    '_', U_VOID,    '*',
 U_VOID,    ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID,    '-', U_VOID, U_VOID, U_VOID,    '+', U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,    '>', U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID
};

CONST t_keysym alt_map_es[] = {
 U_VOID, U_VOID,    '|',    '@',    '#',    '$', U_VOID, U_VOID,
    '{',    '[',    ']',    '}',   '\\',    '~', U_VOID, U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID,    '[',    ']',     13, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
    '{',   '\\', U_VOID,    '}', U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID,    '~', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,    '|', U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID
};

CONST t_keysym key_map_es_latin1[] = {
 U_VOID,     27,    '1',    '2',    '3',    '4',    '5',    '6',
    '7',    '8',    '9',    '0',    '\'', 0xefad,   127,      9,
    'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
    'o',    'p', KEY_DEAD_GRAVE,    '+',     13, U_VOID,    'a',    's',
    'd',    'f',    'g',    'h',    'j',    'k',    'l', 0xefa4,
 KEY_DEAD_ACUTE, 0xefa7, U_VOID, 0xef87,    'z',    'x',    'c',    'v',
    'b',    'n',    'm',    ',',    '.',    '-', U_VOID,    '*',
 U_VOID,    ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID,    '-', U_VOID, U_VOID, U_VOID,    '+', U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,    '<', U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID
};

CONST t_keysym shift_map_es_latin1[] = {
 U_VOID,     27,    '!',    '"', 0xeffa,    '$',    '%',    '&',
    '/',    '(',    ')',    '=',    '?', 0xefa8,    127,      9,
    'Q',    'W',    'E',    'R',    'T',    'Y',    'U',    'I',
    'O',    'P',    KEY_DEAD_CIRCUMFLEX,    '*',     13, U_VOID,    'A',    'S',
    'D',    'F',    'G',    'H',    'J',    'K',    'L', 0xefa5,
     KEY_DEAD_DIAERESIS, 0xefa6, U_VOID, 0xef80,    'Z',    'X',    'C',    'V',
    'B',    'N',    'M',    ';',    ':',    '_', U_VOID,    '*',
 U_VOID,    ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID,    '-', U_VOID, U_VOID, U_VOID,    '+', U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,    '>', U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID
};

CONST t_keysym alt_map_es_latin1[] = {
 U_VOID, U_VOID,    '|',    '@',    '#',    '$', U_VOID, 0xefac,
    '{',    '[',    ']',    '}',   '\\',    '~', U_VOID, U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID,    '[',    ']',     13, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
    '{',   '\\', U_VOID,    '}', U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID,    '~', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,    '|', U_VOID,
 U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
 U_VOID
};

/* keyboard driver for BELGIAN KEYBOARD */

CONST t_keysym key_map_be[] =
{
  U_VOID, 27, '&', 0xef82, '"', '\'', '(', 21,
  0xef8a, '!', 0xef80, 0xef85, ')', '-', 127, 9,
  'a', 'z', 'e', 'r', 't', 'y', 'u', 'i',
  'o', 'p', KEY_DEAD_CIRCUMFLEX, '$', 13, U_VOID, 'q', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 'm',
  0xef97, 0xeffd, U_VOID, 0xefe6, 'w', 'x', 'c', 'v',
  'b', 'n', ',', ';', ':', '=', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_be[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', 0xeff8, '_', 127, 9,
  'A', 'Z', 'E', 'R', 'T', 'Y', 'U', 'I',
  'O', 'P', KEY_DEAD_DIAERESIS, '*', 13, U_VOID, 'Q', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 'M',
  '%', 0xeffc, U_VOID, 0xef9c, 'W', 'X', 'C', 'V',
  'B', 'N', '?', '.', '/', '+', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_be[] =
{
  U_VOID, U_VOID, '|', '@', '#', U_VOID, U_VOID, '^',
  U_VOID, U_VOID, '{', '}', KEY_DEAD_ABOVERING, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '[', ']', 13, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  KEY_DEAD_ACUTE, KEY_DEAD_GRAVE, U_VOID, KEY_DEAD_GRAVE, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, KEY_DEAD_CEDILLA, U_VOID, U_VOID, KEY_DEAD_TILDE, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '\\', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_po[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '`', 0xefae, 127, 9,
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
  'o', 'p', '+', KEY_DEAD_ACUTE, 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xef87,
  0xefa7, '\\', U_VOID, KEY_DEAD_TILDE, 'z', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_po[] =
{
  U_VOID, 27, '!', '"', '#', '$', '%', '&',
  '/', '(', ')', '=', '?', 0xefaf, 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
  'O', 'P', '*', KEY_DEAD_GRAVE, 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xef80,
  0xefa6, '|', '0', KEY_DEAD_CIRCUMFLEX, 'Z', 'X', 'C', 'V',
  'B', 'N', 'M', ';', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_po[] =
{
  U_VOID, U_VOID, U_VOID, '@', 0xef9c, 21, U_VOID, U_VOID,
  '{', '[', ']', '}', U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, KEY_DEAD_DIAERESIS, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_it[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '\'', 0xef8d, 127, 9,
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
  'o', 'p', 0xef8a, '+', 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xef95,
  0xef85, '\\', U_VOID, 0xef97, 'z', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_it[] =
{
  U_VOID, 27, '!', '"', 0xef9c, '$', '%', '&',
  '/', '(', ')', '=', '?', '^', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
  'O', 'P', 0xef82, '*', 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xef87,
  0xeff8, '|', '0', 21, 'Z', 'X', 'C', 'V',
  'B', 'N', 'M', ';', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_it[] =
{
  U_VOID, U_VOID, U_VOID, '@', U_VOID, '$', U_VOID, U_VOID,
  '{', '[', ']', '}', '{', '}', U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '[', ']', 13, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '@',
  '#', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '|', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_sw[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '+', '\'', 127, 9,
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
  'o', 'p', 0xef86, '~', 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xef94,
  0xef84, 21, U_VOID, '\'', 'z', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_sw[] =
{
  0xefab, U_VOID, '!', '"', '#', '$', '%', '&',
  '/', '(', ')', '=', '?', '`', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
  'O', 'P', 0xef8f, '^', 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xef99,
  0xef8e, 0xefab, U_VOID, '*', 'Z', 'X', 'C', 'V',
  'B', 'N', 'M', ';', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_sw[] =
{
   U_VOID, U_VOID, U_VOID, '@', 0xef9c, '$', U_VOID, U_VOID,
  '{', '[', ']', '}', '\\', U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, '~', 13, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '|', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_hu[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', 0xef94, 0xef81, 0xefa2, 127, 9,
  'q', 'w', 'e', 'r', 't', 'z', 'u', 'i',
  'o', 'p', 0xef8b, 0xefa3, 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xef82,
  0xefa0, '0', U_VOID, 0xeffb, 'y', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, 0xefa1, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_hu[] =
{
  U_VOID, 27, '\'', '"', '+', '!', '%', '/',
  '=', '(', ')', 0xef99, 0xef9a, '\'', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I',
  'O', 'P', 0xef8a, 0xefe9, 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xef90,
  0xefb5, 21, '0', 0xefeb, 'Y', 'X', 'C', 'V',
  'B', 'N', 'M', '?', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, 0xef92, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_hu[] =
{
  U_VOID, U_VOID, '~', KEY_DEAD_CARON, KEY_DEAD_CIRCUMFLEX, KEY_DEAD_BREVE, 0xeff8, KEY_DEAD_OGONEK,
  KEY_DEAD_GRAVE, KEY_DEAD_ABOVEDOT, KEY_DEAD_ACUTE, KEY_DEAD_DOUBLEACUTE, KEY_DEAD_DIAERESIS, KEY_DEAD_CEDILLA, U_VOID, U_VOID,
  '\\', '|', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, 0xef92,
  U_VOID, U_VOID, 0xeff6, 0xef9e, 13, U_VOID, U_VOID, 0xefd0,
  0xefd1, '[', ']', U_VOID, 0xefa1, 0xef88, 0xef9d, '$',
  0xefe1, U_VOID, U_VOID, 0xefcf, '>', '#', '&', '@',
  '{', '}', U_VOID, ';', U_VOID, '*', U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_hu_cwi[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', 0xef94, 0xef81, 0xefa2, 127, 9,
  'q', 'w', 'e', 'r', 't', 'z', 'u', 'i',
  'o', 'p', 0xef93, 0xefa3, 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xef82,
  0xefa0, '0', U_VOID, 0xef96, 'y', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, 0xefa1, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_hu_cwi[] =
{
  U_VOID, 27, '\'', '"', '+', '!', '%', '/',
  '=', '(', ')', 0xef99, 0xef9a, 0xef95, 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I',
  'O', 'P', 0xefa7, 0xef97, 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xef90,
  0xef8f, 21, '0', 0xef98, 'Y', 'X', 'C', 'V',
  'B', 'N', 'M', '?', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, 0xef8d, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_hu_cwi[] =
{
  U_VOID, U_VOID, '~', KEY_DEAD_CARON, KEY_DEAD_CIRCUMFLEX, KEY_DEAD_BREVE, 0xeff8, KEY_DEAD_OGONEK,
  KEY_DEAD_GRAVE, KEY_DEAD_ABOVEDOT, KEY_DEAD_ACUTE, KEY_DEAD_DOUBLEACUTE, KEY_DEAD_DIAERESIS, KEY_DEAD_CEDILLA, U_VOID, U_VOID,
  '\\', '|', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, 0xef8d,
  U_VOID, U_VOID, 0xeff6, U_VOID, 13, U_VOID, U_VOID, U_VOID,
  U_VOID, '[', ']', U_VOID, 0xefa1, U_VOID, U_VOID, '$',
  0xefe1, U_VOID, U_VOID, U_VOID, '>', '#', '&', '@',
  '{', '}', U_VOID, ';', U_VOID, '*', U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_hu_latin2[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', 0xeff6, 0xeffc, 0xeff3, 127, 9,
  'q', 'w', 'e', 'r', 't', 'z', 'u', 'i',
  'o', 'p', 0xeff5, 0xeffa, 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xefe9,
  0xefe1, '0', U_VOID, 0xeffb, 'y', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, 0xefed, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_hu_latin2[] =
{
  U_VOID, 27, '\'', '"', '+', '!', '%', '/',
  '=', '(', ')', 0xefd6, 0xefdc, 0xefd3, 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I',
  'O', 'P', 0xefd5, 0xefda, 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xefc9,
  0xefc1, 21, '0', 0xefdb, 'Y', 'X', 'C', 'V',
  'B', 'N', 'M', '?', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, 0xefcd, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_hu_latin2[] =
{
  U_VOID, U_VOID, '~', KEY_DEAD_CARON, KEY_DEAD_CIRCUMFLEX, KEY_DEAD_BREVE, 0xeff8, KEY_DEAD_OGONEK,
  KEY_DEAD_GRAVE, KEY_DEAD_ABOVEDOT, KEY_DEAD_ACUTE, KEY_DEAD_DOUBLEACUTE, KEY_DEAD_DIAERESIS, KEY_DEAD_CEDILLA, U_VOID, U_VOID,
  '\\', '|', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, 0xefcd,
  U_VOID, U_VOID, 0xeff7, 0xefd7, 13, U_VOID, U_VOID, 0xeff0,
  0xefd0, '[', ']', U_VOID, 0xefed, 0xefb3, 0xefa3, '$',
  0xefdf, U_VOID, U_VOID, 0xefa4, '>', '#', '&', '@',
  '{', '}', U_VOID, ';', U_VOID, '*', U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_jp106[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '-', '^', 127, 9,
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
  'o', 'p', '@', '[', 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
  ':', '`', U_VOID, ']', 'z', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '/', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '0', U_VOID,
  U_VOID, '\\', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, '\\', U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '\\', U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_jp106[] =
{
  U_VOID, 27, '!', '"', '#', '$', '%', '&',
  '\'', '(', ')', '~', '=', '~', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
  'O', 'P', '`', '{', KEY_DEAD_GRAVE, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', '+',
  '*', '~', '0', '}', 'Z', 'X', 'C', 'V',
  'B', 'N', 'M', '<', '>', '?', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '0', U_VOID,
  U_VOID, '_', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, '_', U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '|', U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_jp106[] =
{
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};


/* Polish keyboard */
CONST t_keysym key_map_pl[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '+', '\'', 127, 9,
  'q', 'w', 'e', 'r', 't', 'z', 'u', 'i',
  'o', 'p', 0xefbe, 0xef98, 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xef88,
  0xefa5, KEY_DEAD_ABOVEDOT, U_VOID, 0xefa2, 'y', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_pl[] =
{
  U_VOID, 27, '!', '"', '#', '$', '%', '&',
  '/', '(', ')', '=', '?', '*', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I',
  'O', 'P', 0xefe4, 0xef86, 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xef9d,
  0xefa9, KEY_DEAD_OGONEK, '0', 0xefab, 'Y', 'X', 'C', 'V',
  'B', 'N', 'M', ';', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_pl[] =
{
  U_VOID, U_VOID, '~', KEY_DEAD_CARON, KEY_DEAD_CIRCUMFLEX, KEY_DEAD_BREVE, 0xeff8, KEY_DEAD_OGONEK,
  KEY_DEAD_GRAVE, KEY_DEAD_ABOVEDOT, KEY_DEAD_ACUTE, KEY_DEAD_DOUBLEACUTE, KEY_DEAD_DIAERESIS, KEY_DEAD_CEDILLA, U_VOID, U_VOID,
  '\\', '|', 0xefa9, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  0xefa2, U_VOID, 0xeff6, 0xef9e, U_VOID, U_VOID, 0xefa5, 0xefd0,
  0xefd1, '[', ']', U_VOID, U_VOID, U_VOID, 0xef88, '$',
  0xefe1, U_VOID, U_VOID, KEY_DEAD_CARON, 0xefbe, 0xefab, 0xef86, '@',
  '{', '}', 21, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '|', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_hr_cp852[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '\'', '+', 127, 9,
  'q', 'w', 'e', 'r', 't', 'z', 'u', 'i',
  'o', 'p', 0xefe7, 0xefd0, 13, U_VOID, 'a', 's',      /*231=s, 208=d*/
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xef9f,           /*159=c tvrdo*/
  0xef86, U_VOID, U_VOID, 0xefa7, 'y', 'x', 'c', 'v',  /*134=c meko,167=z*/
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_hr_cp852[] =
{
  U_VOID, 27, '!', '"', '#', '$', '%', '&',
  '/', '(', ')', '=', '?', '*', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I',
  'O', 'P', 0xefe6, 0xefd1, 13, U_VOID, 'A', 'S',       /*230=S, 209=D*/
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xefac,            /*172=C tvrdo*/
  0xef8f, U_VOID, U_VOID, 0xefa6, 'Y', 'X', 'C', 'V',  /*143=C meko,166=Z*/
  'B', 'N', 'M', ';', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_hr_cp852[] =
{
  U_VOID, U_VOID, '~', 0xefb7, '^', 0xeff4, 0xeff8, 0xeff2,
  '`', 0xeffa, 0xefef, 0xeff1, 0xeff9, 0xeff7, U_VOID, U_VOID,
  '\\', '|', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, 0xeff6, 0xef9e, 13, U_VOID, U_VOID, U_VOID,
  U_VOID, '[', ']', U_VOID, U_VOID, 0xef92, 0xef91, '\\',
  0xefe1, U_VOID, U_VOID, 0xefcf, U_VOID, U_VOID, U_VOID, '@',
  '{', '}', 0xeff5, U_VOID, '|', '/', U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '|', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym key_map_hr_latin2[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '\'', '+', 127, 9,
  'q', 'w', 'e', 'r', 't', 'z', 'u', 'i',
  'o', 'p', 0xefb9, 0xeff0, 13, U_VOID, 'a', 's',     /*185=s, 240=d*/
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xefe8,          /*232=c tvrdo*/
  0xefe6, U_VOID, U_VOID, 0xefbe, 'y', 'x', 'c', 'v', /*230=c meko,190=z*/
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_hr_latin2[] =
{
  U_VOID, 27, '!', '"', '#', '$', '%', '&',
  '/', '(', ')', '=', '?', '*', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I',
  'O', 'P', 0xefa9, 0xefd0, 13, U_VOID, 'A', 'S',      /*169=S, 208=D*/
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xefc8,           /*200=C tvrdo*/
  0xefc6, U_VOID, U_VOID, 0xefae, 'Y', 'X', 'C', 'V',  /*198=C meko,174=Z*/
  'B', 'N', 'M', ';', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_hr_latin2[] =
{
  U_VOID, U_VOID, '~', 0xefb7, '^', 0xefa2, 0xefb0, 0xefb2,
  '`', 0xefff, 0xefb4, 0xefbd, 0xefa8, 0xefb8, U_VOID, U_VOID,
  '\\', '|', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, 0xeff7, 0xefd7, 13, U_VOID, U_VOID, U_VOID,
  U_VOID, '[', ']', U_VOID, U_VOID, 0xefb3, 0xefa3, '\\',
  0xefdf, U_VOID, U_VOID, 0xefa4, U_VOID, U_VOID, U_VOID, '@',
  '{', '}', 0xefa7, U_VOID, '|', '/', U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '|', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};


/* Czech keyboard QWERTY */
CONST t_keysym key_map_cz_qwerty[] =
{
  U_VOID, 27, '+', 0xefd8, 0xefe7, 0xef9f, 0xeffd, 0xefa7,
  0xefec, 0xefa0, 0xefa1, 0xef82, '=', KEY_DEAD_ACUTE, 127, 9,
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
  'o', 'p', 0xefa3, ')', 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xef85,
  0xeff5, ';', U_VOID, KEY_DEAD_DIAERESIS, 'z', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '&', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_cz_qwerty[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '%', KEY_DEAD_CARON, 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
  'O', 'P', '/', '(', 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', '"',
  '!', 0xeff8, U_VOID, '\'', 'Z', 'X', 'C', 'V',
  'B', 'N', 'M', '?', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '*', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_cz_qwerty[] =
{
  U_VOID, U_VOID, '~', KEY_DEAD_CARON, KEY_DEAD_CIRCUMFLEX, KEY_DEAD_BREVE, 0xeff8, KEY_DEAD_OGONEK,
  KEY_DEAD_GRAVE, KEY_DEAD_ABOVEDOT, KEY_DEAD_ACUTE, KEY_DEAD_DOUBLEACUTE, KEY_DEAD_DIAERESIS, KEY_DEAD_CEDILLA, U_VOID, U_VOID,
  '\\', '|', 0xefa9, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  0xefa2, U_VOID, 0xeff6, 0xef9e, U_VOID, U_VOID, 0xefa5, 0xefd0,
  0xefd1, '[', ']', U_VOID, U_VOID, 0xef88, 0xef9d, '$',
  0xefe1, U_VOID, U_VOID, 0xefcf, '>', '#', 0xef86, '@',
  '{', '}', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

/* Czech keyboard QWERTZ */
CONST t_keysym key_map_cz_qwertz[] =
{
  U_VOID, 27, '+', 0xefd8, 0xefe7, 0xef9f, 0xeffd, 0xefa7,
  0xefec, 0xefa0, 0xefa1, 0xef82, '=', KEY_DEAD_ACUTE, 127, 9,
  'q', 'w', 'e', 'r', 't', 'z', 'u', 'i',
  'o', 'p', 0xefa3, ')', 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xef85,
  0xeff5, ';', U_VOID, KEY_DEAD_DIAERESIS, 'y', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '-', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '&', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_cz_qwertz[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '%', KEY_DEAD_CARON, 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I',
  'O', 'P', '/', '(', 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', '"',
  '!', 0xeff8, U_VOID, '\'', 'Y', 'X', 'C', 'V',
  'B', 'N', 'M', '?', ':', '_', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '*', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_cz_qwertz[] =
{
  U_VOID, U_VOID, '~', KEY_DEAD_CARON, KEY_DEAD_CIRCUMFLEX, KEY_DEAD_BREVE, 0xeff8, KEY_DEAD_OGONEK,
  KEY_DEAD_GRAVE, KEY_DEAD_ABOVEDOT, KEY_DEAD_ACUTE, KEY_DEAD_DOUBLEACUTE, KEY_DEAD_DIAERESIS, KEY_DEAD_CEDILLA, U_VOID, U_VOID,
  '\\', '|', 0xefa9, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  0xefa2, U_VOID, 0xeff6, 0xef9e, U_VOID, U_VOID, 0xefa5, 0xefd0,
  0xefd1, '[', ']', U_VOID, U_VOID, 0xef88, 0xef9d, '$',
  0xefe1, U_VOID, U_VOID, 0xefcf, '>', '#', 0xef86, '@',
  '{', '}', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};
/* this prefereable is overloaded via '-I keytable keyb-user'
 * and is preset with an US keyboard layout
 */

CONST t_keysym key_map_user[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '-', '=', 127, 9,
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
  'o', 'p', '[', ']', 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
  '\'', '`', U_VOID, '\\', 'z', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '/', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '<', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_user[] =
{
  U_VOID, 27, '!', '@', '#', '$', '%', '^',
  '&', '*', '(', ')', '_', '+', 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
  'O', 'P', '{', '}', 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
  '"', '~', '0', '|', 'Z', 'X', 'C', 'V',
  'B', 'N', 'M', '<', '>', '?', U_VOID, '*',
  U_VOID, ' ', U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, '>', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_user[] =
{
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};


CONST t_keysym num_table_dot[]   = { '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.', '\0' };
CONST t_keysym num_table_comma[] = { '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', ',', '\0' };

#define CT(X) (sizeof(X)/sizeof(X[0]))
struct keytable_entry keytable_list[] = {
  {"finnish", KEYB_FINNISH, 0, CT(key_map_finnish), CT(num_table_comma),
    key_map_finnish, shift_map_finnish, alt_map_finnish,
    num_table_comma,},
  {"finnish-latin1", KEYB_FINNISH_LATIN1, 0, CT(key_map_finnish_latin1), CT(num_table_comma),
    key_map_finnish_latin1, shift_map_finnish_latin1, alt_map_finnish_latin1,
    num_table_comma,},
  {"us", KEYB_US, 0, CT(key_map_us), CT(num_table_comma),
    key_map_us, shift_map_us, alt_map_us,
    num_table_dot, ctrl_map_us},
  {"uk", KEYB_UK, 0, CT(key_map_uk), CT(num_table_comma),
    key_map_uk, shift_map_uk, alt_map_uk,
    num_table_dot,},
  {"de", KEYB_DE, 0, CT(key_map_de), CT(num_table_comma),
    key_map_de, shift_map_de, alt_map_de,
    num_table_comma, ctrl_map_de},
  {"de-latin1", KEYB_DE_LATIN1, 0, CT(key_map_de_latin1), CT(num_table_comma),
    key_map_de_latin1, shift_map_de_latin1, alt_map_de_latin1,
    num_table_comma,},
  {"fr", KEYB_FR, 0, CT(key_map_fr), CT(num_table_comma),
    key_map_fr, shift_map_fr, alt_map_fr,
    num_table_dot,},
  {"fr-latin1", KEYB_FR_LATIN1, 0, CT(key_map_fr_latin1), CT(num_table_comma),
    key_map_fr_latin1, shift_map_fr_latin1, alt_map_fr_latin1,
    num_table_dot,},
  {"dk", KEYB_DK, 0, CT(key_map_dk), CT(num_table_comma),
    key_map_dk, shift_map_dk, alt_map_dk,
    num_table_comma,},
  {"dk-latin1", KEYB_DK_LATIN1, 0, CT(key_map_dk_latin1), CT(num_table_comma),
    key_map_dk_latin1, shift_map_dk_latin1, alt_map_dk_latin1,
    num_table_comma,},
  {"dvorak", KEYB_DVORAK, 0, CT(key_map_dvorak), CT(num_table_comma),
    key_map_dvorak, shift_map_dvorak, alt_map_dvorak,
    num_table_comma,},
  {"sg", KEYB_SG, 0, CT(key_map_sg), CT(num_table_comma),
    key_map_sg, shift_map_sg, alt_map_sg,
    num_table_comma,},
  {"sg-latin1", KEYB_SG_LATIN1, 0, CT(key_map_sg_latin1), CT(num_table_comma),
    key_map_sg_latin1, shift_map_sg_latin1, alt_map_sg_latin1,
    num_table_comma,},
  {"keyb-no", KEYB_NO, 0, CT(key_map_no), CT(num_table_comma),
    key_map_no, shift_map_no, alt_map_no,
    num_table_comma,},
  {"no-latin1", KEYB_NO_LATIN1, 0, CT(key_map_no_latin1), CT(num_table_comma),
    key_map_no_latin1, shift_map_no_latin1, alt_map_no_latin1,
    num_table_comma,},
  {"sf", KEYB_SF, 0, CT(key_map_sf), CT(num_table_comma),
    key_map_sf, shift_map_sf, alt_map_sf,
    num_table_comma,},
  {"sf-latin1", KEYB_SF_LATIN1, 0, CT(key_map_sf_latin1), CT(num_table_comma),
    key_map_sf_latin1, shift_map_sf_latin1, alt_map_sf_latin1,
    num_table_comma,},
  {"es", KEYB_ES, 0, CT(key_map_es), CT(num_table_comma),
    key_map_es, shift_map_es, alt_map_es,
    num_table_comma,},
  {"es-latin1", KEYB_ES_LATIN1, 0, CT(key_map_es_latin1), CT(num_table_comma),
    key_map_es_latin1, shift_map_es_latin1, alt_map_es_latin1,
    num_table_comma,},
  {"be", KEYB_BE, 0, CT(key_map_be), CT(num_table_comma),
    key_map_be, shift_map_be, alt_map_be,
    num_table_dot,},
  {"po", KEYB_PO, 0, CT(key_map_po), CT(num_table_comma),
    key_map_po, shift_map_po, alt_map_po,
    num_table_dot,},
  {"it", KEYB_IT, 0, CT(key_map_it), CT(num_table_comma),
    key_map_it, shift_map_it, alt_map_it,
    num_table_dot,},
  {"sw", KEYB_SW, 0, CT(key_map_sw), CT(num_table_comma),
    key_map_sw, shift_map_sw, alt_map_sw,
    num_table_dot,},
  {"hu", KEYB_HU, 0, CT(key_map_hu), CT(num_table_comma),
    key_map_hu, shift_map_hu, alt_map_hu,
    num_table_comma,},
  {"hu-cwi", KEYB_HU_CWI, 0, CT(key_map_hu_cwi), CT(num_table_comma),
    key_map_hu_cwi, shift_map_hu_cwi, alt_map_hu_cwi,
    num_table_comma,},
  {"hu-latin2", KEYB_HU_LATIN2, 0, CT(key_map_hu_latin2), CT(num_table_comma),
    key_map_hu_latin2, shift_map_hu_latin2, alt_map_hu_latin2,
    num_table_comma,},
  {"jp106", KEYB_JP106, 0, CT(key_map_jp106), CT(num_table_comma),
    key_map_jp106, shift_map_jp106, alt_map_jp106,
    num_table_dot,},
  {"pl", KEYB_PL, 0, CT(key_map_pl), CT(num_table_comma),
    key_map_pl, shift_map_pl, alt_map_pl,
    num_table_comma,},
  {"hr-cp852", KEYB_HR_CP852, 0, CT(key_map_hr_cp852), CT(num_table_comma),
    key_map_hr_cp852, shift_map_hr_cp852, alt_map_hr_cp852,
    num_table_comma,},
  {"hr-latin2", KEYB_HR_LATIN2, 0, CT(key_map_hr_latin2), CT(num_table_comma),
    key_map_hr_latin2, shift_map_hr_latin2, alt_map_hr_latin2,
    num_table_comma,},
  {"cz-qwerty", KEYB_CZ_QWERTY, 0, CT(key_map_cz_qwerty), CT(num_table_comma),
    key_map_cz_qwerty, shift_map_cz_qwerty, alt_map_cz_qwerty,
    num_table_dot,},
  {"cz-qwertz", KEYB_CZ_QWERTZ, 0, CT(key_map_cz_qwertz), CT(num_table_comma),
    key_map_cz_qwertz, shift_map_cz_qwertz, alt_map_cz_qwertz,
    num_table_comma,},                                       
  {"keyb-user", KEYB_USER, 0, CT(key_map_user), CT(num_table_comma),
    key_map_user, shift_map_user, alt_map_user,
    num_table_dot,},
  {0},
  {0}
};


/*
 * Look for a console. This is based on code in getfd.c from the kbd-0.99 package
 * (see loadkeys(1)).
 */

static int is_a_console(int fd)
{
  char arg = 0;

  return ioctl(fd, KDGKBTYPE, &arg) == 0 && (arg == KB_101 || arg == KB_84);
}

static int open_a_console(char *fnam)
{
  int fd;

  fd = open(fnam, O_RDONLY);
  if(fd < 0 && errno == EACCES) fd = open(fnam, O_WRONLY);
  if(fd < 0 || ! is_a_console(fd)) return -1;

  return fd;
}

static int getfd()
{
  int fd, i;
  char *t[] = { "", "", "", "/dev/tty", "/dev/tty0", "/dev/console" };
  
  for(i = 0; i < sizeof t / sizeof *t; i++) {
    if(!*t[i] && is_a_console(i)) return i;
    if(*t[i] && (fd = open_a_console(t[i])) >= 0) return fd;
  }

  return -1;
}

/*
 * Try to translate a keycode to a DOSEMU keycode...
 */

static char *dosemu_val(unsigned k, t_keysym *dv)
{
	static char b[100];
	char *s = b;
	unsigned t = KTYP(k), v = KVAL(k);
	t_keysym d;
	
	d = KEY_VOID;
	
	switch(t) {
	case KT_LATIN:
	case KT_LETTER: /* is this correct for all KT_LETTERS? */
		d = v;
		break;

#if 0
	case KT_FN:
		switch(k) {
			/* Function keys are hardcodes so ignored... */
		case K_F1:
		case K_F2:
		case K_F3:
		case K_F4:
		case K_F5:
		case K_F6:
		case K_F7:
		case K_F8:
		case K_F9:
		case K_F10:
		case K_F11:
		case K_F12:
			break;
		case K_FIND:
			/* home */
		case K_INSERT:
		case K_REMOVE:
			/* delete */
		case K_SELECT:
			/* end */
		case K_PGUP:
		case K_PGDN:
		case K_MACRO:
			/* menu */
		case K_HELP:
		case K_DO:
			/* execute */
		case K_PAUSE:
		case K_UNDO:
			break;
		default:
			break;
		}
		break;
#endif
		
		
	case KT_SPEC:
		switch(k) {
		case K_HOLE:  d = KEY_VOID; break;
		case K_ENTER: d = KEY_RETURN; break;
		default:      d = KEY_VOID; /* K_CAPS, K_NUM, ... ??? */
		}
		break;
		
	case KT_PAD:
		switch(k) {
		case K_P0:	d = KEY_PAD_0; break;
		case K_P1:	d = KEY_PAD_1; break;
		case K_P2:	d = KEY_PAD_2; break;
		case K_P3:	d = KEY_PAD_3; break;
		case K_P4:	d = KEY_PAD_4; break;
		case K_P5:	d = KEY_PAD_5; break;
		case K_P6:	d = KEY_PAD_6; break;
		case K_P7:	d = KEY_PAD_7; break;
		case K_P8:	d = KEY_PAD_8; break;
		case K_P9:	d = KEY_PAD_9; break;
		case K_PPLUS:	d = KEY_PAD_PLUS; break;
		case K_PMINUS:	d = KEY_PAD_MINUS; break;
		case K_PSTAR:	d = KEY_PAD_AST; break;
		case K_PSLASH:	d = KEY_PAD_SLASH; break;
		case K_PENTER:	d = KEY_PAD_ENTER; break;
		case K_PCOMMA:	d = KEY_PAD_SEPARATOR; break;
		case K_PDOT:	d = KEY_PAD_DECIMAL; break;
		default:	d = KEY_VOID; break;
		}
		break;
		
	case KT_SHIFT:
	case KT_META:
		d = KEY_VOID;
		break;
		
	case KT_DEAD:
		switch(k) {
		case K_DGRAVE: d = KEY_DEAD_GRAVE; break;
		case K_DACUTE: d = KEY_DEAD_ACUTE; break;
		case K_DCIRCM: d = KEY_DEAD_CIRCUMFLEX; break;
		case K_DTILDE: d = KEY_DEAD_TILDE; break;
		case K_DDIERE: d = KEY_DEAD_DIAERESIS; break;
		case K_DCEDIL: d = KEY_DEAD_CEDILLA; break;
		}
		break;

	default:
		break;
	}

	if(dv) *dv = d;

	sprintf(b, "%04x", k);
	
	if(d < 0x20 || d == 0x7f || d >= 0x80) {
		sprintf(b, "0x%02x", d);
	}
	else if (d >= 0x80) {
		sprintf(b, "0x%04x", d);
	}
	else {
		sprintf(b, "'%c' ", (char) d);
	}
	
	if(d == '\'') s = "'\\''";
	if(d == KEY_VOID) s = " -- ";
	
	return s;
}


struct keycode_map {
	unsigned char dosemu;
	unsigned char kernel;
};
static const struct keycode_map dosemu_to_kernel[] = 
{
	{NUM_L_ALT,		0x38},
	{NUM_R_ALT,		100},
	{NUM_L_CTRL,		0x1d},
	{NUM_R_CTRL,		97},
	{NUM_L_SHIFT,		0x2a},
	{NUM_R_SHIFT,		0x36},
	{NUM_NUM,		0x45},
	{NUM_SCROLL,		0x46},
	{NUM_CAPS,		0x3a},
	{NUM_SPACE,		0x39},
	{NUM_BKSP,		0x0e},
	{NUM_RETURN,		0x1c},
	{NUM_TAB,		0x0f},
	{NUM_A,			0x1e},
	{NUM_B,			0x30},
	{NUM_C,			0x2e},
	{NUM_D,			0x20},
	{NUM_E,			0x12},
	{NUM_F,			0x21},
	{NUM_G,			0x22},
	{NUM_H,			0x23},
	{NUM_I,			0x17},
	{NUM_J,			0x24},
	{NUM_K,			0x25},
	{NUM_L,			0x26},
	{NUM_M,			0x32},
	{NUM_N,			0x31},
	{NUM_O,			0x18},
	{NUM_P,			0x19},
	{NUM_Q,			0x10},
	{NUM_R,			0x13},
	{NUM_S,			0x1f},
	{NUM_T,			0x14},
	{NUM_U,			0x16},
	{NUM_V,			0x2f},
	{NUM_W,			0x11},
	{NUM_X,			0x2d},
	{NUM_Y,			0x15},
	{NUM_Z,			0x2c},
	{NUM_1,			0x02},
	{NUM_2,			0x03},
	{NUM_3,			0x04},
	{NUM_4,			0x05},
	{NUM_5,			0x06},
	{NUM_6,			0x07},
	{NUM_7,			0x08},
	{NUM_8,			0x09},
	{NUM_9,			0x0a},
	{NUM_0,			0x0b},
	{NUM_DASH,		0x0c},
	{NUM_EQUALS,		0x0d},
	{NUM_LBRACK,		0x1a},
	{NUM_RBRACK,		0x1b},
	{NUM_SEMICOLON,		0x27},
	{NUM_APOSTROPHE,	0x28},
	{NUM_GRAVE,		0x29},
	{NUM_BACKSLASH,		0x2b},
	{NUM_COMMA,		0x33},
	{NUM_PERIOD,		0x34},
	{NUM_SLASH,		0x35},
	{NUM_LESSGREATER,	0x56},

	{NUM_PAD_0,		0x52},
	{NUM_PAD_1,		0x4f},
	{NUM_PAD_2,		0x50},
	{NUM_PAD_3,		0x51},
	{NUM_PAD_4,		0x4b},
	{NUM_PAD_5,		0x4c},
	{NUM_PAD_6,		0x4d},
	{NUM_PAD_7,		0x47},
	{NUM_PAD_8,		0x48},
	{NUM_PAD_9,		0x49},
	{NUM_PAD_DECIMAL,	0x53},
	{NUM_PAD_SLASH,		98},
	{NUM_PAD_AST,		0x37},
	{NUM_PAD_MINUS,		0x4a},
	{NUM_PAD_PLUS,		0x4e},
	{NUM_PAD_ENTER,		96},

	{NUM_ESC,		0x01},
	{NUM_F1, 		0x3b},
	{NUM_F2, 		0x3c},
	{NUM_F3, 		0x3d},
	{NUM_F4, 		0x3e},
	{NUM_F5, 		0x3f},
	{NUM_F6, 		0x40},
	{NUM_F7, 		0x41},
	{NUM_F8, 		0x42},
	{NUM_F9, 		0x43},
	{NUM_F10,		0x44},
	{NUM_F11,		0x57},
	{NUM_F12,		0x58},

	{NUM_INS,		110},
	{NUM_DEL,		111},
	{NUM_HOME,		102},
	{NUM_END,		107},
	{NUM_PGUP,		104},
	{NUM_PGDN,		109},
	{NUM_UP,		103},
	{NUM_DOWN,		108},
	{NUM_LEFT,		105},
	{NUM_RIGHT,		106},
	
	{NUM_LWIN,		125},
	{NUM_RWIN,		126},
	{NUM_MENU,		127},

	{NUM_PRTSCR_SYSRQ,	99},
	{NUM_PAUSE_BREAK,	119},
};

static int read_kbd_table(struct keytable_entry *kt)
{
	int fd, i, j = -1;
	t_keysym k;
	struct kbentry ke;
	unsigned v, vs, va, vc;
	
	fd = getfd();
	if(fd < 0) {
		fprintf(stderr, "no console\n");
		return 1;
	}
	
	for(i = 0; i < sizeof(dosemu_to_kernel)/sizeof(dosemu_to_kernel[0]); i++) {
		int kernel, dosemu;
		kernel = dosemu_to_kernel[i].kernel;
		dosemu = dosemu_to_kernel[i].dosemu;
		ke.kb_index = kernel;
		
		ke.kb_table = 0;
		if ((j = ioctl(fd, KDGKBENT, (unsigned long) &ke))) break;
		v = ke.kb_value;
		
		ke.kb_table = 1 << KG_SHIFT;
		if ((j = ioctl(fd, KDGKBENT, (unsigned long) &ke))) break;
		vs = ke.kb_value;
		
		ke.kb_table = 1 << KG_ALTGR;
		if ((j = ioctl(fd, KDGKBENT, (unsigned long) &ke))) break;
		va = ke.kb_value;

		ke.kb_table = 1 << KG_CTRL;
		if ((j = ioctl(fd, KDGKBENT, (unsigned long) &ke))) break;
		vc = ke.kb_value;
		
		if(va == v) va = 0;
		
		dosemu_val(v, &k); kt->key_map[dosemu] = k;
		dosemu_val(vs, &k); kt->shift_map[dosemu] = k;
		dosemu_val(va, &k); kt->alt_map[dosemu] = k;
		dosemu_val(vc, &k); kt->ctrl_map[dosemu] = k;
		
#if 0
		printf("%3d: %04x %04x %04x %04x   ", i, v, vs, va, vc);
		
		printf("%s ", dosemu_val(v, &k));
		printf("%s ", dosemu_val(vs, &k));
		printf("%s\n", dosemu_val(va, &k));
		printf("%s\n", dosemu_val(vc, &k));
#endif
	}

	/* look for numpad ',' or '.' */
	ke.kb_index = 83;
	ke.kb_table = 0;
	if(!j && !(j = ioctl(fd, KDGKBENT, (unsigned long) &ke))) {
		if(ke.kb_value == K_PDOT) kt->num_table[12] = '.';
	}

	if(fd > 2) close(fd);

	return j;
}


/*
 * Read the console keyboard description and try to build
 * a DOSEMU compatible map from it.
 *
 * NOTE: If you use X you might get the *wrong* mapping
 * (e.g. on remote machines)... :-)
 */

int setup_default_keytable()
{
  static char *dt_name = "auto";
  static t_keysym 
	  plain_map[NUM_KEY_NUMS], 
	  shift_map[NUM_KEY_NUMS], 
	  alt_map[NUM_KEY_NUMS], 
	  num_map[14],
	  ctrl_map[NUM_KEY_NUMS],
	  shift_alt_map[NUM_KEY_NUMS],
	  ctrl_alt_map[NUM_KEY_NUMS];
  struct keytable_entry *kt;
  int idx;

  idx = sizeof keytable_list / sizeof *keytable_list - 2;

  k_printf("KBD: setup_default_keytable: setting up table %d\n", idx);

  kt = keytable_list + idx;

  kt->name = dt_name;
  kt->keyboard = KEYB_AUTO;
  kt->flags = 0;
  kt->sizemap = CT(plain_map);
  kt->sizepad = CT(num_map);
  kt->key_map = plain_map;
  kt->shift_map = shift_map;
  kt->alt_map = alt_map;
  kt->num_table = num_map;
  kt->ctrl_map = ctrl_map;
  kt->shift_alt_map = shift_alt_map;
  kt->ctrl_alt_map = ctrl_alt_map;

  memcpy(num_map, num_table_comma, sizeof num_map);

  if(read_kbd_table(kt)) {
    k_printf("setup_default_keytable: failed\n");
    return -1;
  }

#if 0
  {
    int i;

    kt = keytable_list;
    i = 0; while(kt->name) {
      printf(
        "%2d: name \"%s\", keyboard %d, flags 0x%x, sizemap %d, sizepad %d\n",
        i, kt->name, kt->keyboard, kt->flags, kt->sizemap, kt->sizepad
      );
      i++, kt++;
    }
  }
#endif

#if 0
  {
    int i;

    kt = keytable_list + KEYB_DE;
    for(i = 0; i < kt->sizemap; i++) {
      printf("%3d: %04x %04x %04x\n",
        i, kt->key_map[i], kt->shift_map[i], kt->alt_map[i]
      );
    }
  }
#endif

  return KEYB_AUTO;
}



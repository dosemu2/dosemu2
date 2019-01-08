/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <dlfcn.h>

#include "keymaps.h"
#include "keyb_clients.h"
#include "keynum.h"
#include "getfd.h"
#include "utilities.h"

#ifdef X_SUPPORT
int X11_DetectLayout (void);
#endif

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
  U_VOID, ' ', U_VOID, DKY_F1, DKY_F2, DKY_F3, DKY_F4, DKY_F5,
  DKY_F6, DKY_F7, DKY_F8, DKY_F9, DKY_F10, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, DKY_F11,
  DKY_F12, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
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
  DKY_DEAD_ACUTE, DKY_DEAD_GRAVE, U_VOID, DKY_DEAD_TILDE, U_VOID, U_VOID, U_VOID, U_VOID,
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
  '7', '8', '9', '0', 0xefe1, DKY_DEAD_ACUTE, 127, 9,
  'q', 'w', 'e', 'r', 't', 'z', 'u', 'i',
  'o', 'p', 0xef81, '+', 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xef94,
  0xef84, DKY_DEAD_CIRCUMFLEX, U_VOID, '#', 'y', 'x', 'c', 'v',
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
  '/', '(', ')', '=', '?', DKY_DEAD_GRAVE, 127, 9,
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
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
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
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
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
  'o', 'p', DKY_DEAD_CIRCUMFLEX, '$', 13, U_VOID, 'q', 's',
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
  'O', 'P', DKY_DEAD_DIAERESIS, 0xef9c, 13, U_VOID, 'Q', 'S',
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
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
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
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
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
  'o', 'p', 0xef86, DKY_DEAD_DIAERESIS, 13, U_VOID, 'a', 's',
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
  '/', '(', ')', '=', '?', DKY_DEAD_GRAVE, 127, 9,
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
  'O', 'P', 0xef8f, DKY_DEAD_CIRCUMFLEX, 13, U_VOID, 'A', 'S',
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
  '{', '[', ']', '}', U_VOID, DKY_DEAD_ACUTE, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, DKY_DEAD_TILDE, 13, U_VOID, U_VOID, U_VOID,
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
    'o',    'p', DKY_DEAD_GRAVE,    '+',     13, U_VOID,    'a',    's',
    'd',    'f',    'g',    'h',    'j',    'k',    'l', 0xefa4,
 DKY_DEAD_ACUTE, 0xefa7, U_VOID, 0xef87,    'z',    'x',    'c',    'v',
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
    'O',    'P',    DKY_DEAD_CIRCUMFLEX,    '*',     13, U_VOID,    'A',    'S',
    'D',    'F',    'G',    'H',    'J',    'K',    'L', 0xefa5,
     DKY_DEAD_DIAERESIS, 0xefa6, U_VOID, 0xef80,    'Z',    'X',    'C',    'V',
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
  'o', 'p', DKY_DEAD_CIRCUMFLEX, '$', 13, U_VOID, 'q', 's',
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
  'O', 'P', DKY_DEAD_DIAERESIS, '*', 13, U_VOID, 'Q', 'S',
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
  U_VOID, U_VOID, '{', '}', DKY_DEAD_ABOVERING, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '[', ']', 13, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  DKY_DEAD_ACUTE, DKY_DEAD_GRAVE, U_VOID, DKY_DEAD_GRAVE, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, DKY_DEAD_CEDILLA, U_VOID, U_VOID, DKY_DEAD_TILDE, U_VOID, U_VOID,
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
  'o', 'p', '+', DKY_DEAD_ACUTE, 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xef87,
  0xefa7, '\\', U_VOID, DKY_DEAD_TILDE, 'z', 'x', 'c', 'v',
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
  'O', 'P', '*', DKY_DEAD_GRAVE, 13, U_VOID, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xef80,
  0xefa6, '|', '0', DKY_DEAD_CIRCUMFLEX, 'Z', 'X', 'C', 'V',
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
  U_VOID, U_VOID, DKY_DEAD_DIAERESIS, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
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
  '=', '(', ')', 0xef99, 0xef9a, 224, 127, 9,
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
  U_VOID, U_VOID, '~', DKY_DEAD_CARON, DKY_DEAD_CIRCUMFLEX, DKY_DEAD_BREVE, 0xeff8, DKY_DEAD_OGONEK,
  DKY_DEAD_GRAVE, DKY_DEAD_ABOVEDOT, DKY_DEAD_ACUTE, DKY_DEAD_DOUBLEACUTE, DKY_DEAD_DIAERESIS, DKY_DEAD_CEDILLA, U_VOID, U_VOID,
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
  U_VOID, U_VOID, '~', DKY_DEAD_CARON, DKY_DEAD_CIRCUMFLEX, DKY_DEAD_BREVE, 0xeff8, DKY_DEAD_OGONEK,
  DKY_DEAD_GRAVE, DKY_DEAD_ABOVEDOT, DKY_DEAD_ACUTE, DKY_DEAD_DOUBLEACUTE, DKY_DEAD_DIAERESIS, DKY_DEAD_CEDILLA, U_VOID, U_VOID,
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
  U_VOID, U_VOID, '~', DKY_DEAD_CARON, DKY_DEAD_CIRCUMFLEX, DKY_DEAD_BREVE, 0xeff8, DKY_DEAD_OGONEK,
  DKY_DEAD_GRAVE, DKY_DEAD_ABOVEDOT, DKY_DEAD_ACUTE, DKY_DEAD_DOUBLEACUTE, DKY_DEAD_DIAERESIS, DKY_DEAD_CEDILLA, U_VOID, U_VOID,
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
  'O', 'P', '`', '{', DKY_DEAD_GRAVE, U_VOID, 'A', 'S',
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
     U_VOID,27,'1','2','3','4','5','6',
     '7','8','9','0','-','=',127,9,
     'q','w','e','r','t','y','u','i',
     'o','p','[',']',13,U_VOID,'a','s',
     'd','f','g','h','j','k','l',';',
     39,96,U_VOID,92,'z','x','c','v',
     'b','n','m',',','.','/',U_VOID,'*',
     U_VOID,32,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,'-',U_VOID,U_VOID,U_VOID,'+',U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID};

CONST t_keysym shift_map_pl[] =
{
     U_VOID,27,'!','@','#','$','%','^',
     '&','*','(',')','_','+',127,9,
     'Q','W','E','R','T','Y','U','I',
     'O','P','{','}',13,U_VOID,'A','S',
     'D','F','G','H','J','K','L',':',
     34,'~',U_VOID,'|','Z','X','C','V',
     'B','N','M','<','>','?',U_VOID,'*',
     U_VOID,32,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,'-',U_VOID,U_VOID,U_VOID,'+',U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID};

CONST t_keysym alt_map_pl[] =
{
     U_VOID,U_VOID,U_VOID,'@',U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,0x0119,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     0x00f3,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,0x0105,0x015b,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,0x0142,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,0x017c,0x017a,0x0107,U_VOID,
     U_VOID,0x0144,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID};

CONST t_keysym shift_alt_map_pl[] =
{
     U_VOID,U_VOID,U_VOID,'@',U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,0x0118,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     0x00d3,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,0x0104,0x015a,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,0x0141,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,0x017b,0x0179,0x0106,U_VOID,
     U_VOID,0x0143,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID};


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
  0xefec, 0xefa0, 0xefa1, 0xef82, '=', DKY_DEAD_ACUTE, 127, 9,
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
  'o', 'p', 0xefa3, ')', 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xef85,
  0xeff5, ';', U_VOID, DKY_DEAD_DIAERESIS, 'z', 'x', 'c', 'v',
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
  '7', '8', '9', '0', '%', DKY_DEAD_CARON, 127, 9,
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
  U_VOID, U_VOID, '~', DKY_DEAD_CARON, DKY_DEAD_CIRCUMFLEX, DKY_DEAD_BREVE, 0xeff8, DKY_DEAD_OGONEK,
  DKY_DEAD_GRAVE, DKY_DEAD_ABOVEDOT, DKY_DEAD_ACUTE, DKY_DEAD_DOUBLEACUTE, DKY_DEAD_DIAERESIS, DKY_DEAD_CEDILLA, U_VOID, U_VOID,
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
  0xefec, 0xefa0, 0xefa1, 0xef82, '=', DKY_DEAD_ACUTE, 127, 9,
  'q', 'w', 'e', 'r', 't', 'z', 'u', 'i',
  'o', 'p', 0xefa3, ')', 13, U_VOID, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xef85,
  0xeff5, ';', U_VOID, DKY_DEAD_DIAERESIS, 'y', 'x', 'c', 'v',
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
  '7', '8', '9', '0', '%', DKY_DEAD_CARON, 127, 9,
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
  U_VOID, U_VOID, '~', DKY_DEAD_CARON, DKY_DEAD_CIRCUMFLEX, DKY_DEAD_BREVE, 0xeff8, DKY_DEAD_OGONEK,
  DKY_DEAD_GRAVE, DKY_DEAD_ABOVEDOT, DKY_DEAD_ACUTE, DKY_DEAD_DOUBLEACUTE, DKY_DEAD_DIAERESIS, DKY_DEAD_CEDILLA, U_VOID, U_VOID,
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

CONST t_keysym key_map_ru[] =
{
  U_VOID, 27, '1', '2', '3', '4', '5', '6',
  '7', '8', '9', '0', '-', '=', 127, 9,
  0x439,0x446,0x443,0x43A,0x435,0x43D,0x433,0x448,0x449,0x437,0x445,0x44A,
  13, U_VOID,
  0x444,0x44B,0x432,0x430,0x43F,0x440,0x43E,0x43B,0x434,0x436,0x44D,0x451,
  U_VOID, '\\',
  0x44F,0x447,0x441,0x43C,0x438,0x442,0x44C,0x431,0x44E,
  '/', U_VOID, '*',
  U_VOID, 32, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym shift_map_ru[] =
{
  U_VOID, 27, '!', '@', '#', '$', '%', '^',
  '&', '*', '(', ')', '_', '+', 127, 9,
  0x419,0x426,0x423,0x41A,0x415,0x41D,0x413,0x428,0x429,0x417,0x425,0x42A,
  13, U_VOID,
  0x424,0x42B,0x412,0x410,0x41F,0x420,0x41E,0x41B,0x414,0x416,0x42D,0x401,
  U_VOID, '\\',
  0x42F,0x427,0x421,0x41C,0x418,0x422,0x42C,0x411,0x42E,
  '/', U_VOID, '*',
  U_VOID, 32, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, '-', U_VOID, U_VOID, U_VOID, '+', U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
  U_VOID
};

CONST t_keysym alt_map_ru[] =
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

CONST t_keysym ctrl_map_ru[] =
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

CONST t_keysym key_map_tr[] =	{
	U_VOID,27,'1','2','3','4','5','6','7','8','9','0','*','-',127,9,
	'q','w','e','r','t','y','u',0x131,'o','p',0x11f,0xfc,13,U_VOID,
	'a','s','d','f','g','h','j','k','l',0x15f,'i',0xe9,U_VOID,44,
	'z','x','c','v','b','n','m',0xf6,0xe7,'.',U_VOID,
	'*',U_VOID,' ',U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
	U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
	U_VOID,'-',U_VOID,U_VOID,U_VOID,'+',U_VOID,
	U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,'<',U_VOID,
	U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
	U_VOID
};
CONST t_keysym shift_map_tr[] =
{
     U_VOID,27,'!','\'','^','+','%','&','/','(',')','=','?','_',127,9,
     'Q','W','E','R','T','Y','U','I','O','P',0x11e,0xdc,13,U_VOID,
		 'A','S','D','F','G','H','J','K','L',0x15e,0x130,34,U_VOID,';',
		 'Z','X','C','V','B','N','M',0xd6,0xc7,':',U_VOID,
		 '*',U_VOID,' ',U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
		 U_VOID,U_VOID,'-',U_VOID,U_VOID,U_VOID,'+',U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,'>',
		 U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
		 U_VOID
};
CONST t_keysym alt_map_tr[] =
{
     U_VOID,U_VOID,U_VOID,U_VOID,'#','$',U_VOID,U_VOID,'{','[',']','}','\\',
		 U_VOID,U_VOID,U_VOID,
     '@',U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
		 '~',U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
		 U_VOID,'`',U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
		 U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
     U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
		 U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
		 U_VOID,U_VOID,'|',U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,U_VOID,
		 U_VOID,U_VOID,U_VOID
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
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
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
  U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID, U_VOID,
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
  /* US must be first in the table, no other rules about order */
  {"us", KEYB_US, 0, CT(key_map_us), CT(num_table_comma),
    key_map_us, shift_map_us, alt_map_us,
    num_table_dot, ctrl_map_us},
  {"finnish", KEYB_FINNISH, 0, CT(key_map_finnish), CT(num_table_comma),
    key_map_finnish, shift_map_finnish, alt_map_finnish,
    num_table_comma,},
  {"finnish-latin1", KEYB_FINNISH_LATIN1, 0, CT(key_map_finnish_latin1), CT(num_table_comma),
    key_map_finnish_latin1, shift_map_finnish_latin1, alt_map_finnish_latin1,
    num_table_comma,},
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
    num_table_comma, 0, shift_alt_map_pl,},
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
  {"ru", KEYB_RU, KT_ALTERNATE, CT(key_map_ru), CT(num_table_dot),
    key_map_ru, shift_map_ru, alt_map_ru,
    num_table_dot, ctrl_map_ru},
  {"tr", KEYB_TR, 0, CT(key_map_tr), CT(num_table_dot),
    key_map_tr, shift_map_tr, alt_map_tr,
    num_table_dot,},
  {"keyb-user", KEYB_USER, 0, CT(key_map_user), CT(num_table_dot),
    key_map_user, shift_map_user, alt_map_user,
    num_table_dot,},
  {0},
  {0},
  {0}
};

#if 0
static char* pretty_keysym(t_keysym d)
{
	static char b[100];
	char *s = b;

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
	if(d == DKY_VOID) s = " -- ";

	return s;
}
#endif

/*
 * Read the console keyboard description and try to build
 * a DOSEMU compatible map from it.
 *
 * NOTE: If you use X you might get the *wrong* mapping
 * (e.g. on remote machines)... :-)
 *
 * we now compare this map and all the keymaps above
 * with the X keymap (if X is available) and chose the
 * best one.
 */
void setup_default_keytable()
{
  static const char *dt_name = "auto";
  static t_keysym
	  plain_map[NUM_DKY_NUMS],
	  shift_map[NUM_DKY_NUMS],
	  alt_map[NUM_DKY_NUMS],
	  num_map[14],
	  ctrl_map[NUM_DKY_NUMS],
	  shift_alt_map[NUM_DKY_NUMS],
	  ctrl_alt_map[NUM_DKY_NUMS];
  struct keytable_entry *kt, *altkt;
  int i, idx;
#if defined(X_SUPPORT) && defined(USE_DL_PLUGINS)
  void *handle;
#endif

  idx = sizeof keytable_list / sizeof *keytable_list - 3;

  k_printf("KBD: setup_default_keytable: setting up table %d\n", idx);

  config.keytable = kt = keytable_list + idx;
  config.altkeytable = altkt = kt + 1;

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

  memcpy(altkt, kt, sizeof(*kt));
  altkt->name = NULL;
  altkt->keyboard = KEYB_AUTO;
  altkt->flags = 0;

  /* Initialize everything to unknown */
  for(i = 0; i < NUM_DKY_NUMS; i++) {
    plain_map[i] = U_VOID;
    shift_map[i] = U_VOID;
    alt_map[i] = U_VOID;
    ctrl_map[i] = U_VOID;
    shift_alt_map[i] = U_VOID;
    ctrl_alt_map[i] = U_VOID;
  }
  /* Copy in the us keymap for a default */
  memcpy(plain_map, key_map_us, sizeof(key_map_us));
  memcpy(shift_map, shift_map_us, sizeof(shift_map_us));
  memcpy(alt_map, alt_map_us, sizeof(alt_map_us));
  memcpy(num_map, num_table_dot, sizeof(num_table_dot));
  memcpy(ctrl_map, ctrl_map_us, sizeof(ctrl_map_us));

  /* Now copy parameters for the linux kernel keymap */
  if(read_kbd_table(kt, altkt)) {
    k_printf("setup_default_keytable: failed to read kernel console keymap\n");
    kt->name = NULL;
  }

  idx = 1;
#ifdef X_SUPPORT
#ifdef USE_DL_PLUGINS
  handle = load_plugin("X");
  if (handle) {
    int (*X11_DetectLayout)(void) =
      (int(*)(void))dlsym(handle, "X11_DetectLayout");
    if (X11_DetectLayout)
      idx = X11_DetectLayout();
  }
#else
  idx = X11_DetectLayout();
#endif
#endif
  if (idx && kt->name == NULL) {
    error("Unable to open console or check with X to evaluate the keyboard "
	  "map.\nPlease specify your keyboard map explicitly via the "
	  "$_layout option. Defaulting to US.\n");
    config.keytable = keytable_list; /* US must be first */
  }
}

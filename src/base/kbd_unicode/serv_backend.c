/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * Description: Keyboard backend - interface to the DOS side
 *
 * Maintainer: Eric Biederman
 *
 * REMARK
 * This module handles interfacing to the DOS side both on int9/port60h level,
 * or on the bios buffer level.
 * Keycodes are buffered in a queue, which, however, has limited depth, so it
 * shouldn't be used for pasting.
 *
 * More information about this module is in doc/README.newkbd
 *
 * /REMARK
 * DANG_END_MODULE
 *
 */

#include <string.h>
#include <stdlib.h>

#include "emu.h"
#include "types.h"
#include "keyboard.h"
#include "keyb_server.h"
#include "keyb_clients.h"
#include "bios.h"
#include "pic.h"
#include "cpu.h"
#include "timers.h"
#include "keystate.h"

/********************** (BIOS) mode backend ***************/

/*
 *    Interface to DOS (BIOS keyboard buffer/shiftstate flags)
 */

void clear_bios_keybuf()
{
   WRITE_WORD(BIOS_KEYBOARD_BUFFER_START,0x001e);
   WRITE_WORD(BIOS_KEYBOARD_BUFFER_END,  0x003e);
   WRITE_WORD(BIOS_KEYBOARD_BUFFER_HEAD, 0x001e);
   WRITE_WORD(BIOS_KEYBOARD_BUFFER_TAIL, 0x001e);
   MEMSET_DOS(BIOS_KEYBOARD_BUFFER,0,32);

   WRITE_BYTE(BIOS_KEYBOARD_TOKEN,0);  /* buffer for Alt-XXX (not used by emulator) */
   put_shift_state(dos_keyboard_state.shiftstate);
   keyb_client_set_leds(get_modifiers_r(dos_keyboard_state.shiftstate));
}

#if 0
static inline Boolean bios_keybuf_full(void)
{
   int start,end,head,tail;

   start = READ_WORD(BIOS_KEYBOARD_BUFFER_START);
   end   = READ_WORD(BIOS_KEYBOARD_BUFFER_END);
   head  = READ_WORD(BIOS_KEYBOARD_BUFFER_HEAD);
   tail  = READ_WORD(BIOS_KEYBOARD_BUFFER_TAIL);

   tail+=2;
   if (tail==end) tail=start;
   return (tail==head);
}

static inline void put_bios_keybuf(Bit16u scancode)
{
   int start,end,head,tail;

   k_printf("KBD: put_bios_keybuf(%04x)\n",(unsigned int)scancode);

   start = READ_WORD(BIOS_KEYBOARD_BUFFER_START);
   end   = READ_WORD(BIOS_KEYBOARD_BUFFER_END);
   head  = READ_WORD(BIOS_KEYBOARD_BUFFER_HEAD);
   tail  = READ_WORD(BIOS_KEYBOARD_BUFFER_TAIL);

   WRITE_WORD(0x400+tail,scancode);
   tail+=2;
   if (tail==end) tail=start;
   if (tail==head) {
      k_printf("KBD: BIOS keyboard buffer overflow\n");
      return;
   }

   WRITE_WORD(BIOS_KEYBOARD_BUFFER_TAIL,tail);
}
#endif

/*
 * update the seg 0x40 keyboard flags from dosemu's internal 'shiftstate'
 * variable.
 * This is called either from kbd_process() or the get_bios_key() helper.
 * It is never called if a dos application takes complete
 * control of int9.
 */

void put_shift_state(t_shiftstate shift)
{
   Bit8u flags1, flags2, flags3, leds;

   flags1=leds=0;
   /* preserve pause bit */
   flags2 = READ_BYTE(BIOS_KEYBOARD_FLAGS2) & PAUSE_MASK;
   flags3 = READ_BYTE(BIOS_KEYBOARD_FLAGS3) & ~0x0c;

   if (shift & INS_LOCK)      flags1 |= 0x80;
   if (shift & CAPS_LOCK)   { flags1 |= 0x40;  leds |= 0x04; }
   if (shift & NUM_LOCK)    { flags1 |= 0x20;  leds |= 0x02; }
   if (shift & SCR_LOCK)    { flags1 |= 0x10;  leds |= 0x01; }
   if (shift & ANY_ALT)       flags1 |= 0x08;
   if (shift & ANY_CTRL)      flags1 |= 0x04;
   if (shift & L_SHIFT)       flags1 |= 0x02;
   if (shift & R_SHIFT)       flags1 |= 0x01;

   if (shift & INS_PRESSED)   flags2 |= 0x80;
   if (shift & CAPS_PRESSED)  flags2 |= 0x40;
   if (shift & NUM_PRESSED)   flags2 |= 0x20;
   if (shift & SCR_PRESSED)   flags2 |= 0x10;
   if (shift & SYSRQ_PRESSED) flags2 |= 0x04;
   if (shift & L_ALT)         flags2 |= 0x02;
   if (shift & L_CTRL)        flags2 |= 0x01;

   flags3 |= 0x10;  /* set MF101/102 keyboard flag */
   if (shift & R_ALT)         flags3 |= 0x08;
   if (shift & R_CTRL)        flags3 |= 0x04;

   WRITE_BYTE(BIOS_KEYBOARD_FLAGS1,flags1);
   WRITE_BYTE(BIOS_KEYBOARD_FLAGS2,flags2);
   WRITE_BYTE(BIOS_KEYBOARD_FLAGS3,flags3);
   WRITE_BYTE(BIOS_KEYBOARD_LEDS,leds);
}

static t_shiftstate get_shift_state(void)
{
   Bit8u flags1, flags2, flags3;
   t_shiftstate shift = 0;

   flags1 = READ_BYTE(BIOS_KEYBOARD_FLAGS1);
   flags2 = READ_BYTE(BIOS_KEYBOARD_FLAGS2);
   flags3 = READ_BYTE(BIOS_KEYBOARD_FLAGS3);

   if (flags1 & 0x80)         shift |= INS_LOCK;
   if (flags1 & 0x40)         shift |= CAPS_LOCK;
   if (flags1 & 0x20)         shift |= NUM_LOCK;
   if (flags1 & 0x10)         shift |= SCR_LOCK;
   if (flags1 & 0x02)         shift |= L_SHIFT;
   if (flags1 & 0x01)         shift |= R_SHIFT;

   if (flags2 & 0x80)         shift |= INS_PRESSED;
   if (flags2 & 0x40)         shift |= CAPS_PRESSED;
   if (flags2 & 0x20)         shift |= NUM_PRESSED;
   if (flags2 & 0x10)         shift |= SCR_PRESSED;
   if (flags2 & 0x04)         shift |= SYSRQ_PRESSED;
   if (flags2 & 0x02)         shift |= L_ALT;
   if (flags2 & 0x01)         shift |= L_CTRL;

   if (flags3 & 0x08)         shift |= R_ALT;
   if (flags3 & 0x04)         shift |= R_CTRL;

   return shift;
}


Bit16u get_bios_key(t_rawkeycode raw)
{
	Boolean make;
	t_keynum key;
	Bit16u bios_key = 0;
	Bit8u flags3 = READ_BYTE(BIOS_KEYBOARD_FLAGS3);

	if (flags3 & 1) {
		if (raw == 0x1d || raw == 0x9d)
			dos_keyboard_state.raw_state.rawprefix = 0xe1;
		else if (raw & 0x80)
			dos_keyboard_state.raw_state.rawprefix = 0xe19d;
		else
			dos_keyboard_state.raw_state.rawprefix = 0xe11d;
	} else if (flags3 & 2)
		dos_keyboard_state.raw_state.rawprefix = 0xe0;
	else
		dos_keyboard_state.raw_state.rawprefix = 0;

	key = compute_keynum(&make, raw, &dos_keyboard_state.raw_state);
	key = compute_functional_keynum(make, key, &dos_keyboard_state.keys_pressed);
	if (key != NUM_VOID) {
		t_shiftstate shiftstate = get_shift_state();
		dos_keyboard_state.shiftstate = shiftstate;
		bios_key = translate_key(make, key, &dos_keyboard_state);
		if (shiftstate != dos_keyboard_state.shiftstate) {
			put_shift_state(dos_keyboard_state.shiftstate);
			keyb_client_set_leds(get_modifiers_r(dos_keyboard_state.shiftstate));
		}
	}

	flags3 = READ_BYTE(BIOS_KEYBOARD_FLAGS3) & ~3;
	switch (dos_keyboard_state.raw_state.rawprefix) {
		case 0xe0:
			flags3 |= 2;
			break;
		case 0xe1:
		case 0xe11d:
		case 0xe19d:
			flags3 |= 1;
			break;
	}
	WRITE_BYTE(BIOS_KEYBOARD_FLAGS3, flags3);

	return bios_key;
}

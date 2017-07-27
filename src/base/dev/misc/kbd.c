/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * Description: Keyboard interface
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
#include "keyboard/keyboard.h"
#include "keyboard/keyb_server.h"
#include "keyboard/keyb_clients.h"
#include "bios.h"
#include "pic.h"
#include "cpu.h"
#include "timers.h"

/*
 * Our keyboard clock rate is 27.5KHz. This looks optimal for dosemu,
 * even though the real keyboards are usually clocked to <= 20KHz.
 * Anyway, 8042 should give an extra delay.
 */
#define KBD_CHAR_PERIOD 400

/* If this is set to 1, the server will check whether the BIOS keyboard buffer is
 * full.
 * This is somewhat inaccurate emulation, as the state of BIOS variables really
 * shouldn't affect 'hardware' behaviour, but this seems the only way of knowing how
 * fast DOS is processing keystrokes, in particular for pasting.
 */
#define KEYBUF_HACK 0

/* the below hacks are disabled in favour of bios-assisted hack in serv_8042.c */
#define USE_KBD_DELAY 0
#define KBD_PIC_HACK 1

/********** QUEUE ***********/

/*
 * This is the dosemu keyboard queue.
 * Each queue entry holds a data structure corresponding to (mostly)
 * one keypress or release event. [The exception are the braindead
 * 0xe02a / 0xe0aa shift key emulation codes the keyboard processor
 * 'decorates' some kinds of keyboard events, which for convenience
 * are treated as seperate events.]
 * Each queue entry holds a up to 4 bytes of raw keycodes for the
 * port 60h emulation, along with a 2-byte translated int16h keycode
 * and the shift state after this event was processed.
 * Note that the bios_key field can be empty (=0), e.g. for shift keys,
 * while the raw field should always contain something.
 */

struct keyboard_queue keyb_queue = {
0, 0, 0, 0
};

static inline Boolean queue_empty(struct keyboard_queue *q)
{
	return (q->head == q->tail);
}


int queue_level(struct keyboard_queue *q)
{
	int n;
	/* q->tail is the first item to pop
	 * q->head is the place to write the next item
	 */
	n = q->head - q->tail;
	return (n < 0) ? n + q->size : n;
}

static inline Boolean queue_full(struct keyboard_queue *q)
{
	return (q->size == 0) || (queue_level(q) == (q->size - 1));
}

/*
 * this has to work even if the variables are uninitailized!
 */
void clear_queue(struct keyboard_queue *q)
{
	q->head = q->tail = 0;
	k_printf("KBD: clear_queue() queuelevel=0\n");
}

void write_queue(struct keyboard_queue *q, t_rawkeycode raw)
{
	int qh;

	k_printf("KBD: writing to queue: scan=%08x\n",
		(unsigned int)raw);

	if (queue_full(q)) {
		/* If the queue is full grow it */
		t_rawkeycode *new;
		int sweep1, sweep2;
		new = malloc(q->size + KEYB_QUEUE_LENGTH);
		if (!new) {
			k_printf("KBD: queue overflow!\n");
			return;
		}
		k_printf("KBD: resize queue %d->%d head=%d tail=%d level=%d\n",
			 q->size, q->size + KEYB_QUEUE_LENGTH, q->head, q->tail, queue_level(q));
		if (q->tail <= q->head) {
			sweep1 = q->head - q->tail;
			sweep2 = 0;
		} else {
			sweep1 = q->size - q->tail;
			sweep2 = q->head;

		}
		memcpy(new, q->queue + q->tail, sweep1);
		memcpy(new + sweep1, q->queue, sweep2);

		free(q->queue);
		q->tail = 0;
		q->head = sweep1 + sweep2;
		q->size += KEYB_QUEUE_LENGTH;
		q->queue = new;
	}
	qh = q->head;
	if (++qh == q->size)
		qh = 0;
	if (qh == q->tail) {
		k_printf("KBD: queue overflow!\n");
		return;
	}
	q->queue[q->head] = raw;
	q->head = qh;
	k_printf("KBD: queuelevel=%d\n", queue_level(q));
}



t_rawkeycode read_queue(struct keyboard_queue *q)
{
	t_rawkeycode *qp;
	t_rawkeycode raw = 0;

	if (!queue_empty(q)) {
		qp = &q->queue[q->tail];

		raw = *qp;
		if (++q->tail == q->size) q->tail = 0;
	}
	return raw;
}

/****************** END QUEUE *******************/

#if USE_KBD_DELAY
static int kbd_period_elapsed(void)
{
	static hitimer_t kbd_time = 0;
	hitimer_t delta = GETusTIME(0) - kbd_time;
	if (delta >= KBD_CHAR_PERIOD) {
		kbd_time = GETusTIME(0);
		return 1;
	}
	return 0;
}
#endif

/****************** KEYBINT MODE BACKEND *******************/

/* run the queue backend in keybint=on mode
 * called either periodically from keyb_server_run or, for faster response,
 * when writing to the queue and after the IRQ1 handler is finished.
 */
void int_check_queue(void)
{
   t_rawkeycode rawscan;

#if 0
   k_printf("KBD: int_check_queue(): queue_empty=%d port60_ready=%d\n",
	    queue_empty(&keyb_queue), port60_ready);
#endif

   if (queue_empty(&keyb_queue))
      return;

#if 1
   if (port60_ready) {
      k_printf("KBD: port60 still has data\n");
      return;
   }
#endif

#if KEYBUF_HACK
   if (bios_keybuf_full() && !(READ_BYTE(BIOS_KEYBOARD_FLAGS2) & PAUSE_MASK))
      return;
#endif

#if USE_KBD_DELAY
   if (!kbd_period_elapsed())
      return;
#endif

#if KBD_PIC_HACK
   /* HACK - extra sentinel needed, timing is not
    * a reliable measure under heavy loads */
   if (pic_irq_active(PIC_IRQ1))
      return;
#endif

   rawscan = read_queue(&keyb_queue);
   k_printf("KBD: read queue: raw=%02x, queuelevel=%d\n",
	rawscan, queue_level(&keyb_queue));
   output_byte_8042(rawscan);
}

/******************* GENERAL ********************************/


void backend_run(void)
{
   int_check_queue();
}


void backend_reset(void)
{
   clear_queue(&keyb_queue);

/* initialise keyboard-related BIOS variables */

   clear_bios_keybuf();
}

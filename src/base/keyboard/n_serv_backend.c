/* 
 * DANG_BEGIN_MODULE
 * 
 * Description: Keyboard backend - interface to the DOS side
 * 
 * Maintainer: Rainer Zimmermann <zimmerm@mathematik.uni-marburg.de>
 * 
 * This module handles interfacing to the DOS side both on int9/port60h level
 * (if keybint=on), or on the bios buffer level.
 * Keycodes are buffered in a queue, which, however, has limited depth, so it
 * shouldn't be used for pasting.
 *
 * More information about this module is in doc/README.newkbd
 *
 * DANG_END_MODULE
 *
 */

#include <string.h>

#include "emu.h"
#include "types.h"
#include "keyboard.h"
#include "keyb_server.h"
#include "bios.h"
#include "pic.h"

/* If this is set to 1, the server will check whether the BIOS keyboard buffer is
 * full even in keybint mode.
 * This is somewhat inaccurate emulation, as the state of BIOS variables really
 * shouldn't affect 'hardware' behaviour, but this seems the only way of knowing how
 * fast DOS is processing keystrokes, in particular for pasting.
 */
#define KEYBUF_HACK 1

t_shiftstate shiftstate;

Bit16u bios_buffer;
t_shiftstate shiftstate_buffer;

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

#define QUEUE_SIZE (KEYB_QUEUE_LENGTH+1)

typedef struct {
   Bit16u bios_key;            /* for the seg 0x40 buffer */
   t_shiftstate shiftstate;
   t_rawkeycode raw[4];        /* sub-queue for raw keycodes - read */
                               /* top-to-bottom, empty entries=0 */
} t_queueentry;

t_queueentry queue[QUEUE_SIZE];
int q_head, q_tail;


/*
 * queue handling
 */

static inline Boolean queue_empty(void) {
   return (q_head==q_tail);
}

#if 0
static inline Boolean queue_full(void) {
}
#endif

int keyb_queuelevel(void) {
   int n;
   n=q_head-q_tail;
   return (n<0) ? n+QUEUE_SIZE : n;
}

/*
 * this has to work even if the variables are uninitailized!
 */
void clear_queue(void) {
   q_head=q_tail=0;
}

void write_queue(Bit16u bios_key,t_shiftstate shift,Bit32u raw) {
   int qh = q_head;

   k_printf("KBD: writing to queue: bios_key=%04x shift=%04x scan=%08x\n",
            bios_key,shift,(unsigned int)raw);
   
   if (++qh==QUEUE_SIZE) qh=0;
   if (qh==q_tail) {
      k_printf("KBD: queue overflow!\n");
      return;
   }
   queue[q_head].bios_key=bios_key;
   queue[q_head].shiftstate=shift;
   *(Bit32u*)&queue[q_head].raw=raw;   /* XXX this is endian dependent! */
   q_head=qh;
}



/*
 * NOTE: this routine does NOT read one queue entry at a time!
 * rather, we first read the raw codes byte-wise and only return the bios
 * codes along with the last raw byte.
 */

void read_queue(Bit16u *bios_key, t_shiftstate *shift, t_rawkeycode *raw) {
   int i;
   t_queueentry *qp;

   *bios_key = 0;

   if (queue_empty()) {
      *raw = 0;
      return;
   }

   qp = &queue[q_tail];

   for(i=3;i>=0;i--)
      if (qp->raw[i]) {
         *raw=qp->raw[i];
         qp->raw[i]=0;
         break;
      }

   /* bios codes and shiftstates are only returned after all of the raw codes
    * in the current queue entry have been read.
    * This mimics the behaviour of a real keyboard bios, i.e. only after int9
    * has read all scancodes of a particular keyboard event, the bios scancode
    * appears in the buffer.
    */

   if (*((Bit32u*)qp->raw)==0) {
      *bios_key = qp->bios_key;
      *shift    = qp->shiftstate;
      if (++q_tail==QUEUE_SIZE) q_tail=0;
   }
}

/****************** END QUEUE *******************/




/********************** NON-KEYBINT (BIOS) mode backend ***************/

/*
 *    Interface to DOS (BIOS keyboard buffer/shiftstate flags)
 */

void clear_bios_keybuf() {
   WRITE_WORD(BIOS_KEYBOARD_BUFFER_START,0x001e);
   WRITE_WORD(BIOS_KEYBOARD_BUFFER_END,  0x003e);
   WRITE_WORD(BIOS_KEYBOARD_BUFFER_HEAD, 0x001e);
   WRITE_WORD(BIOS_KEYBOARD_BUFFER_TAIL, 0x001e);
   MEMSET_DOS(BIOS_KEYBOARD_BUFFER,0,32);
}

static inline Boolean bios_keybuf_full(void) {
   int start,end,head,tail;

   start = READ_WORD(BIOS_KEYBOARD_BUFFER_START);
   end   = READ_WORD(BIOS_KEYBOARD_BUFFER_END);
   head  = READ_WORD(BIOS_KEYBOARD_BUFFER_HEAD);
   tail  = READ_WORD(BIOS_KEYBOARD_BUFFER_TAIL);
   
   tail+=2;
   if (tail==end) tail=start;
   return (tail==head);
}

static inline void put_bios_keybuf(Bit16u scancode) {
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

/*
 * update the seg 0x40 keyboard flags from dosemu's internal 'shiftstate'
 * variable.
 * This is called either from kbd_process() or the get_bios_key() helper
 * (in keybint mode). It is never called if a dos application takes complete
 * control of int9.
 */

void copy_shift_state(t_shiftstate shift) {
   Bit8u flags1, flags2, flags3, leds;

#if 0
   k_printf("KBD: copy_shift_state() %04x\n",shift);
#endif
   
   flags1=flags2=flags3=leds=0;
   if (shift & INS_LOCK)      flags1 |= 0x80;
   if (shift & CAPS_LOCK)   { flags1 |= 0x40;  leds |= 0x04; }
   if (shift & NUM_LOCK)    { flags1 |= 0x20;  leds |= 0x02; }
   if (shift & SCR_LOCK)    { flags1 |= 0x10;  leds |= 0x01; }
   if (shift & ALT)           flags1 |= 0x08;
   if (shift & CTRL)          flags1 |= 0x04;
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

static inline void nonint_check_queue() {
   Bit16u bios_scan;
   t_shiftstate shift;
   t_rawkeycode dummy;

   while (!bios_keybuf_full() && !queue_empty()) {

      read_queue(&bios_scan,&shift,&dummy);
      copy_shift_state(shift);

      if (bios_scan)  {
            
         /* check for 'special action' codes */
         switch(bios_scan)  {
            case SP_BREAK:
               do_soft_int(0x1b);
               break;

            case SP_PAUSE:
               /* XXX */
               break;

            case SP_PRTSCR:
               do_soft_int(0x05);
               break;
        
            case SP_SYSRQ_MAKE:
               _AX=0x8500;
               do_soft_int(0x15);
               break;
         
            case SP_SYSRQ_BREAK:
               _AX=0x8501;
               do_soft_int(0x15);
               break;
         }
         
         if (!IS_SPECIAL(bios_scan))
            put_bios_keybuf(bios_scan);
	 
      }
   }
}


/****************** KEYBINT MODE BACKEND *******************/

/* run the queue backend in keybint=on mode
 * called either periodically from keyb_server_run or, for faster response,
 * when writing to the queue and after the IRQ1 handler is finished.
 */
void int_check_queue() {
   t_rawkeycode rawscan;
   
#if 0
   k_printf("KBD: int_check_queue(): queue_empty=%d port60_ready=%d bios_keybuf_full=%d\n",
	    queue_empty(), port60_ready, bios_keybuf_full());
#endif

   if (queue_empty())
      return;
   
   if (int9_running) {
      k_printf("KBD: int9 running\n");
      return;
   }
   
#if 1
   if (port60_ready) {
      k_printf("KBD: port60 still has data\n");
      return;
   }
#endif   

   if (!port60_ready
#if KEYBUF_HACK
       && !bios_keybuf_full()
#endif
       )
   {
      read_queue(&bios_buffer,&shiftstate_buffer,&rawscan);
      k_printf("KBD: read queue: bios_buffer=%04x shiftstate_buffer=%04x raw=%02x\n",
               bios_buffer,shiftstate_buffer,rawscan);
      k_printf("KBD: queuelevel=%d\n",keyb_queuelevel());

      output_byte_8042(rawscan);
   }
}

/******************* GENERAL ********************************/


void backend_run(void) {
   static int running = 0;

   /* avoid re-entrance problems */
   if (running) {
      k_printf("KBD: backend_run cancelled\n");
      return;
   }
   running++;
   
   if (config.keybint)
      int_check_queue();
   else
      nonint_check_queue();
   
   running--;
}


void backend_reset() {
   clear_queue();

   bios_buffer=0;
   shiftstate_buffer=0;

/* initialise keyboard-related BIOS variables */

   WRITE_BYTE(BIOS_KEYBOARD_TOKEN,0);  /* buffer for Alt-XXX (not used by emulator) */
   
   clear_bios_keybuf();
   copy_shift_state(shiftstate);
}

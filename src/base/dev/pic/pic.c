/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*  DANG_BEGIN_MODULE
 *
 * REMARK
 *  pic.c is a fairly complete emulation of both 8259 Priority Interrupt 
 *  Controllers.  It also includes provision for 16 lower level interrupts.
 *  This implementation supports the following i/o commands:
 *
 * VERB
 *      ICW1    bits 0 and 1     number of ICWs to expect
 *      ICW2    bits 3 - 7       base address of IRQs
 *      ICW3    no bits          accepted but ignored
 *      ICW4    no bits          accepted but ignored
 *
 *      OCW1    all bits         sets interrupt mask
 *      OCW2    bits 7,5-0       EOI commands only
 *      OCW3    bits 0,1,5,6     select read register, 
 *                               select special mask mode
 *
 *     Reads of both pic ports are supported completely.
 * /VERB
 *
 *  An important concept to understand in pic is the interrupt level.
 *  This is a value which represents the priority of the current interrupt.  
 *  It is used to identify interrupts, and IRQs can be mapped to these 
 *  levels(see pic.h~). The currently active interrupt level is maintained 
 *  in pic_ilevel, which is globally available,   A pic_ilevel of 32 means
 *  no interrupts are active; 0, the highest priority, represents the NMI.
 *  IRQs 0 through 15 are mapped, in priority order, to values of 1-15
 *  (there is no IRQ2 in an AT). Values of 16 - 31 represent additional
 *  interrupt levels available for internal dosemu usage.
 *
 *     More detail is available in the file README.pic
 *
 *  Debug information:
 *
 *  A debug flag of +r or 1r will generate debug messages concerning
 *  reads and writes to pic ports.  Many of these messages are not yet
 *  implemented.  A debug flag of 2r will generate messages concerning
 *  activation, nesting, and completion of interrupts.  A flag of -r
 *  or 0r will turn off debugging messages.  Flags may be combined:
 *  to get both 1r and 2r, use a value of 3r.
 *
 * /REMARK
 * DANG_END_MODULE
 */

/*
 * > 
 * > In 0.66.x the new interrupt is scheduled too early, and restore_rm_regs()
 * > is never called. Then the crash.
 * > 
 * > There are at least 2 problems with this kind of code:
 * > 1) avoiding interrupt reentrancy
 * > 2) avoiding too early interrupt scheduling
 * > 
 *
 * You have run into an old problem, which was created through attempts
 * (not by me) to speed up pic by creating holes in its re-entrancy protection.
 * As I released pic, it was quite safe on this score, however, that made
 * dosemu unpleasantly slow.  For everyone's edification, here's the problem:
 * 
 * As dos is designed, it is sometimes necessary to presume that an interrupt
 * (most often the timer, but sometimes the keyboard or even a serial port)
 * could not possibly re-trigger before a certain amount of time has elapsed.
 * Sloppy dos programmers have also often made this assumption even when it was
 * not necessary.
 *
 * Dosemu breaks this assumption, because dos doesn't always have the entire
 * machine, and interrupts can "pile-up" while other processes are running.
 * As far as pic (hardware or emulated) is concerned, an interrupt ends
 * when the interrupt acknowledge is sent to pic.  However, to avoid
 * re-entrancy, pic.c points the iret to a HLT, which is trapped, and used to
 * allow re-triggerring of an interrupt.  This works, and would completely
 * eliminate the above problem, but there's more to the story.
 *
 * Since some programs put great piles of code inside an interrupt ("I've ACK'd
 * the interrupt, what's the hurry on the IRET?"), this kind of hold-off slows
 * dosemu down.  A few (2 or 3) people have modified pic since I released it
 * to create holes in the hold-off mechanism, citing the need for more speed.
 * They were certainly right that speed was needed, but I could never get them
 * to understand the problem they were creating.  These changes are what's
 * creating the re-entrancy, I believe.
 *
 * To make matters worse, dos uses IRETs for software interrupts, even inside
 * hardware interrupt routines.  So the only reliable way to catch the IRET of
 * a hardware interrupt is to point it to a HLT by modifying the stack.  Please
 * don't question this, I've many hours of debugging time that demonstrated
 * this quite thoroughly.
 *
 * BUT...task-switching environments (MS-Win) use hardware interrupts (mostly
 * timer and keyboard, but sometimes serial, too) to switch tasks. When a new
 * task is created, there is no address pointing to a HLT on the (new) stack,
 * so the end of the interrupt code will never be sensed, and the re-trigger
 * protection will lock up dosemu.  So it is necessary to detect this event,
 * and decide that the interrupt has terminated, or we'll never let the timer
 * (or keyboard, etc. as the case may be) interrupt re-trigger.  So, in a way,
 * we MUST create some kind of hole in the hold-off process.
 *
 * The entire pic debate is one of finding the right kind of hole to make, and
 * the trade-off is robustness vs. speed. Since the task switch problem only
 * occurs on the initial invocation of a dos task (when a new stack is built),
 * I elected to use a conservative hole, to maintain robustness.  Others have
 * since modified pic.c to gain speed, often without adequate testing of
 * robustness.
 * 
 * There's lots of re-entrancy protection in pic, but it has largely been
 * defeated by speed-up efforts.  Perhaps the best fix is to re-visit those
 * speed-up efforts, and see if a better solution can't be found.
 *
 * My next thing to try was to put back the correct pic_icount stuff and see if
 * I can't set up separate HLTs for each IRQ, automatically releasing any
 * pending (pic_pirr) interrupts when their HLT is encountered.  The pic_icount
 * and related stuf may still be needed for stack switches.  This may be
 * enough to gain the full speed-up without creating significant holes.
 *
 * The other thing I want to do is some timer changes to better guarantee
 * interrupts get activated in their proper order.  This is a bit messy, and
 * I've got the code somewhere. It involves threading the timer queue.  Some of
 * it may already be done, but I think not.
 *
 * Larry
 */

#include <stdio.h>
#include "config.h"
#include "port.h"
#include "hlt.h"
#include "bitops.h"
#include "pic.h"
#include "memory.h"
#include <sys/time.h>
#include "cpu.h"
#include "emu.h"
#include "timers.h"
#include "iodev.h"
#include "dpmi.h"
#include "serial.h"
#include "int.h"
#include "ipx.h"

#undef us
#define us unsigned
static void pic_activate(void);

static unsigned long pic1_isr;         /* second isr for pic1 irqs */
static unsigned long pic_irq2_ivec = 0;

static Bit16u cb_cs = 0;
static Bit16u cb_ip = 0;

/*
 * pic_pirr contains a bit set for each interrupt which has attempted to
 * re-trigger.
 *
 * pic_icount is an attempt to count active interrupts.  Pending (pic_pirr)
 * interrupts are released when pic_icount gets back to zero (as designed).
 * Haven't looked how it is now, this is one thing others have played with.
 *
 * pic_watch() has the primary job of releasing interrupts when a stack switch
 * occurs.
 */

/* PIC "registers", plus a few more */

static unsigned long pic_irr;          /* interrupt request register */
static unsigned long pic_isr;          /* interrupt in-service register */
static unsigned int pic_iflag;        /* interrupt enable flag: en-/dis- =0/0xfffe */
static unsigned int pic_icount;       /* iret counter (to avoid filling stack) */
static unsigned long pic_irqall = 0xfffe;       /* bits for all IRQs set. */

static unsigned long pic0_imr = 0xf800;  /* interrupt mask register, pic0 */
static unsigned long pic1_imr = 0x0670;         /* interrupt mask register, pic1 */
static unsigned long pic_imr = 0xfff8;          /* interrupt mask register */
static unsigned int pic_stack[32];     /* list of active irqd */
static unsigned int pic_sp = 0;	       /* pointer to pic_stack */ 
static unsigned int pic_vm86_count = 0;   /* count of times 'round the vm86 loop*/
static unsigned int pic_dpmi_count = 0;   /* count of times 'round the dpmi loop*/
static unsigned long pic1_mask = 0x07f8; /* bits set for pic1 levels */
static unsigned long   pic_smm = 0;      /* 32=>special mask mode, 0 otherwise */

static unsigned long   pic_pirr;         /* pending requests: ->irr when icount==0 */
static unsigned long   pic_wirr;             /* watchdog timer for pic_pirr */
static unsigned long   pic_wcount = 0;       /* watchdog for pic_icount  */
unsigned long	pic_icount_od = 1;           /* overdrive for pic_icount_od */
unsigned long	pic_irqs_active = 0;

static   hitimer_t pic_ltime[33] =     /* timeof last pic request honored */
                {NEVER, NEVER, NEVER, NEVER, NEVER, NEVER, NEVER, NEVER,
                 NEVER, NEVER, NEVER, NEVER, NEVER, NEVER, NEVER, NEVER,
                 NEVER, NEVER, NEVER, NEVER, NEVER, NEVER, NEVER, NEVER,
                 NEVER, NEVER, NEVER, NEVER, NEVER, NEVER, NEVER, NEVER,
                 NEVER};
         hitimer_t pic_itime[33] =     /* time to trigger next interrupt */
                {NEVER, NEVER, NEVER, NEVER, NEVER, NEVER, NEVER, NEVER,
                 NEVER, NEVER, NEVER, NEVER, NEVER, NEVER, NEVER, NEVER,
                 NEVER, NEVER, NEVER, NEVER, NEVER, NEVER, NEVER, NEVER,
                 NEVER, NEVER, NEVER, NEVER, NEVER, NEVER, NEVER, NEVER,
                 NEVER};
  
#define PNULL	(void *) 0
static struct lvldef pic_iinfo[32] = {
{PNULL,PNULL,0x02}, {PNULL,PNULL,0x08}, {PNULL,PNULL,0x09}, {PNULL,PNULL,0x70},
{PNULL,PNULL,0x71}, {PNULL,PNULL,0x72}, {PNULL,PNULL,0x73}, {PNULL,PNULL,0x74},
{PNULL,PNULL,0x75}, {PNULL,PNULL,0x76}, {PNULL,PNULL,0x77}, {PNULL,PNULL,0x0b},
{PNULL,PNULL,0x0c}, {PNULL,PNULL,0x0d}, {PNULL,PNULL,0x0e}, {PNULL,PNULL,0x0f},
{PNULL,PNULL,0x00}, {PNULL,PNULL,0x00}, {PNULL,PNULL,0x00}, {PNULL,PNULL,0x00},
{PNULL,PNULL,0x00}, {PNULL,PNULL,0x00}, {PNULL,PNULL,0x00}, {PNULL,PNULL,0x00},
{PNULL,PNULL,0x00}, {PNULL,PNULL,0x00}, {PNULL,PNULL,0x00}, {PNULL,PNULL,0x00},
{PNULL,PNULL,0x00}, {PNULL,PNULL,0x00}, {PNULL,PNULL,0x00}, {PNULL,PNULL,0x00}
};

static void do_irq(int ilevel);

/*
 * run_irq()       checks for and runs any interrupts requested in pic_irr
 * do_irq()        runs the dos interrupt for the current irq
 * set_pic0_imr()  sets pic0 interrupt mask register       \\
 * set_pic1_imr()  sets pic1 interrupt mask register       ||
 * get_pic0_imr()  returns pic0 interrupt mask register    ||
 * get_pic1_imr()  returns pic1 interrupt mask register    || called by read_
 * get_pic0_isr()  returns pic0 in-service register        || and write_ picX
 * get_pic1_isr()  returns pic1 in-service register        ||
 * get_pic0_irr()  returns pic0 interrupt request register ||
 * get_pic1_irr()  returns pic1 interrupt request register ||
 * set_pic0_base() sets base interrupt for irq0 - irq7     ||
 * set_pic1_base() sets base interrupt for irq8 - irq15    //
 * write_pic0()    processes write to pic0 i/o 
 * write_pic1()    processes write to pic1 i/o
 * read_pic0()     processes read from pic0 i/o
 * read_pic1()     processes read from pic1 i/o
 * pic_print()     print pic debug messages
 * pic_watch()     catch any un-reset irets, increment time
 * pic_push()      save active dos interrupt level
 * pic_pop()       restore active dos interrupt level
 * pic_pending()   detect if interrupt is requested and unmasked
 */

#define set_pic0_imr(x) pic0_imr=pic0_to_emu(x);pic_set_mask
#define set_pic1_imr(x) pic1_imr=(((long)x)<<3);pic_set_mask
#define get_pic0_imr()  emu_to_pic0(pic0_imr)
#define get_pic1_imr()  (pic1_imr>>3)
#define get_pic0_isr()  emu_to_pic0(pic_isr)
#define get_pic1_isr()  (pic_isr>>3)
#define get_pic0_irr()  emu_to_pic0(pic_irr)
#define get_pic1_irr()  (pic_irr>>3)


/*  State flags.  picX_cmd is only read by debug output */

static unsigned char pic0_isr_requested; /* 0/1 =>next port 0 read=  irr/isr */
static unsigned char pic1_isr_requested;
static unsigned char pic0_icw_state; /* 0-3=>next port 1 write= mask,ICW2,3,4 */
static unsigned char pic1_icw_state;
static unsigned char pic0_cmd; /* 0-3=>last port 0 write was none,ICW1,OCW2,3*/
static unsigned char pic1_cmd;

/* DANG_BEGIN_FUNCTION pic_print
 *
 * This is the pic debug message printer.  It writes out some basic 
 * information, followed by an informative message.  The basic information
 * consists of: 
 *       interrupt nesting counter change flag (+, -, or blank)
 *       interrupt nesting count (pic_icount)
 *       interrupt level change flag (+, -, or blank)
 *       current interrupt level
 *       interrupt in-service register
 *       interrupt mask register
 *       interrupt request register
 *       message part one
 *       decimal data value
 *       message part two
 *
 * If the message part 2 pointer is a null pointer, then only message
 * part one (without the data value) is printed.
 *
 * The change flags are there to facilitate grepping for changes in 
 * pic_ilevel and pic_icount
 * 
 * To avoid line wrap, the first seven values are printed without labels.
 * Instead, a header line is printed every 15 messages.
 *
 * DANG_END_FUNCTION pic_print
 */
#ifdef NO_DEBUGPRINT_AT_ALL
#define pic_print(code,s1,v1,s2)
#else
#define pic_print(code,s1,v1,s2)	if (debug_level('r')>code){p_pic_print(s1,v1,s2);}

static void p_pic_print(char *s1, int v1, char *s2)
{
static int oldi=0, oldc=0, header_count=0;
int pic_ilevel=find_bit(pic_isr);
char ci,cc;

  if (pic_icount > oldc) cc='+';
  else if(pic_icount < oldc) cc='-';
  else cc=' ';
  oldc=pic_icount;

  if (pic_ilevel > oldi) ci='+';
  else if(pic_ilevel < oldi) ci='-';
  else ci=' ';
  oldi = pic_ilevel;
  if (!header_count++)
    log_printf(1, "PIC: cnt lvl pic_isr  pic_imr  pic_irr (column headers)\n");
  if(header_count>15) header_count=0;
  
  if(s2)
  log_printf(1, "PIC: %c%2d %c%2d %08lx %08lx %08lx %s%02d%s\n",
     cc, pic_icount, ci, pic_ilevel, pic_isr, pic_imr, pic_irr, s1, v1, s2);
  else
  log_printf(1, "PIC: %c%2d %c%2d %08lx %08lx %08lx %s\n",
     cc, pic_icount, ci, pic_ilevel, pic_isr, pic_imr, pic_irr, s1);

}
#endif

/* DANG_BEGIN_REMARK pic_push,pic_pop
 *
 * Pic maintains two stacks of the current interrupt level. an internal one
 * is maintained by run_irqs, and is valid whenever the emulator code for
 * an interrupt is active.  These functions maintain an external stack,
 * which is valid from the time the dos interrupt code is called until
 * the code has issued all necessary EOIs.  Because pic will not necessarily 
 * get control immediately after an EOI, another EOI (for another interrupt)
 * could occur.  This external stack is kept strictly synchronized with
 * the actions of the dos code to avoid any problems.  pic_push and pic_pop
 * maintain the external stack.

 * DANG_END_REMARK pic_print
 */
static inline void pic_push(int val)
{
    if(pic_sp<32){
       pic_stack[pic_sp++]=val;
    } else {
       pic_print(1,"pic_stack overrun! ",0,"");
    }
}

static inline int pic_pop(void)
{
    if(pic_sp) {
       return pic_stack[--pic_sp];
    } else {
       pic_print(1,"pic_stack empty! ",0,"");
       return 32;
    }
}


static void set_pic0_base(unsigned char int_num)
{
  unsigned char int_n;
  int_n        = int_num & 0xf8;         /* it's not worth doing a loop */
  pic_iinfo[1].ivec  = int_n++;  /* irq  0 */
  pic_iinfo[2].ivec  = int_n++;  /* irq  1 */
  pic_irq2_ivec      = int_n++;  /* irq  2 */
  pic_iinfo[11].ivec = int_n++;  /* irq  3 */
  pic_iinfo[12].ivec = int_n++;  /* irq  4 */
  pic_iinfo[13].ivec = int_n++;  /* irq  5 */
  pic_iinfo[14].ivec = int_n++;  /* irq  6 */
  pic_iinfo[15].ivec = int_n;    /* irq  7 */
  return;
}


static void set_pic1_base(unsigned char int_num)
{
  unsigned char int_n;
  int_n        = int_num & 0xf8;         /* it's not worth doing a loop */
  pic_iinfo[3].ivec  = int_n++;  /* irq  8 */
  pic_iinfo[4].ivec  = int_n++;  /* irq  9 */
  pic_iinfo[5].ivec  = int_n++;  /* irq 10 */
  pic_iinfo[6].ivec  = int_n++;  /* irq 11 */
  pic_iinfo[7].ivec  = int_n++;  /* irq 12 */
  pic_iinfo[8].ivec  = int_n++;  /* irq 13 */
  pic_iinfo[9].ivec  = int_n++;  /* irq 14 */
  pic_iinfo[10].ivec = int_n;    /* irq 15 */
  return;
}


/* DANG_BEGIN_FUNCTION write_pic0,write_pic1
 *
 * write_pic_0() and write_pic1() implement dos writes to the pic ports.
 * They are called by the code that emulates inb and outb instructions.
 * Each function implements both ports for the pic:  pic0 is on ports
 * 0x20 and 0x21; pic1 is on ports 0xa0 and 0xa1.  These functions take
 * two arguments: a port number (0 or 1) and a value to be written.
 *
 * DANG_END_FUNCTION
 */
void write_pic0(ioport_t port, Bit8u value)
{

/* if port == 0 this must be either an ICW1, OCW2, or OCW3
 * if port == 1 this must be either ICW2, ICW3, ICW4, or load IMR 
 */

#if 0
static char  icw_state,              /* !=0 => port 1 does icw 2,3,(4) */
#endif
static char                icw_max_state;          /* number of icws expected        */
int ilevel;			  /* level to reset on outb 0x20  */

port -= 0x20;
ilevel = 32;
if (pic_isr)
  ilevel=find_bit(pic_isr);
if (ilevel != 32 && !test_bit(ilevel, &pic_irqall)) {
  /* this is a fake IRQ, don't allow to reset its ISR bit */
  pic_print(1, "Protecting ISR bit for lvl ", ilevel, " from spurious EOI");
  ilevel = 32;
}

if (in_dpmi)
  dpmi_return_request();	/* we have to leave the signal context */

if(!port){                          /* icw1, ocw2, ocw3 */
  if(value&0x10){                   /* icw1 */
    icw_max_state = (value & 1) + 1;
    if(value&2) ++icw_max_state;
    pic0_icw_state = 1;
    pic0_cmd=1;
    }
  
  else if (value&0x08){              /* ocw3 */
    if(value&2) pic0_isr_requested = value&1;
    if(value&64)pic_smm = value&32; /* must be either 0 or 32, conveniently */
    pic0_cmd=3;
    }
  else if((value&0xb8) == 0x20) {    /* ocw2 */
    /* irqs on pic1 require an outb20 to each pic. we settle for any 2 */
     if(!clear_bit(ilevel,&pic1_isr)) {
       clear_bit(ilevel,&pic_isr);  /* the famous outb20 */
       pic_print(1,"EOI resetting bit ",ilevel, " on pic0");
       }
     else
       pic_print(1,"EOI resetting bit ",ilevel, " on pic1");
     pic0_cmd=2;
      }
   }
else                              /* icw2, icw3, icw4, or mask register */
    switch(pic0_icw_state){
     case 0:                        /* mask register */
       set_pic0_imr(value);
       break;
     case 1:                        /* icw2          */
       set_pic0_base(value);
     default:                       /* icw2, 3, and 4*/
       if(pic0_icw_state++ >= icw_max_state) pic0_icw_state=0; 
  }
}  


void write_pic1(ioport_t port, Bit8u value)
{
/* if port == 0 this must be either an ICW1, OCW2, or OCW3 */
/* if port == 1 this must be either ICW2, ICW3, ICW4, or load IMR */
static char /* icw_state, */     /* !=0 => port 1 does icw 2,3,(4) */
               icw_max_state;    /* number of icws expected        */
int ilevel;			  /* level to reset on outb 0x20  */

port -= 0xa0;
ilevel = 32;
if (pic_isr)
  ilevel=find_bit(pic_isr);
if (ilevel != 32 && !test_bit(ilevel, &pic_irqall)) {
  /* this is a fake IRQ, don't allow to reset its ISR bit */
  pic_print(1, "Protecting ISR bit for lvl ", ilevel, " from spurious EOI");
  ilevel = 32;
}

if (in_dpmi)
  dpmi_return_request();	/* we have to leave the signal context */

if(!port){                            /* icw1, ocw2, ocw3 */
  if(value&0x10){                     /* icw1 */
    icw_max_state = (value & 1) + 1;
    if(value&2) ++icw_max_state;
    pic1_icw_state = 1;
    pic1_cmd=1;
    }
  else if (value&0x08) {                /* ocw3 */
    if(value&2) pic1_isr_requested = value&1;
    if(value&64)pic_smm = value&32; /* must be either 0 or 32, conveniently */
    pic1_cmd=3;
    }
  else if((value&0xb8) == 0x20) {    /* ocw2 */
    /* irqs on pic1 require an outb20 to each pic. we settle for any 2 */
     if(!clear_bit(ilevel,&pic1_isr)) {
       clear_bit(ilevel,&pic_isr);  /* the famous outb20 */
       pic_print(1,"EOI resetting bit ",ilevel, " on pic0");
       }
     else
       pic_print(1,"EOI resetting bit ",ilevel, " on pic1");
     pic0_cmd=2;
     }
  }
else                         /* icw2, icw3, icw4, or mask register */
  switch(pic1_icw_state){
     case 0:                    /* mask register */
       set_pic1_imr(value);
       break;
     case 1:                    /* icw 2         */
       set_pic1_base(value);
     default:                   /* icw 2,3 and 4 */
       if(pic1_icw_state++ >= icw_max_state) pic1_icw_state=0; 
  }
}  


/* DANG_BEGIN_FUNCTION read_pic0,read_pic1
 *
 * read_pic0 and read_pic1 return the values for the interrupt mask register
 * (port 1), or either the in service register or interrupt request register,
 * as determined by the last OCW3 command (port 0).  These functions take
 * a single parameter, which is a port number (0 or 1).  They are called by
 * code that emulates the inb instruction.
 *
 * DANG_END_FUNCTION
 */
Bit8u read_pic0(ioport_t port)
{
  port -= 0x20;
  if(port)		return((unsigned char)get_pic0_imr());
  if(pic0_isr_requested) return((unsigned char)get_pic0_isr());
                         return((unsigned char)get_pic0_irr());
}


Bit8u read_pic1(ioport_t port)
{
  port -= 0xa0;
  if(port)		return((unsigned char)get_pic1_imr());
  if(pic1_isr_requested) return((unsigned char)get_pic1_isr());
                         return((unsigned char)get_pic1_irr());
}

/* DANG_BEGIN_FUNCTION pic_seti
 *
 * pic_seti is used to initialize an interrupt for dosemu.  It requires
 * four parameters.  The first parameter is the interrupt level, which
 * may select the NMI, any of the IRQs, or any of the 16 extra levels
 * (16 - 31).  The second parameter is the dosemu function to be called
 * when the interrupt is activated.  This function should call do_irq()
 * if the DOS interrupt is really to be activated.  If there is no special
 * dosemu code to call, the second parameter can specify do_irq(), but
 * see that description for some special considerations. 
 * The third parameter is a number of an interrupt to activate if there is
 * no default interrupt for this ilevel.
 * The fourth parameter is the dosemu function to be called from do_irq().
 * Required by some internal dosemu drivers that needs some additional code
 * before calling an actual handler. This function MUST provide a EOI at
 * the end of a callback.
 *
 * DANG_END_FUNCTION
 */
void pic_seti(unsigned int level, int (*func)(int), unsigned int ivec,
  void (*callback)(void))
{
  if(level>=32) return;
  if(pic_iinfo[level].func) {
    if(pic_iinfo[level].func != func) {
      error("Attempt to register more than one handler for IRQ level %i (%p %p)\n",
        level, pic_iinfo[level].func, func);
      config.exitearly = 1;
    } else {
      error("Handler for IRQ level %i was registered more than once! (%p)\n",
        level, func);
    }
  }
  pic_iinfo[level].func = func;
  if(callback) {
    pic_iinfo[level].callback = callback;
    set_bit(level, &pic_irqall);
  }
  if(level>15) pic_iinfo[level].ivec = ivec;
}


/* DANG_BEGIN_FUNCTION run_irqs
 *
 * run_irqs, which is initiated via the macro pic_run, is the "brains" of
 * the pic.  It is called from the vm86() loop, checks for the highest
 * priority interrupt requested, and executes it.  This function is
 * written in assembly language in order to take advantage of atomic
 * (indivisible) instructions, so that it should be safe for a two
 * process model, even in a multiple CPU machine.  A c language
 * version was started, but it became impossible, even with in-line 
 * assembly macros, because such macros can only return a single result.
 * If I find a way to do it in c, I will, but don't hold your breath.
 *
 * I found a way to write it in C --EB 15 Jan 97
 *
 * DANG_END_FUNCTION
 */
/* DANG_BEGIN_COMMENT
 *   This is my C version of the assembly version of run_irqs.
 *   I believe it is a correct translation of the assembly into C.
 *
 *   I got frustrated with the old incomplete version.  Especially
 *   because of indent problems and I don't believe in changing code I
 *   don't understand.
 *
 *   This code is correct in the single process case.
 *   Except possilby it's handling of special mask mode.
 *   If pic_smm is set to 32 it appears to me that no interrupt is
 *   ever run.
 *
 *   But this code is certainly incorrect in the multiple process
 *   case.  Because it spins without rereading pic_irr, to recalculate
 *   pic_ilevel.  And I think there are other less serious problems as
 *   well.  However it doesn't matter because we aren't doing multiple
 *   processes, or multiple threads. :)
 *
 *   Note Comments taken from the original C version
 *
 *   --EB 5 Jan 97
 *  DANG_END_COMMENT
 */
void run_irqs(void)
/* find the highest priority unmasked requested irq and run it */
{
       int int_request;

       /* don't allow HW interrupts in force trace mode */
       if (vm86s.vm86plus.vm86dbg_TFpendig) return;

       /* check for and find any requested irqs.  Having found one, we atomic-ly
        * clear it and verify it was there when we cleared it.  If it wasn't, we
        * look for the next request.  There are two in_service bits for pic1 irqs.
        * This is needed, because irq 8-15 must do 2 outb20s, which, if the dos
        * irq code actually runs, will reset the bits.  We also reset them here,
        * since dos code won't necessarily run.
        */
       while((int_request = pic_irr & ~(pic_isr | pic_imr)) != 0) { /* while something to do*/
               int local_pic_ilevel, old_ilevel, ret;

	       if (!isset_IF()) {
		       set_VIP();
	    	       return;                      /* exit if ints are disabled */
	       }

               local_pic_ilevel = find_bit(int_request);    /* find out what it is  */
	       old_ilevel = find_bit(pic_isr);
               /* In case int_request has no bits set */
               if (local_pic_ilevel == -1)
                       return;
               if (local_pic_ilevel >= old_ilevel + pic_smm)  /* priority check */
                       return;

               if (pic_irqs_active && local_pic_ilevel >= find_bit(pic_irqs_active))
                       return;

               clear_bit(local_pic_ilevel, &pic_irr);
	       /* pic_isr bit is set in do_irq() */
               ret = (pic_iinfo[local_pic_ilevel].func ?
	    	      pic_iinfo[local_pic_ilevel].func(local_pic_ilevel) : 1);      /* run the function */
	       if (ret) {
		       do_irq(local_pic_ilevel);
	       }
       }
}

   
/* DANG_BEGIN_FUNCTION do_irq
 *
 *  do_irq() calls the correct do_int().
 *  It then executes a vm86 loop until an outb( end-of-interrupt) is found.
 *  For priority levels 0 and >15 (not real IRQs), vm86 executes once, then 
 *  returns, since no outb20 will come.
 *  Returns: 0 = complete, 1 = interrupt not run because it directly
 *  calls our "bios"   See run_timer_tick() in timer.c for an example
 *  To assure notification when the irq completes, we push flags, ip, and cs
 *  here and fake cs:ip to PIC_[SEG,OFF], where there is a hlt.  This makes 
 *  the irq generate a sigsegv, which calls pic_iret when it completes.
 *  pic_iret then pops the real cs:ip from the stack.
 *  This routine is RE-ENTRANT - it calls run_irqs,
 *  which may call an interrupt routine,
 *  which may call do_irq().  Be Careful!  !!!!!!!!!!!!!!!!!! 
 *  No single interrupt is ever re-entered.
 *
 * Callers:
 * base/misc/ioctl.c
 * base/keyboard/serv_8042.c
 * base/keyboard/keyboard-server.c
 * base/serial/ser_irq.c
 * dosext/sound/sound.c
 * dosext/net/net/pktnew.c
 *
 * DANG_END_FUNCTION
 */
static void do_irq(int ilevel)
{
    int intr;

    set_bit(ilevel, &pic_isr);     /* set in-service bit */
    set_bit(ilevel, &pic1_isr);    /* pic1 too */
    pic1_isr &= pic_isr & pic1_mask;         /* isolate pic1 irqs */

    intr=pic_iinfo[ilevel].ivec;

    if(ilevel==PIC_IRQ9)      /* unvectored irq9 just calls int 0x0a.. */
      if(!IS_REDIRECTED(intr)) {intr=0x0a;pic1_isr&= 0xffef;} /* & one EOI */

     if (test_bit(ilevel, &pic_irqall)) {
       pic_push(ilevel);
       set_bit(ilevel, &pic_irqs_active);
       pic_icount++;
     }

     if (!in_dpmi || in_dpmi_dos_int) {
 /* save the real return address on the stack. Make iret return to
  * PIC_SEG:PIC_OFF so we can catch it.
  */
      fake_call_to(PIC_SEG, PIC_OFF);
      if(debug_level('r')>7) r_printf("PIC: setting iret trap at %04x:%04lx\n",
        REG(cs), REG(eip));
     }

     if (pic_iinfo[ilevel].callback)
        pic_iinfo[ilevel].callback();
     else {
       if (in_dpmi) run_pm_int(intr);
       else {
 /* schedule the requested interrupt, then enter the vm86() loop */
         run_int(intr);
       }
     }
}

/* DANG_BEGIN_FUNCTION pic_resched
 *
 * pic_resched decrements a count of interrupts on the stack
 * (set by do_irq()). If the count is then less or equal to some pre-defined
 * value (normally 1, pic_icount_od), pic_resched moves all queued interrupts 
 * to the interrupt request register.
 *
 * Normally it is called from pic_iret(), but it can also be called 
 * directly if dosemu was fooled by the program and failed to catch iret.
 *
 * DANG_END_FUNCTION
 */
void
pic_resched(void) 
{
unsigned long pic_newirr, pic_last_ilevel;
    if(pic_icount) {
       pic_icount--;
       pic_last_ilevel = pic_pop();
       if (pic_icount != pic_sp)
         error("pic_icount=%i != pic_sp=%i\n", pic_icount, pic_sp);
       if (!test_bit(pic_last_ilevel, &pic_irqs_active)) {
         error("PIC: bit %li is not set, irqs_active=%lx\n",
	   pic_last_ilevel, pic_irqs_active);
	 pic_irqs_active = 0;
       }
       clear_bit(pic_last_ilevel, &pic_irqs_active);
    } else if (pic_irqs_active) {
      error("pic_icount=%i and pic_irqs_active=%lx are out of sync!\n",
        pic_icount, pic_irqs_active);
      pic_irqs_active = 0;
    }
    if(pic_icount<=pic_icount_od) {
	pic_newirr=pic_pirr&~pic_irr&~pic_isr;
        pic_irr|=pic_newirr;
        pic_pirr&=~pic_newirr;
        pic_wirr&=~pic_newirr;        /* clear watchdog timer */
	pic_activate();
    }
    if (!pic_icount)
      pic_wcount = 0;
}

/* DANG_BEGIN_FUNCTION pic_request
 *
 * pic_request triggers an interrupt.  There is presently no way to
 * "un-trigger" an interrupt.  The interrupt will be initiated the
 * next time pic_run is called, unless masked or superceded by a 
 * higher priority interrupt.  pic_request takes one argument, an
 * interrupt level, which specifies the interrupt to be triggered.
 * If that interrupt is already active, the request will be queued 
 * until all active interrupts have been completed.  The queue is
 * only one request deep for each interrupt, so it is the responsibility
 * of the interrupt code to retrigger itself if more interrupts are
 * needed.
 *
 * DANG_END_FUNCTION
 */
int pic_request(int inum)
{
static char buf[81];
  int ret=PIC_REQ_NOP;

  if ((pic_irr|pic_isr)&(1<<inum))
    {
    if (pic_pirr&(1<<inum)){
     ret=PIC_REQ_LOST;
     pic_print(2,"Requested irq lvl ",    inum, " lost     ");
      }
    else {
     ret=PIC_REQ_PEND;
     pic_print(2,"Requested irq lvl ",    inum, " pending  ");
      }
    pic_pirr|=(1<<inum);
    if(pic_itime[inum] == pic_ltime[inum]) {
       pic_print(2,"pic_itime and pic_ltime for timer ",inum," matched!");
       pic_itime[inum] = pic_itime[32];
    }
    pic_ltime[inum] = pic_itime[inum];
  }
  else {
    pic_print(2,"Requested irq lvl ",    inum, " successfully");
    pic_irr|=(1<<inum);
    if(pic_itime[inum] == pic_ltime[inum]) pic_itime[inum] = pic_itime[32];
    pic_ltime[inum] = pic_itime[inum];
    ret=PIC_REQ_OK;
  }
  if (debug_level('r') >2) {
    /* avoid going through sprintf for non-debugging */
    sprintf(buf,", k%d",(int)pic_dpmi_count);
    pic_print(2,"Zeroing vm86, DPMI from ",pic_vm86_count,buf);
  }
  pic_vm86_count=pic_dpmi_count=0;
  return ret;
}

void pic_untrigger(int inum)
{
    if ((pic_irr | pic_pirr) & (1<<inum)) {
      pic_print(2,"Requested irq lvl ", inum, " untriggered");
    }
    pic_pirr &= ~(1<<inum);
    pic_irr &= ~(1<<inum);
    pic_wirr &= ~(1<<inum);
}

/* DANG_BEGIN_FUNCTION pic_iret
 *
 * pic_iret is used to sense that all active interrupts are really complete,
 * so that interrupts queued by pic_request can be triggered.
 * Interrupts end when they issue an outb 0x20 to the pic, however it is
 * not yet safe at that time to retrigger interrupts, since the stack has
 * not been restored to its initial state by an iret.  pic_iret is called
 * whenever interrupts have been enabled by a popf, sti, or iret.  It 
 * determines if an iret was the cause by comparing stack contents with
 * cs and ip. If so, it calls pic_resched() and does the actual iret by
 * pop'ing ip and cs from stack.
 * It is possible for pic_iret to be fooled by dos code; for this reason 
 * active interrupts are checked, any queued interrupts that are also 
 * active will remain queued.
 * Also, some programs fake an iret, so that it is possible for pic_iret 
 * to fail.
 * See pic_watch for the watchdog timer that catches and fixes this event.
 *
 * DANG_END_FUNCTION
 */
void
pic_iret_dpmi(void)
{
/* Even if cs:ip now points to PIC_SEG:PIC_OFF hlt, it means nothing while
 * in protected mode, so we can't modify it here.
 */
    pic_resched();
    pic_print(2,"IRET in dpmi, loops=",pic_dpmi_count," ");
    pic_dpmi_count=0;
    if ((cb_cs || cb_ip) && !pic_icount) {
      r_printf("PIC: entering callback at %x:%x\n", cb_cs, cb_ip);
      fake_pm_int();
     /* we will have an extra pic_iret() after this, but no problems
      * since pic_icount==0
      */
      fake_call_to(cb_cs, cb_ip);
      cb_cs = cb_ip = 0;
    }
}

void
pic_iret(void)
{
/* if we've really come from an irq, cs:ip will point to PIC_SEG:PIC_OFF */
/* if we haven't come from an irq and cs:ip points as above, we're going to
 * die anyway, since our next instruction is a hlt.
 */
      if (REG(cs) != PIC_SEG || LWORD(eip) != PIC_OFF)
        error("pic_iret called from %#04x:%#04x\n", REG(cs), LWORD(eip));
      if (!pic_icount)
        r_printf("pic_iret called when pic_icount==0 !!\n");
      pic_resched();
      pic_print(2,"IRET in vm86, loops=", in_dpmi?
      pic_dpmi_count : pic_vm86_count," ");
      pic_vm86_count=0;
      pic_dpmi_count=0;
      if ((cb_cs || cb_ip) && !pic_icount) {
	r_printf("PIC: entering callback at %x:%x\n", cb_cs, cb_ip);
	LWORD(eip) = cb_ip;
	REG(cs) = cb_cs;
	cb_cs = cb_ip = 0;
      }
      else {
	unsigned char * ssp;
	unsigned long sp;
	ssp = SEG2LINEAR(LWORD(ss));
	sp = (unsigned long) LWORD(esp);
	LWORD(eip) = popw(ssp, sp);
	REG(cs) = popw(ssp, sp);
	LWORD(esp) = (LWORD(esp) + 4) & 0xffff;
      }
}

 
/* DANG_BEGIN_FUNCTION pic_watch
 *
 * pic_watch is a watchdog timer for pending interrupts.  If pic_iret
 * somehow fails to activate a pending interrupt request for 2 consecutive 
 * timer ticks, pic_watch will activate them anyway.  pic_watch is called
 * ONLY by timer_tick, the interval timer signal handler, so the two functions
 * will probably be merged.
 *
 * DANG_END_FUNCTION
 */
inline void pic_watch(hitimer_u *s_time)
{
hitimer_t t_time;
unsigned long pic_newirr;

  pic_newirr=pic_wirr&~pic_irr&~pic_isr;
  pic_irr|=pic_newirr;
  pic_pirr&=~pic_newirr;
  pic_wirr&=~pic_newirr;
  pic_wirr|=pic_pirr;   	  /* set a new pending list for next time */
/*  pic_activate(); */

/*  calculate new sys_time
 *  values are kept modulo 2^32 (exactly 1 hour)
 */
  t_time = s_time->td;

  /* check for any freshly initiated timers, and sync them to s_time */
  pic_print(2,"pic_itime[1]= ",pic_itime[1]," ");
  pic_sys_time=t_time + (t_time == NEVER);
  pic_print(2,"pic_sys_time set to ",pic_sys_time," ");
  pic_dos_time = pic_itime[32];
  if(pic_icount<=pic_icount_od) pic_activate();
  if(config.pic_watchdog > 0) {
    if(pic_icount && !pic_isr) {
      if(++pic_wcount >= config.pic_watchdog) {
        r_printf("PIC: force reschedule\n");
        pic_resched();
        pic_wcount = 0;
      }
    }
  }
}      


/* DANG_BEGIN_FUNCTION pic_pending
 * This function returns a non-zero value if the designated interrupt has
 * been requested and is not masked.  In these circumstances, it is important
 * for a hardware emulation to return a status which does *not* reflect the
 * event(s) which caused the request, until the interrupt actually gets 
 * processed.  This, in turn, hides the interrupt latency of pic from the dos
 * software.
 *
 * The single parameter ilevel is the interrupt level (see pic.h) of the
 * interrupt of interest.
 *
 * If the requested interrupt level is currently active, the returned status
 * will depend upon whether the interrupt code has re-requested itself.  If
 * no re-request has occurred, a value of false (zero) will be returned.  
 * DANG_END_FUNCTION
 */
 
int pic_pending(void)
{
    return (pic_irr & ~(pic_isr | pic_imr | pic_irqs_active));
}

int pic_irq_active(int num)
{
    return test_bit(num, &pic_isr);
}

int pic_irq_masked(int num)
{
    return test_bit(num, &pic_imr);
}

/* DANG_BEGIN_FUNCTION pic_activate
 * 
 * pic_activate requests any interrupts whose scheduled time has arrived.
 * anything after pic_dos_time and before pic_sys_time is activated.
 * pic_dos_time is advanced to the earliest time scheduled.
 * DANG_END_FUNCTION
 */
static void pic_activate(void)
{
hitimer_t earliest;
int timer, count;

/*if(pic_irr&~pic_imr) return;*/
   earliest = pic_sys_time;
   count = 0;
   for (timer=0; timer<32; ++timer) { 
      if ((pic_itime[timer] != NEVER) && (pic_itime[timer] < pic_sys_time)) {
         if (pic_itime[timer] != pic_ltime[timer]) {
               if ((earliest == NEVER) || (pic_itime[timer] < earliest))
                    earliest = pic_itime[timer];
               pic_request(timer);
               ++count;
         }
      }
   }
   if(count) pic_print(2,"Activated ",count, " interrupts.");
   pic_print(2,"Activate ++ dos time to ",earliest, " ");
   pic_print(2,"pic_sys_time is ",pic_sys_time," ");
   /*if(!pic_icount)*/ pic_dos_time = pic_itime[32] = earliest;
}

/* DANG_BEGIN_FUNCTION pic_sched
 * pic_sched schedules an interrupt for activation after a designated
 * time interval.  The time measurement is in unis of 1193047/second,
 * the same rate as the pit counters.  This is convenient for timer
 * emulation, but can also be used for pacing other functions, such as
 * serial emulation, incoming keystrokes, or video updates.  Some sample 
 * intervals:
 *
 * rate/sec:	5	7.5	11	13.45	15	30	60
 * interval:	238608	159072	108459	88702	79536	39768	19884
 *
 * rate/sec:	120	180	200	240	360	480	720
 * interval:	9942	6628	5965	4971	3314	2485	1657
 *
 * rate/sec:	960	1440	1920	2880	3840	5760	11520
 * interval:	1243	829	621	414	311	207	103	
 *
 * pic_sched expects two parameters: an interrupt level and an interval.
 * To assure proper repeat scheduling, pic_sched should be called from
 * within the interrupt handler for the same interrupt.  The maximum 
 * interval is 15 minutes (0x3fffffff).
 * DANG_END_FUNCTION
 */
 
void pic_sched(int ilevel, int interval)
{
  char mesg[35];

 /* default for interval is 65536 (=54.9ms)
  * There's a problem with too small time intervals - an interrupt can
  * be continuously scheduled, without letting any time to process other
  * code.
  *
  * BIG WARNING - in non-periodic timer modes pit[0].cntr goes to -1
  *	at the end of the interval - was this the reason for the following
  *	[1u-15sec] range check?
  */
  if(interval > 0 && interval < 0x3fffffff) {
     if(pic_ltime[ilevel]==NEVER) {
	pic_itime[ilevel] = pic_itime[32] + interval;
     } else {
	pic_itime[ilevel] = pic_itime[ilevel] + interval;
     }
  }
  if (debug_level('r') > 2) {
    /* avoid going through sprintf for non-debugging */
    sprintf(mesg,", delay= %d.",interval);
    pic_print(2,"Scheduling lvl= ",ilevel,mesg);
    pic_print(2,"pic_itime set to ",pic_itime[ilevel],"");
  }
}

void pic_set_callback(Bit16u cs, Bit16u ip)
{
  r_printf("PIC: setting callback to %x:%x (pic_icount=%u)\n",
    cs, ip, pic_icount);
  cb_ip = ip;
  cb_cs = cs;
}

void pic_sti(void)
{
  pic_iflag = 0;
  pic_set_mask;
}

void pic_cli(void)
{
  pic_iflag = pic_irqall;
  pic_set_mask;
}

int CAN_SLEEP(void)
{
  return (!(pic_icount || pic_isr || (REG(eflags) & VIP) || signal_pending ||
    (pic_sys_time > pic_dos_time)));
}

void pic_init(void)
{
  /* do any one-time initialization of the PIC */
  emu_iodev_t  io_device;
  emu_hlt_t    hlt_hdlr;

  /* 8259 PIC (Programmable Interrupt Controller) */
  io_device.read_portb   = read_pic0;
  io_device.write_portb  = write_pic0;
  io_device.read_portw   = NULL;
  io_device.write_portw  = NULL;
  io_device.read_portd   = NULL;
  io_device.write_portd  = NULL;
  io_device.handler_name = "8259 PIC0";
  io_device.start_addr   = 0x0020;
  io_device.end_addr     = 0x0021;
  io_device.irq          = EMU_NO_IRQ;
  io_device.fd = -1;
  port_register_handler(io_device, 0);

  io_device.handler_name = "8259 PIC1";
  io_device.start_addr = 0x00A0;
  io_device.end_addr   = 0x00A1;
  io_device.read_portb   = read_pic1;
  io_device.write_portb  = write_pic1;
  port_register_handler(io_device, 0);

  hlt_hdlr.name       = "PIC";
  hlt_hdlr.start_addr = PIC_ADD - BIOS_HLT_BLK;
  hlt_hdlr.end_addr = hlt_hdlr.start_addr;
  hlt_hdlr.func       = (emu_hlt_func)pic_iret;
  hlt_register_handler(hlt_hdlr);
}

void pic_reset(void)
{
  pic_set_mask;
}

/* 
 * (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
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
#ifndef C_RUN_IRQS
#define C_RUN_IRQS
#include "bitops.h"
#undef C_RUN_IRQS
#else
#include "bitops.h"
#endif
#include "pic.h"
#include "memory.h"
/* #include <sys/vm86.h> */
#include <sys/time.h>
#include "cpu.h"
#include "emu.h"
#include "timers.h"
#include "iodev.h"
#include "dpmi.h"
#include "serial.h"
#include "int.h"
#include "ipx.h"
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif

#undef us
#define us unsigned
void timer_tick(void);
void pic_activate();
extern void timer_int_engine(void);

static unsigned long pic1_isr;         /* second isr for pic1 irqs */
static unsigned long pic_irq2_ivec = 0;

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

#if 0
static unsigned long pic1_mask;        /* bits set for pic1 levels */
#endif
static unsigned long pic1_mask = 0x07f8; /* bits set for pic1 levels */
/* 
even asgcc -Wall says;
pic.c: At top level:
pic.c:81: warning: `pic1_mask' defined but not used
though it _is_ used in the assembly part
*/
static unsigned long   pic_smm = 0;      /* 32=>special mask mode, 0 otherwise */

static unsigned long   pic_pirr;         /* pending requests: ->irr when icount==0 */
static unsigned long   pic_wirr;             /* watchdog timer for pic_pirr */
static unsigned long   pic_wcount = 0;       /* watchdog for pic_icount  */
unsigned long	pic_icount_od = 1;           /* overdrive for pic_icount_od */
#if 0
unsigned long   pic0_imr = 0xf800;  /* IRQs 3-7 on pic0 start out disabled */
unsigned long   pic1_imr = 0x07f8;  /* IRQs 8-15 on pic1 start out disabled */
unsigned long   pic_imr = 0xfff8;   /* interrupt mask register, enable irqs 0,1 */
unsigned long   pice_imr = -1;      /* interrupt mask register, dos emulator */ 
unsigned long   pic_stack[32];      /* stack of interrupt levels */
unsigned long   pic_sp = 0;         /* pointer to stack */
unsigned long   pic_vm86_count=0;   /* counter for trips around vm86 loop */
unsigned long   pic_dpmi_count=0;   /* counter for trips around dpmi loop */
         hitimer_t pic_sys_time=NEVER; /* system time for scheduling interrupts */
#endif
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
static struct lvldef pic_iinfo[32] =
                     {{PNULL,0x02}, {PNULL,0x08}, {PNULL,0x09}, {PNULL,0x70},
                      {PNULL,0x71}, {PNULL,0x72}, {PNULL,0x73}, {PNULL,0x74},
                      {PNULL,0x75}, {PNULL,0x76}, {PNULL,0x77}, {PNULL,0x0b},
                      {PNULL,0x0c}, {PNULL,0x0d}, {PNULL,0x0e}, {PNULL,0x0f},
                      {PNULL,0x00}, {PNULL,0x00}, {PNULL,0x00}, {PNULL,0x00},
                      {PNULL,0x00}, {PNULL,0x00}, {PNULL,0x00}, {PNULL,0x00},
                      {PNULL,0x00}, {PNULL,0x00}, {PNULL,0x00}, {PNULL,0x00},
                      {PNULL,0x00}, {PNULL,0x00}, {PNULL,0x00}, {PNULL,0x00}};   

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
#define pic_print(code,s1,v1,s2)	if (d.request>code){p_pic_print(s1,v1,s2);}

static void p_pic_print(char *s1, int v1, char *s2)
{
static int oldi=0, oldc=0, header_count=0;
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
  log_printf(1, "PIC: %c%2ld %c%2ld %08lx %08lx %08lx %s%02d%s\n",
     cc, pic_icount, ci, pic_ilevel, pic_isr, pic_imr, pic_irr, s1, v1, s2);
  else
  log_printf(1, "PIC: %c%2ld %c%2ld %08lx %08lx %08lx %s\n",
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
inline void pic_push(int val)
{
    if(pic_sp<32){
       pic_stack[pic_sp++]=val;
    } else {
       pic_print(1,"pic_stack overrun! ",0,"");
    }
}

inline int pic_pop()
{
    if(pic_sp) {
       return pic_stack[--pic_sp];
    } else {
       pic_print(1,"pic_stack empty! ",0,"");
       return 32;
    }
}


void set_pic0_base(int_num)
unsigned char int_num;
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


void set_pic1_base(int_num)
unsigned char int_num;
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
ilevel=pic_ilevel;
if(pic_sp) {
   if(ilevel!=pic_stack[pic_sp-1]) {
      pic_print(2,"ilevel=",ilevel," differed from pic_stack");
   }
   ilevel=pic_stack[pic_sp-1];
}

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
     if(!pic_sp) {
	pic_print(2,"EOI with nothing on pic_stack! ilevel=",ilevel,"");
     } else if(pic_stack[pic_sp-1]!=ilevel) {
	pic_print(2,"Stack value differed from ilevel! stack=",pic_stack[pic_sp-1],"");
     }
    /* irqs on pic1 require an outb20 to each pic. we settle for any 2 */
     if(!clear_bit(ilevel,&pic1_isr)) {
       clear_bit(ilevel,&pic_isr);  /* the famous outb20 */
       pic_ilevel=pic_pop();  /* only pop stack when pic_isr gets cleared */
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
ilevel=pic_ilevel;
if(pic_sp) {
   if(ilevel!=pic_stack[pic_sp-1]) {
      pic_print(2,"ilevel=",ilevel," differed from pic_stack");
   }
   ilevel=pic_stack[pic_sp-1];
}

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
     if(!pic_sp) {
	pic_print(2,"EOI with nothing on pic_stack! ilevel=",ilevel,"");
     } else if(pic_stack[pic_sp-1]!=ilevel) {
	pic_print(2,"Stack value differed from ilevel! stack=",pic_stack[pic_sp-1],"");
     }
    /* irqs on pic1 require an outb20 to each pic. we settle for any 2 */
     if(!clear_bit(ilevel,&pic1_isr)) {
       clear_bit(ilevel,&pic_isr);  /* the famous outb20 */
       pic_ilevel=pic_pop();  /* only pop stack when pic_isr gets cleared */
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


/* DANG_BEGIN_FUNCTION pic_mask,pic_unmask
 *
 * The pic maintains an additional interrupt mask which is not visible
 * to the DOS process.  This is normally cleared (enabling an interrupt)
 * when an interrupt is initialized, but dosemu code may choose to
 * use this mask internally.  One possible use is to implement the interrupt
 * gate controlled by the OUT2 bit of the 16550A UART's Modem Control 
 * Register.  This mask is cleared by pic_unmaski() and set by pic_maski()
 *
 * DANG_END_FUNCTION
 */
void pic_unmaski(level)
int level;
{
 pic_print(2,"Unmasking lvl= ",level,"");
 if(pic_iinfo[level].func != (void*)0) clear_bit(level,&pice_imr);
 pic_set_mask;
}


void pic_maski(level)
int level;
{
  pic_print(2,"Masking lvl= ",level,"");
  set_bit(level,&pice_imr);
}


/* DANG_BEGIN_FUNCTION pic_seti
 *
 * pic_seti is used to initialize an interrupt for dosemu.  It requires
 * three parameters.  The first parameter is the interrupt level, which
 * man select the NMI, any of the IRQs, or any of the 16 extra levels
 * (16 - 31).  The second parameter is the dosemu function to be called
 * when the interrupt is activated.  This function should call do_irq()
 * if the DOS interruptis really to be activated.  If there is no special
 * dosemu code to call, the second parameter can specify do_irq(), but
 * see that description for some special considerations.
 *
 * DANG_END_FUNCTION
 */
void pic_seti(level,func,ivec)
unsigned int level,ivec;
void (*func);
{
  if(level>=32) return;
  pic_iinfo[level].func = func;
  if(level>15) pic_iinfo[level].ivec = ivec;
  if(func == (void*)0) pic_maski(level);
}


/* this is the guts if the irq processor. it should be called from an if:
 * if(pic_irr) run_irqs();     
 * This will maximize speed.
 */
 
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
 * DANG_END_FUNCTION
 */
#ifdef C_RUN_IRQS
/* see assembly language version below */
void run_irqs()
/* find the highest priority unmasked requested irq and run it */
{
 int old_ilevel;
 int int_request;
#warning using C run_irqs

/* check for and find any requested irqs.  Having found one, we atomic-ly
 * clear it and verify it was there when we cleared it.  If it wasn't, we
 * look for the next request.  There are two in_service bits for pic1 irqs.
 * This is needed, because irq 8-15 must do 2 outb20s, which, if the dos
 * irq code actually runs, will reset the bits.  We also reset them here,
 * since dos code won't necessarily run.  
 */
   
#if 0
 if(!(pic_irr&~(pic_isr|pic_imr))) return;     /* exit if nothing to do */
#else
 if(!(pic_irr&~(pic_isr))) return;     /* exit if nothing to do */
#endif
 old_ilevel=pic_ilevel;                          /* save old pic_ilevl   */
 
#if 0
 while(int_request= pic_irr&~(pic_isr|pic_imr)) /* while something to do*/
#else
 while(int_request= pic_irr&~(pic_isr)) /* while something to do*/
#endif
  {
      
  pic_ilevel=find_bit(&int_request);              /* find out what it is  */
      
  if(pic_ilevel<(old_ilevel+pic_smm))            /* priority check       */
    if(clear_bit(pic_ilevel,&pic_irr))           /* dbl check & clear req*/
      {     
      /* we got one!  it is identified by pic_ilevel */

     set_bit(pic_ilevel,&pic_isr);               /* set in-service bit   */
     pic1_isr = pic_isr & pic1_mask;            /* pic1 too            */
     pic_iinfo[pic_ilevel].func();               /* run the function     */
     clear_bit(pic_ilevel,&pic_isr);             /* clear in_service bit */
     clear_bit(pic_ilevel,&pic1_isr);            /* pic1 too            */
     }
   }
   /* whether we did or didn't :-( get one, we must still reset pic_ilevel */
   pic_ilevel=old_ilevel;
}

#else

/* I got inspired.  Here's an assembly language version, somewhat faster.
 * This is also much shorter - fewer checks needed, because there's no
 * question of compiler behavior, and less code because we can use both 
 * results and flags from an instruction.
 * RE-ENTRANT CODE - uses 16 bytes of stack
 */
void run_irqs()
{

#if 1 /* BUG FIXER (if 1), lets WIN31 run */
if ((!(REG(eflags) & VIF)) && (!in_dpmi) ) {
#if 0
  g_printf("*");
#endif
  return;
}
#else
if (!(REG(eflags) & VIF)) return;
#endif

/* Without this one dope crashes (DPMI_rm_procedure_* running increases 
 * up to a stack fault)
 *
 * This is a gross disgusting cludge, that I am going to make even grosser
 * and more disgusting in an attempt to get it to work properly.  Someday
 * I am going to find and fix the root problems that are making this 
 * necessary.  -KenC
 */
if (in_dpmi && in_dpmi_timer_int) return;

#warning using assembly run_irqs,expect pic1_mask not used
  __asm__ __volatile__
  ("movl "CISH_INLINE(pic_ilevel)",%%ecx\n\t" /* get old ilevel              */
   "pushl %%ecx\n\t"                          /* save old ilevel             */
   "addl "CISH_INLINE(pic_smm)",%%ecx\n\t"    /* check spec. mask mode       */
                                              /***  while(.....){            */
   "1:movl "CISH_INLINE(pic_isr)",%%eax\n\t" /* 1: int_request = (pic_isr   */
   "orl "CISH_INLINE(pic_imr)",%%eax\n\t"     /* | pic_imr)                  */
   "notl %%eax\n\t"                           /* int_request = ~int_request  */
   "andl "CISH_INLINE(pic_irr)",%%eax\n\t"    /* int_request &= pic_irr      */
   "jz 3f\n\t"                                /* if(!int_request) goto 3     */
   "2:bsfl %%eax,%%ebx\n\t"                  /* 2: ebx=find_bit(int_request)*/
   "jz 3f\n\t"                                /* if(!int_request) goto 3     */
   "cmpl %%ebx,%%ecx\n\t"                     /* if(ebx > pic_ilevel)...     */
   "jle 3f\n\t"                               /* ... goto 3                  */
   "btrl %%ebx,"CISH_INLINE(pic_irr)"\n\t"    /* clear_bit(ebx,&pic_irr)     */
   "jnc 2b\n\t"                               /* if bit wasn't set, go to 2  */
   "movl "CISH_INLINE(pic_isr)",%%ecx\n\t"    /* get current pic_isr         */
   "btsl %%ebx,%%ecx\n\t"                     /* set bit in pic_isr          */
   "movl %%ebx,"CISH_INLINE(pic_ilevel)"\n\t" /* set new ilevel              */
   "movl %%ecx,"CISH_INLINE(pic_isr)"\n\t"    /* save new isr                */
   "andl "CISH_INLINE(pic1_mask)",%%ecx\n\t"  /* isolate pic1 irqs           */
   "movl %%ecx,"CISH_INLINE(pic1_isr)"\n\t"   /* save in pic1_isr            */
   "call *"CISH_INLINE(pic_iinfo)"(,%%ebx,8)\n\t" /* call interrupt handler  */
   "movl "CISH_INLINE(pic_ilevel)",%%eax\n\t" /* get new ilevel              */
   "btrl %%eax,"CISH_INLINE(pic_isr)"\n\t"    /* reset isr bit - just in case*/
   "btrl %%eax,"CISH_INLINE(pic1_isr)"\n\t"   /* reset isr bit - just in case*/
   "jmp 1b\n\t"                               /* go back for next irq        */
                                              /**** end of while   }      ****/
   "3:popl "CISH_INLINE(pic_ilevel)"\n\t"    /* 3: restore old ilevel & exit*/
   :                                          /* no output                   */
   :                                          /* no input                    */
   :"eax","ebx","ecx");                       /* registers eax, ebx, ecx used*/

 } 
#endif
               
   
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
int do_irq()
{
 int intr;
 unsigned char * ssp;
 unsigned long sp;
#ifdef X86_EMULATOR
 unsigned char *tmp_ssp;
 int tmp;
#endif

    if(pic_ilevel==32) return 0;
    intr=pic_iinfo[pic_ilevel].ivec;

    if(pic_ilevel==PIC_IRQ9)      /* unvectored irq9 just calls int 0x0a.. */
      if(!IS_REDIRECTED(intr)) {intr=0x0a;pic1_isr&= 0xffef;} /* & one EOI */
#ifndef USE_NEW_INT
    if(IS_REDIRECTED(intr)||pic_ilevel<=PIC_IRQ1||in_dpmi)
#endif /* not USE_NEW_INT */
    {
#if 0 /* BUG CATCHER (if 1) */
/* outputting more then one character here will change dynamic behave such
 * that we measure the wrong thing.
 */
g_printf("+%d",(int)pic_ilevel);
#endif
     if(pic_ilevel < 16) pic_push(pic_ilevel);
     if (in_dpmi) {
      run_pm_int(intr);
     } else {
#ifndef USE_NEW_INT
      pic_cli();
#endif /* not USE_NEW_INT */
      ssp = (unsigned char *)(LWORD(ss)<<4);
      sp = (unsigned long) LWORD(esp);

 /* save the real return address on the stack. Make iret return to
  * PIC_SEG:PIC_OFF so we can catch it.
  * change esp first to protect the stack we're about to use
  */
      LWORD(esp) = (LWORD(esp) - 4) & 0xffff;  
#ifdef X86_EMULATOR
      tmp_ssp = ssp+sp;
      tmp = E_MUNPROT_STACK(tmp_ssp);
#endif
      pushw(ssp, sp, REG(cs));
      pushw(ssp, sp, LWORD(eip));
#ifdef X86_EMULATOR
      if (tmp) E_MPROT_STACK(tmp_ssp);
#endif
      REG(cs) = PIC_SEG;
      LWORD(eip) = PIC_OFF;
      
 /* schedule the requested interrupt, then enter the vm86() loop */
      run_int(intr);
     }
      pic_icount++;
      pic_wcount++;

 /* enter PIC loop - we can continue only when pic_isr has been cleared */
      while(!fatalerr && test_bit(pic_ilevel,&pic_isr))
      {
	if (d.request>2)
		r_printf("------ PIC: intr loop ---%06lx--%06lx----\n",
			pic_vm86_count,pic_dpmi_count);
	if (in_dpmi ) {
          ++pic_dpmi_count;
	  pic_print(2, "Initiating DPMI irq lvl ", pic_ilevel, " in do_irq");
	  run_dpmi();
	  }
	else {
	  ++pic_vm86_count;
	  pic_print(2, "Initiating VM86 irq lvl ", pic_ilevel, " in do_irq");
          run_vm86();
          }
        pic_isr &= PIC_IRQALL;    /*  levels 0 and 16-31 are Auto-EOI  */
        serial_run();           /*  delete when moved to timer stuff */
        if(pic_irr) run_irqs();
#if 0
#ifdef USING_NET
        /* check for available packets on the packet driver interface */
        /* (timeout=0, so it immediately returns when none are available) */
        pkt_check_receive(0);
#endif
#endif
      }
      pic_sti();
      return(0);

    }   /* else */ 
    return(1);
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
int pic_request(inum)
int inum;
{
static char buf[81];
  int ret=PIC_REQ_NOP;

  if (pic_iinfo[inum].func == (void *)0)
    return ret; 

#if 1		/* use this result mouse slowdown in winos2 */
  if (((pic_irr|pic_isr)&(1<<inum)) || (pic_icount>pic_icount_od))
#else          /* this makes mouse work under winos2, but sometimes */
	       /* results in internal stack overflow  */
  if(pic_isr&(1<<inum) || pic_irr&(1<<inum))
#endif
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
  if (d.request&2) {
    /* avoid going through sprintf for non-debugging */
    sprintf(buf,", k%d",(int)pic_dpmi_count);
    pic_print(2,"Zeroing vm86, DPMI from ",pic_vm86_count,buf);
  }
  pic_vm86_count=pic_dpmi_count=0;
  return ret;
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
 * cs and ip.  If so, it decrements a count of interrupts on the stack
 * (set by do_irq()).  If the count is then zero, pic_iret moves all queued
 * interrupts to the interrupt request register.  It is possible for pic_iret
 * to be fooled by dos code; for this reason active interrupts are checked,
 * any queued interrupts that are also active will remain queued.  Also,
 * some programs fake an iret, so that it is possible for pic_iret to fail.
 * See pic_watch for the watchdog timer that catches and fixes this event.
 *
 * DANG_END_FUNCTION
 */
void
pic_iret()
{
unsigned char * ssp;
unsigned long sp;
unsigned long pic_newirr;

  if(in_dpmi) {
    if(pic_icount) {
       pic_icount--;
       pic_newirr=pic_pirr&~pic_irr&~pic_isr;
       pic_irr|=pic_newirr;
       pic_pirr&=~pic_newirr;
       pic_wirr&=~pic_newirr;        /* clear watchdog timer */
       if(pic_icount<=pic_icount_od)
	  pic_activate();
       if(!pic_irr) dpmi_eflags &= ~VIP;
    } 
     pic_print(2,"IRET in dpmi, loops=",pic_dpmi_count," ");
     pic_dpmi_count=0;
  return;
  }

/* if we've really come from an irq, cs:ip will point to PIC_SEG:PIC_OFF */
/* if we haven't come from an irq and cs:ip points as above, we're going to
 * die anyway, since our next instruction is a hlt.
 */ 
 if(REG(cs) == PIC_SEG)
   if(LWORD(eip) == PIC_OFF) {
    if(pic_icount) { 
       pic_icount--;
       pic_newirr=pic_pirr&~pic_irr&~pic_isr;
       pic_irr|=pic_newirr;
       pic_pirr&=~pic_newirr;
       pic_wirr&=~pic_newirr;        /* clear watchdog timer */
       if(pic_icount<=pic_icount_od)
	  pic_activate();
       if(!pic_irr) REG(eflags)&=~(VIP);
    }
     pic_print(2,"IRET in vm86, loops=",pic_vm86_count," ");
     pic_vm86_count=0;
      ssp = (unsigned char *)(LWORD(ss)<<4);
      sp = (unsigned long) LWORD(esp);
      LWORD(eip) = popw(ssp, sp);
      REG(cs) = popw(ssp, sp);
      LWORD(esp) = (LWORD(esp) + 4) & 0xffff;
    }
 return;
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
inline void pic_watch(s_time)
hitimer_u *s_time;	/* time in us, 64-bit unsigned */
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
  t_time = UStoTICK(s_time->td);

  /* check for any freshly initiated timers, and sync them to s_time */
  pic_print(2,"pic_itime[1]= ",pic_itime[1]," ");
  pic_sys_time=t_time + (t_time == NEVER);
  pic_print(2,"pic_sys_time set to ",pic_sys_time," ");
  pic_dos_time = pic_itime[32];
  if(pic_icount<=pic_icount_od) pic_activate();
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
 
int pic_pending(int ilevel)
{
    return (pic_irr|pic_pirr|~pic_imr)&(1<<ilevel);
}
/* DANG_BEGIN_FUNCTION pic_activate
 * 
 * pic_activate requests any interrupts whose scheduled time has arrived.
 * anything after pic_dos_time and before pic_sys_time is activated.
 * pic_dos_time is advanced to the earliest time scheduled.
 * DANG_END_FUNCTION
 */
void pic_activate()
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
     if (interval < 100) {
       if (in_dpmi) run_dpmi();  /* we're surely out of int8 here */
     }
     if(pic_ltime[ilevel]==NEVER) {
	pic_itime[ilevel] = pic_itime[32] + interval;
     } else {
	pic_itime[ilevel] = pic_itime[ilevel] + interval;
     }
  }
  if (d.request > 2) {
    /* avoid going through sprintf for non-debugging */
    sprintf(mesg,", delay= %d.",interval);
    pic_print(2,"Scheduling lvl= ",ilevel,mesg);
    pic_print(2,"pic_itime set to ",pic_itime[ilevel],"");
  }
}

void pic_init(void)
{
  /* do any one-time initialization of the PIC */
  emu_iodev_t  io_device;

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
}

void pic_reset(void)
{
#if 0
  int i;

  pic_ilevel     = 32;
  pic_isr        = 0;
  pic_irq2_ivec  = 0;
  pic1_mask      = 0x07f8;
  pic0_imr       = 0xf800;
  pic1_imr       = 0x07f8;
  pic_imr        = 0xfff8;
  pice_imr       = 0xffffffff;
  pic_sp         = 0;
  pic_vm86_count = 0;
  pic_dpmi_count = 0;
  pic_sys_time   = NEVER;
  for (i = 0; i < 33; i++) {
    pic_ltime[i] = NEVER;
    pic_itime[i] = NEVER;
  }

  /* PIC reset on reboot and at bootup */
  pic_seti(PIC_IRQ0, timer_int_engine, 0);  /* do_irq0 in pic.c */
  pic_unmaski(PIC_IRQ0);
  pic_request(PIC_IRQ0);                    /* start timer */
  if (mice->intdrv || mice->type == MOUSE_PS2 || mice->type == MOUSE_IMPS2) {
    pic_seti(PIC_IMOUSE, DOSEMUMouseEvents, 0);
    pic_unmaski(PIC_IMOUSE);
  }
#ifdef USING_NET
#ifdef CONFIG_IPX
  pic_seti(PIC_IPX, IPXRelinquishControl, 0);
  pic_unmaski(PIC_IPX);
#endif
#ifdef CONFIG_IPXPICPKT
  pic_seti(PIC_NET, pkt_check_receive_quick, 0x61);
#else
  pic_seti(PIC_NET, pkt_check_receive_quick, 0);
#endif
  pic_unmaski(PIC_NET);
#endif
#endif
}

#undef set_pic0_imr
#undef set_pic1_imr
#undef get_pic0_imr
#undef get_pic1_imr
#undef get_pic0_isr
#undef get_pic1_isr
#undef get_pic0_irr
#undef get_pic1_irr
#undef NEVER

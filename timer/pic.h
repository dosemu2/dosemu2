#ifndef PIC
#define PIC
/* Priority Interrupt Controller Emulation 
   Copyright (C) 1994, J. Lawrence Stephan  jlarry@ssnet.com */

/* This software is distributed under the terms of the GNU General Public 
   License, there are NO WARRANTIES, expressed or implied.  Use at your own 
   risk.  Use of this software as a part of any product is permitted only 
   if that  product is also distributed under the terms of the GNU General 
   Public  License.  */
   


/* Definitions for 8259A programmable Interrupt Controller Emulation */
  

/* IRQ priority levels.  These are in order of priority, lowest number has
   highest priority.  Levels 16 - 30 are available for use, but currently
   have no fixed assignment.  They are extras for the convenience of dosemu.
   31 is tentatively reserved for a watchdog timer  (stall alarm).  
   
   IRQ2 does not exist.  A PCs IRQ2 line is actually connected to IRQ9.  The
   bios for IRQ9 does an int1a, then sends an EOI to the second PIC. ( This
   is simulated in the PIC code if IRQ9 points to the bios segment.)  For
   this reason there is no PIC_IRQ2. */
   
#define PIC_NMI   0        /*  non-maskable interrupt 0x02 */
#define PIC_IRQ0  1        /*  timer -    usually int 0x08 */
#define PIC_IRQ1  2        /*  keyboard - usually int 0x09 */
#define PIC_IRQ8  3        /*  real-time clock "  int 0x70 */
#define PIC_IRQ9  4        /*  int 0x71 revectors to  0x0a */
#define PIC_IRQ10 5        /*             usually int 0x72 */
#define PIC_IRQ11 6        /*             usually int 0x73 */
#define PIC_IRQ12 7        /*             usually int 0x74 */
#define PIC_IRQ13 8        /*             usually int 0x75 */
#define PIC_IRQ14 9        /*  disk       usually int 0x76 */
#define PIC_IRQ15 10       /*             usually int 0x77 */
#define PIC_IRQ3  11       /*  COM 2      usually int 0x0b */
#define PIC_IRQ4  12       /*  COM 1      usually int 0x0c */
#define PIC_IRQ5  13       /*  LPT 2      usually int 0x0d */
#define PIC_IRQ6  14       /*  floppy     usually int 0x0e */
#define PIC_IRQ7  15       /*  LPT 1      usually int 0x0f */

#define PIC_IRQALL 0xfffe  /*  bits for all IRQs set. This never changes  */

/* Some dos extenders modify interrupt vectors, particularly for IRQs 0-7 */

/* PIC "registers", plus a few more */

unsigned long pic_irr,          /* interrupt request register */
              pic_isr,          /* interrupt in-service register */
              pic1_isr,         /* second isr for pic1 irqs */
              pic_iflag,        /* interrupt enable flag: en-/dis- =0/0xfffe */
              pic_imr=-1,       /* interrupt mask register */
              pic0_imr,         /* interrupt mask register, pic0 */
              pic1_imr,         /* interrupt mask register, pic1 */
              pice_imr=-1,      /* interrupt mask register, dos emulator */
              pic_ilevel=32,    /* current interrupt level */
              pic_smm,          /* 32=>special mask mode, 0 otherwise */
              pic1_mask=0x07f8, /* bits set for pic1 levels */
              pic_icount,       /* iret counter (to avoid filling stack) */
              pic_pirr;         /* pending requests: ->irr when icount==0 */

/*  State flags.  picX_cmd is only read by debug output */

static unsigned char pic0_isr_requested; /* 0/1 =>next port 0 read=  irr/isr */
static unsigned char pic1_isr_requested;                     
static unsigned char pic0_icw_state; /* 0-3=>next port 1 write= mask,ICW2,3,4 */
static unsigned char pic1_icw_state;
static unsigned char pic0_cmd; /* 0-3=>last port 0 write was none,ICW1,OCW2,3*/
static unsigned char pic1_cmd;

/* IRQ definitions.  Each entry specifies the emulator routine to call, and
   the dos interrupt vector to use.  pic_iinfo.func is set by pic_seti(),
   as are the interrupt vectors for all but [1] through [15], which  may be
   changed by the ICW2 command from dos. (Some dos extenders do this.) */
   
struct lvldef {
       void (*func)();
       int    ivec;
       };
#define PNULL (void*)0
static struct lvldef pic_iinfo[32] = 
                     {{PNULL,0x02}, {PNULL,0x08}, {PNULL,0x09}, {PNULL,0x70},
                      {PNULL,0x71}, {PNULL,0x72}, {PNULL,0x73}, {PNULL,0x74},
                      {PNULL,0x75}, {PNULL,0x76}, {PNULL,0x77}, {PNULL,0x0b},
                      {PNULL,0x0c}, {PNULL,0x0d}, {PNULL,0x0e}, {PNULL,0x0f},
                      {PNULL,0x00}, {PNULL,0x00}, {PNULL,0x00}, {PNULL,0x00},
                      {PNULL,0x00}, {PNULL,0x00}, {PNULL,0x00}, {PNULL,0x00},
                      {PNULL,0x00}, {PNULL,0x00}, {PNULL,0x00}, {PNULL,0x00},
                      {PNULL,0x00}, {PNULL,0x00}, {PNULL,0x00}, {PNULL,0x00}};
#undef PNULL

int pic_irq2_ivec = 0;

/* function definitions - level refers to irq priority level defined above  */
/* port = 0 or 1     value = 0 - 0xff   level = 0 - 31    ivec = 0 - 0xff   */

void write_pic0(unsigned char port, unsigned char value); /* write to PIC 0 */
void write_pic1(unsigned char port, unsigned char value); /* write to PIC 1 */
unsigned char read_pic0(unsigned char port);             /* read from PIC 0 */
unsigned char read_pic1(unsigned char port);             /* read from PIC 1 */
void pic_unmaski(int level);                 /* clear dosemu's irq mask bit */
void pic_maski(int level);                   /*  set  dosemu's irq mask bit */
void pic_seti(unsigned int level, void (*func), unsigned int ivec); 
                                       /* set function and interrupt vector */
void run_irqs();                                      /* run requested irqs */
#define pic_run() if(pic_irr)run_irqs()   /* the right way to call run_irqs */
int do_irq();                                /* run dos portion of irq code */
void pic_request(int inum);                            /* interrupt trigger */
void pic_iret();                             /* interrupt completion notify */

/* The following are too simple to be anything but in-line */

#define pic_set_mask pic_imr=(pic0_imr|pic1_imr|pice_imr|pic_iflag)
#define pic_sti() pic_iflag=0;pic_set_mask          /*    emulate STI      */
#define pic_cli() pic_iflag=PIC_IRQALL;pic_set_mask /*    emulate CLI      */

#endif

#include "bitops.h"
#include "pic.h"
#define us unsigned
#include "memory.h"
#include "cpu.h"

extern int fatalerr;

/*  This code provides routines for several functions:

run_irqs()        checks for and runs any interrupts requested in pic_irr
do_irq()          runs the dos interrupt for the current irq
set_pic0_imr()  sets pic0 interrupt mask register       \\
set_pic1_imr()  sets pic1 interrupt mask register       ||
get_pic0_imr()  returns pic0 interrupt mask register    ||
get_pic1_imr()  returns pic1 interrupt mask register    || called by read_
get_pic0_isr()  returns pic0 in-service register        || and write_ picX
get_pic1_isr()  returns pic1 in-service register        ||
get_pic0_irr()  returns pic0 interrupt request register ||
get_pic1_irr()  returns pic1 interrupt request register ||
set_pic0_base() sets base interrupt for irq0 - irq7     ||
set_pic1_base() sets base interrupt for irq8 - irq15    //
write_pic0()    processes write to pic0 i/o 
write_pic1()    processes write to pic1 i/o
read_pic0()     processes read from pic0 i/o
read_pic1()     processes read from pic1 i/o
                                                                     */

#define set_pic0_imr(x) pic0_imr=pic0_to_emu(x);pic_set_mask
#define set_pic1_imr(x) pic1_imr=(((long)x)<<3);pic_set_mask
#define get_pic0_imr()  emu_to_pic0(pic0_imr)
#define get_pic1_imr()  (pic1_imr>>3)
#define get_pic0_isr()  emu_to_pic0(pic_isr)
#define get_pic1_isr()  (pic_isr>>3)
#define get_pic0_irr()  emu_to_pic0(pic_irr)
#define get_pic1_irr()  (pic_irr>>3)

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

void write_pic0(port,value)
unsigned char port,value;
{

/* if port == 0 this must be either an ICW1, OCW2, or OCW3 */
/* if port == 1 this must be either ICW2, ICW3, ICW4, or load IMR */

static char /* icw_state,              /* !=0 => port 1 does icw 2,3,(4) */
               icw_max_state;          /* number of icws expected        */

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
  else
    /* irqs on pic1 require an outb20 to each pic. we settle for any 2 */
    
    if((value&0xb8) == 0x20 && !clear_bit(pic_ilevel,&pic1_isr))     
          {clear_bit(pic_ilevel,&pic_isr);  /* the famous outb20 */
           pic0_cmd=2;}
  }
else                                /* icw2, icw3, icw4, or mask register */
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

void write_pic1(port,value)
unsigned char port,value;
{
/* if port == 0 this must be either an ICW1, OCW2, or OCW3 */
/* if port == 1 this must be either ICW2, ICW3, ICW4, or load IMR */
static char /* icw_state,       /* !=0 => port 1 does icw 2,3,(4) */
               icw_max_state;    /* number of icws expected        */

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
  else                                /* ocw2 */
    /* irqs on pic1 require an outb20 to each pic. we settle for any 2 */
    
    if((value&0xb8) == 0x20 && !clear_bit(pic_ilevel,&pic1_isr))
      {clear_bit(pic_ilevel,&pic_isr);  /* the famous outb20 */
       pic1_cmd=2;}
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

unsigned char read_pic0(port)
unsigned char port;
{
  if(port)               return((unsigned char)get_pic0_imr());
  if(pic0_isr_requested) return((unsigned char)get_pic0_isr());
                         return((unsigned char)get_pic0_irr());
}

unsigned char read_pic1(port)
unsigned char port;
{
  if(port)               return((unsigned char)get_pic1_imr());
  if(pic1_isr_requested) return((unsigned char)get_pic1_isr());
                         return((unsigned char)get_pic1_irr());
}

void pic_unmaski(level)
int level;
{
 if(pic_iinfo[level].func != (void*)0) clear_bit(level,&pice_imr);
 pic_set_mask;
}


void pic_maski(level)
int level;
{
  set_bit(level,&pice_imr);
}

void pic_seti(level,func,ivec)
unsigned int level,ivec;
void (*func);
{
 if(level>=32) return;
 pic_iinfo[level].func = func;
 if(level>16) pic_iinfo[level].ivec = ivec;
 if(func == (void*)0) pic_maski(level);
}

/* this is the guts if the irq processor. it should be called from an
  if:   if(pic_irr) run_irqs();   This will maximize speed  */

#if 0
/* see assembly language version below */
void run_irqs()
/* find the highest priority unmasked requested irq and run it */
{
 int old_ilevel;
 int int_request;

/* check for and find any requested irqs.  Having found one, we atomic-ly
   clear it and verify it was there when we cleared it.  If it wasn't, we
   look for the next request.  There are two in_service bits for pic1 irqs.
   This is needed, because irq 8-15 must do 2 outb20s, which, if the dos
   irq code actually runs, will reset the bits.  We also reset them here,
   since dos code won't necessarily run.  */
   
 if(!(pic_irr&~(pic_isr|pic_imr))) return;     /* exit if nothing to do */
 old_ilevel=pic_ilevel;                          /* save old pic_ilevl   */
 
 while(int_request= pic_irr&~(pic_isr|pic_imr)) /* while something to do*/
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

#endif

/* I got inspired.  Here's an assembly language version, somewhat faster. */
/* This is also much shorter - fewer checks needed, because there's no
   question of compiler behavior, and less code because we can use both 
   results and flags from an instruction . */
/* RE-ENTRANT CODE - uses 16 bytes of stack */
void run_irqs()
{
  __asm__ __volatile__
  ("movl _pic_ilevel,%%ecx\n\t"      /* get old ilevel                  */
   "pushl %%ecx\n\t"                 /* save old ilevel                 */ 
   "addl _pic_smm,%%ecx\n\t"         /* check special mask mode         */
                                     /***       while(.....){           */
   "L1:movl _pic_isr,%%eax\n\t"      /* 1: int_request = (pic_isr       */ 
   "orl _pic_imr,%%eax\n\t"          /* | pic_imr)                      */ 
   "notl %%eax\n\t"                  /* int_request = ~int_request      */ 
   "andl _pic_irr,%%eax\n\t"         /* int_request &= pic_irr          */ 
   "jz L3\n\t"                       /* if(!int_request) goto 3         */ 
   "L2:bsfl %%eax,%%ebx\n\t"         /* 2: ebx=find_bit(int_request)    */ 
   "jz L3\n\t"                       /* if(!int_request) goto 3         */ 
   "cmpl %%ebx,%%ecx\n\t"            /* if(ebx > pic_ilevel)...         */ 
   "jl L3\n\t"                       /* ... goto 3                      */ 
   "btrl %%ebx,_pic_irr\n\t"         /* clear_bit(ebx,&pic_irr)         */ 
   "jz L2\n\t"                       /* if bit wasn't set, go back to 2 */ 
   "movl _pic_isr,%%ecx\n\t"         /* get current pic_isr             */
   "btsl %%ebx,%%ecx\n\t"            /* set bit in pic_isr              */ 
   "movl %%ebx,_pic_ilevel\n\t"      /* set new ilevel                  */
   "movl %%ecx,_pic_isr\n\t"         /* save new isr                    */
   "andl _pic1_mask,%%ecx\n\t"       /* isolate pic1 irqs               */
   "movl %%ecx,_pic1_isr\n\t"        /* save in pic1_isr                */
   "call *_pic_iinfo(,%%ebx,8)\n\t"  /* call interrupt handler          */
   "movl _pic_ilevel,%%eax\n\t"      /* get new ilevel                  */
   "btrl %%eax,_pic_isr\n\t"         /* reset isr bit - just in case    */
   "btrl %%eax,_pic1_isr\n\t"        /* reset isr bit - just in case    */
   "jmp L1\n\t"                      /* go back for next irq            */
                                     /**** end of while   }          ****/ 
   "L3:popl _pic_ilevel\n\t"         /* 3: restore old ilevel and exit  */ 
   :                                 /* no output                       */ 
   :                                 /* no input                        */ 
   :"eax","ebx","ecx");              /* registers eax, ebx, ecx used    */
 } 
               
   

/*  do_irq() calls the correct do_int().
    It then executes a vm86 loop until an outb( end-of-interrupt) is found.
    For priority levels 0 and >15 (not real IRQs), vm86 executes once, then 
    returns, since no outb20 will come.
    Returns: 0 = complete, 1 = interrupt not run because it directly
    calls our "bios"   See run_timer_tick() in timer.c for an example
!!! This routine is RE-ENTRANT - it calls run_irqs, which
    which may call an interrupt routine,
    which may call do_irq().  Be Careful!  !!!!!!!!!!!!!!!!!!  */

int do_irq()
{
 int intr;
 long int_ptr;
 
    intr=pic_iinfo[pic_ilevel].ivec;
    if(!pic_ilevel) return;

#ifndef PICTEST
    if(pic_ilevel==PIC_IRQ9)      /* unvectored irq9 just calls int 0x1a.. */
      if(!IS_REDIRECTED(intr)) {intr=0x1a;pic1_isr&= 0xffef;} /* & one EOI */
    if(IS_REDIRECTED(intr))
    {
#endif

     pic_cli();
     run_int(intr);
      while(!fatalerr && test_bit(pic_ilevel,&pic_isr))
      {
        run_vm86();
        pic_isr &= PIC_IRQALL;    /*  levels 0 and 16-31 are Auto-EOI  */
        run_irqs();
        serial_run();           /*  delete when moved to timer stuff */
        int_queue_run();        /*  delete when moved to timer stuff */
      }
#if 0
      count_time; 
#endif
      pic_sti();
      return(0);

#ifndef PICTEST
    }   /* else */ 
#if 0
    count_time; 
#endif
    return(1);
#endif
}
void
pic_request(inum)
int inum;
{
 if(pic_isr&&(1<<inum)) { ++pic_icount;pic_pirr|=(1<<inum);}
 else pic_irr|=(1<<inum);
 return;
}

void
pic_iret()
{
 if(pic_icount)
   if(!(--pic_icount)){
     pic_irr|=pic_pirr;
     pic_pirr=0;
   }
 return;
}
 
   

#undef set_pic0_imr(x)
#undef set_pic1_imr(x)
#undef get_pic0_imr()
#undef get_pic1_imr()
#undef get_pic0_isr()
#undef get_pic1_isr()
#undef get_pic0_irr()
#undef get_pic1_irr()

/* CPU/V86 support for dosemu
 * much of this code originally written by Matthias Lautner
 * taken over by:
 *          Robert Sanders, gt8134b@prism.gatech.edu 
 *
 * $Date: 1993/11/30 22:21:03 $
 * $Source: /home/src/dosemu0.49pl3/RCS/cpu.c,v $
 * $Revision: 1.8 $
 * $State: Exp $
 *
 * $Log: cpu.c,v $
 * Revision 1.8  1993/11/30  22:21:03  root
 * Final Freeze for release pl3
 *
 * Revision 1.7  1993/11/30  21:26:44  root
 * Chips First set of patches, WOW!
 *
 * Revision 1.6  1993/11/29  22:44:11  root
 * Prepare for pl3 release
 *
 * Revision 1.5  1993/11/29  00:05:32  root
 * Add code to start on Diamond card, get rid (temporarily) of sig cd,80,85
 *
 * Revision 1.4  1993/11/25  22:45:21  root
 * About to destroy keybaord routines.
 *
 * Revision 1.3  1993/11/23  22:24:53  root
 * *** empty log message ***
 *
 * Revision 1.2  1993/11/15  19:56:49  root
 * Fixed sp -> ssp overflow, it is a hack at this time, but it works.
 *
 * Revision 1.1  1993/11/12  12:32:17  root
 * Initial revision
 *
 * Revision 1.3  1993/07/21  01:52:44  rsanders
 * implemented {push,pop}_{byte,word,long}, and push_ifs()/
 * pop_ifs() for playing with interrupt stack frames.
 *
 * Revision 1.2  1993/07/13  19:18:38  root
 * changes for using the new (0.99pl10) signal stacks
 *
 *
 */
#define CPU_C

#include "config.h"
#include "memory.h"
#include "cpu.h"
#include "emu.h"

struct vm86_struct vm86s;
struct CPU cpu;

struct vec_t orig[256];		/* "original" interrupt vectors */
struct vec_t snapshot[256];	/* vectors from last snapshot */

extern int in_sigsegv, in_sighandler, ignore_segv, in_interrupt;
extern int fatalerr;

int in_vm86=0;

inline void 
update_cpu(long flags)
{
  cpu.iflag = (flags&IF)  ? 1 : 0;
  cpu.nt    = (flags&NT)  ? 1 : 0;
  cpu.ac    = (flags&AC)  ? 1 : 0;

  cpu.iopl  = flags&IOPL_MASK;
}


inline void 
update_flags(long *flags)
{
#define SETFLAG(bit)  (*flags |=  (bit))
#define CLRFLAG(bit)  (*flags &= ~(bit))

  cpu.iflag ? SETFLAG(IF)  : CLRFLAG(IF);

  /* on a 386, the AC (alignment check) bit doesn't exist */
  if (cpu.type == CPU_486 && cpu.ac)
    SETFLAG(AC);
  else
    CLRFLAG(AC);

  *flags &= ~IOPL_MASK;		/* clear IOPL */

  /* the 80286 allowed one to set bit 15 of the flags register,
   * (so says HelpPC 2.1...but INFOPLUS uses the test below)
   * 80286 in real mode keeps the upper 4 bits of FLAGS zeroed.  this
   * test works with the programs I've tried, while setting bit15 
   * does NOT.
   */
 if (cpu.type == CPU_286) *flags &= ~0xfffff000;
 else {
   *flags |= cpu.iopl;
   cpu.nt ? SETFLAG(NT) : CLRFLAG(NT);
 }
}


void 
cpu_init(void)
{
  /* cpu.type set in emulate() via getopt */
  cpu.iflag=1;
  cpu.nt=0;
  cpu.iopl=0;
  cpu.ac=0;
  cpu.sti=1;

  update_flags(&REG(eflags));

  /* make ivecs array point to low page (real mode IDT) */
  ivecs=0;
}



void 
show_regs(void)
{
  int i;
  unsigned char *cp = SEG_ADR((unsigned char *), cs, ip);
  unsigned long vflags=REG(eflags);

  /* adjust vflags with our virtual IF bit (iflag) */
  update_flags(&vflags);
  
  dbug_printf("\nEIP: %04x:%08lx",LWORD(cs),REG(eip));
  dbug_printf(" ESP: %04x:%08lx",LWORD(ss),REG(esp));
  dbug_printf("         VFLAGS(b): ");
  for (i=(1 << 0x11); i > 0; i=(i >> 1))
      dbug_printf( (vflags&i) ? "1" : "0");

  dbug_printf("\nEAX: %08lx EBX: %08lx ECX: %08lx EDX: %08lx VFLAGS(h): %08lx",
	      REG(eax),REG(ebx),REG(ecx),REG(edx), vflags);
  dbug_printf("\nESI: %08lx EDI: %08lx EBP: %08lx",
	      REG(esi), REG(edi), REG(ebp));
  dbug_printf(" DS: %04x ES: %04x FS: %04x GS: %04x\n",
       LWORD(ds), LWORD(es), LWORD(fs), LWORD(gs));

  /* display vflags symbolically...the #f "stringizes" the macro name */
#define PFLAG(f)  if (vflags&(f)) dbug_printf(#f" ")

  dbug_printf("FLAGS: ");
  PFLAG(CF);  PFLAG(PF);  PFLAG(AF);  PFLAG(ZF);  
  PFLAG(SF);  PFLAG(TF);  PFLAG(IF);  PFLAG(DF);  
  PFLAG(OF);  PFLAG(NT);  PFLAG(RF);  PFLAG(VM);
  PFLAG(AC); 
  dbug_printf(" IOPL: %u\n", (unsigned)((vflags&IOPL_MASK) >> 12));

  /* display the 10 bytes before and after CS:EIP.  the -> points
   * to the byte at address CS:EIP
   */
  dbug_printf("OPS: ");
  cp -= 10; 
  for (i=0; i<10; i++)
    dbug_printf("%02x ", *cp++);
  dbug_printf("-> ");
  for (i=0; i<10; i++)
    dbug_printf("%02x ", *cp++);
  dbug_printf("\n");
}


void 
show_ints(int min, int max)
{
  int i, b;

  max = (max-min)/3;
  for (i=0, b=min; i<=max; i++, b+=3)
    {
      dbug_printf("%02x| %04x:%04x->%05x    ",b,ISEG(b), IOFF(b),
		  IVEC(b));
      dbug_printf("%02x| %04x:%04x->%05x    ",b+1,ISEG(b+1), IOFF(b+1),
		  IVEC(b+1));
      dbug_printf("%02x| %04x:%04x->%05x\n",b+2,ISEG(b+2),IOFF(b+2),
		  IVEC(b+2));
    }
}


inline int 
do_hard_int(int intno) {
#if AJT
  queue_hard_int(intno,NULL,NULL);
  return(1);
#else
  if (cpu.iflag) { do_int(intno); return 1; }
  else return 0;
#endif
}


inline int
do_soft_int(int intno) 
{
  do_int(intno); 
  return 1;
}


inline void 
do_sti(void)
{
  cpu.sti=1;
  REG(eflags)|=TF;
}




/* this is the brain, the privileged instruction handler.
 * see cpu.h for the SIGSTACK definition (new in Linux 0.99pl10)
 */
void 
sigsegv(SIGSTACK)
{
  us *ssp;
  unsigned char *csp, *lina;
  static int haltcount=0;
  int op32=0, ad32=0, i;

#define MAX_HALT_COUNT 1

  if (ignore_segv)
    {
      error("ERROR: sigsegv ignored!\n");
      return;
    }

  if (! in_vm86) {
  csp = (char *)eip;

/* This has been added temporarily as most illegal sigsegv's are attempt
   to call Linux int routines */

  if (!(csp[-2] == 0xcd && csp[-1] == 0x80 && csp[0] == 0x85))
    {
      error("ERROR: NON-VM86 sigsegv!\n"
     "eip: 0x%08lx  esp: 0x%08lx  eflags: 0x%lx\n"
     "cs: 0x%04lx  ds: 0x%04lx  es: 0x%04lx  ss: 0x%04lx\n",
     eip, esp, eflags, cs, ds, es, ss);

      dbug_printf("  VFLAGS(b): ");
      for (i = (1 << 17); i; i >>= 1)
 dbug_printf((eflags&i) ? "1" : "0");
      dbug_printf("\n");
      
      dbug_printf("EAX: %08lx  EBX: %08lx  ECX: %08lx  EDX: %08lx"
    "  VFLAGS(h): %08lx\n",
    eax, ebx, ecx, edx, eflags);
      dbug_printf("ESI: %08lx  EDI: %08lx  EBP: %08lx\n",
		  esi, edi, ebp);
      dbug_printf("CS: %04lx  DS: %04lx  ES: %04lx  FS: %04lx  GS: %04lx\n",
    cs, ds, es, fs, gs);
      
      /* display vflags symbolically...the #f "stringizes" the macro name */
#undef PFLAG
#define PFLAG(f)  if (eflags&(f)) dbug_printf(" " #f)
      
      dbug_printf("FLAGS:");
      PFLAG(CF);  PFLAG(PF);  PFLAG(AF);  PFLAG(ZF);  
      PFLAG(SF);  PFLAG(TF);  PFLAG(IF);  PFLAG(DF);  
      PFLAG(OF);  PFLAG(NT);  PFLAG(RF);  PFLAG(VM);
      PFLAG(AC); 
      dbug_printf("  IOPL: %u\n", (unsigned)((eflags&IOPL_MASK) >> 12));
      
      /* display the 10 bytes before and after CS:EIP.  the -> points
       * to the byte at address CS:EIP
       */
      dbug_printf("OPS: ");
      csp = (unsigned char *)eip - 10;
      for (i=0; i<10; i++)
 dbug_printf("%02x ", *csp++);
      dbug_printf("-> ");
      for (i=0; i<10; i++)
 dbug_printf("%02x ", *csp++);
      dbug_printf("\n");
      
    }
  }

  if (in_sigsegv)
    error("ERROR: in_sigsegv=%d!\n", in_sigsegv);

  in_vm86 = 0;
  in_sigsegv++;
  
  /* In a properly functioning emulator :-), sigsegv's will never come
   * while in a non-reentrant system call (ioctl, select, etc).  Therefore,
   * there's really no reason to worry about them, so I say that I'm NOT
   * in a signal handler (I might make this a little clearer later, to
   * show that the purpose of in_sighandler is to stop non-reentrant system
   * calls from being reentered.
   * I reiterate: sigsegv's should only happen when I'm running the vm86
   * system call, so I really shouldn't be in a non-reentrant system call
   * (except maybe vm86)
   */
  in_sighandler=0;

  lina=SEG_ADR((unsigned char *), cs, ip);

  if (REG(eflags) & TF) {
    g_printf("SIGSEGV %d received\n", sig);
    show_regs(); 
  }

restart_segv:
  csp = SEG_ADR((unsigned char *), cs, ip);

/* printf("CSP in cpu is 0x%04x\n", *csp); */

  switch (*csp) 
    {

    case 0x62: /* bound */
      error("ERROR: BOUND instruction");
      show_regs();
      REG(eip) += 2;  /* check this! */
      do_int(5); 
      break;
      
    case 0x66: /* 32-bit operand prefix */
      op32=1;
      REG(eip)++;
      goto restart_segv;
      break;
    case 0x67: /* 32-bit address prefix */
      ad32=1;
      REG(eip)++;
      goto restart_segv;
      break;
      
    case 0x6c: /* insb */
    case 0x6d: /* insw */
    case 0x6e: /* outsb */
    case 0x6f: /* outsw */
      error("ERROR: IN/OUT SB/SW: 0x%02x\n",*csp);
      REG(eip)++;
      break;
      
    case 0xcd: /* int xx */
      REG(eip) += 2;
      do_int((int)*++csp);
      break;
    case 0xcc: /* int 3 */
      REG(eip) += 1;
      do_int(3);
      break;
      
    case 0xcf: /* iret */
      ssp = SEG_ADR((us *), ss, sp);
      REG(eip) = *ssp++;
      REG(cs) = *ssp++;
      REG(eflags) = (REG(eflags) & 0xffff0000) | *ssp++;
      REG(esp) += 6;
      
      /* make sure our "virtual flags" correspond to the
       * popped eflags
       */
      update_cpu(REG(eflags));

#if AJT
      int_end();
#endif
      /* decrease the interrupt depth count */
      in_interrupt--;
      break;
      
    case 0xe5: /* inw xx */
      REG(eax) &= ~0xff00;
      REG(eax) |= inb((int)csp[1] +1) << 8;
    case 0xe4: /* inb xx */
      REG(eax) &= ~0xff;
      REG(eax) |= inb((int)csp[1]);
      REG(eip) += 2;
      break;
      
    case 0xed: /* inw dx */
      REG(eax) &= ~0xff00;
      REG(eax) |= inb(REG(edx) +1) << 8;
    case 0xec: /* inb dx */
      REG(eax) &= ~0xff;
      REG(eax) |= inb(REG(edx));
      REG(eip) += 1;
      break;
      
    case 0xe7: /* outw xx */
      outb((int)csp[1] +1, HI(ax));
    case 0xe6: /* outb xx */
      outb((int)csp[1], LO(ax));
      REG(eip) += 2;
      break;
      
    case 0xef: /* outw dx */
      outb(REG(edx) +1, HI(ax));
    case 0xee: /* outb dx */
      outb(REG(edx), LO(ax));
      REG(eip) += 1;
      break;
      
/* Emulate REP F3 6F */
  case 0xf3:
    printf("Checking for REP F3 6F\n");
    if ( (csp[1] & 0xff) == 0x6f){
	u_char *si=SEG_ADR((unsigned char *), ds, si);
	printf("Doing REP F3 6F\n");
	show_regs();
	while (_regs.ecx) {
		printf("REG(ecx)=0x%08x, *si=0x%04x, REG(edx)=0x%08x\n", REG(ecx), *si, REG(edx));
		if ((REG(edx) >= 0x23c0) && (REG(edx) <= 0x23cf)) iopl(3);
		port_out(*si,REG(edx));
		if ((REG(edx) >= 0x23c0) && (REG(edx) <= 0x23cf)) iopl(3);
		REG(ecx)--;
		si++;
	}
	printf("Finished REP F3 6F\n");
	REG(eip)++;
	REG(eip)++;
	_regs.eflags |= CF;
	break;
    }
    else printf("Nope CSP[1] = 0x%04x\n", csp[1]);
      
    case 0xfa: /* cli */
      cpu.iflag = 0;
      REG(eip) += 1;
      break;
    case 0xfb: /* sti */
#ifdef PROPER_STI
      do_sti();
#else
      cpu.iflag = 1;
#endif
      REG(eip) += 1;
      break;
    case 0x9c: /* pushf */
      /* fetch virtual CPU flags into eflags */
      update_flags(&REG(eflags));
      if (op32)
	{
	  unsigned long *lssp;
	  op32=0;
	  lssp = SEG_ADR((unsigned long *), ss, sp);
	  *--lssp = (unsigned long)REG(eflags);
	  REG(esp) -= 4;
	  REG(eip) += 1;
	} else {
	  ssp = SEG_ADR((us *), ss, sp);
	  *--ssp = (us)REG(eflags);
	  REG(esp) -= 2;
	  REG(eip) += 1;
	}
      break;
      
    case 0x9d: /* popf */
      if (op32)
	{
	  unsigned long *lssp;
	  op32=0;
	  lssp = SEG_ADR((unsigned long *), ss, sp);
	  REG(eflags) = *lssp;
	  REG(esp) += 4;
	} else {
	  ssp = SEG_ADR((us *), ss, sp);
	  REG(eflags) &= ~0xffff;
	  REG(eflags) |= (u_short)*ssp;
	  REG(esp) += 2;
	}
      REG(eip) += 1;
      /* make sure our "virtual flags" correspond to the
       * popped eflags
       */
      update_cpu(REG(eflags));
      break;
      
    case 0xf4: /* hlt...I use it for various things, 
		  like trapping direct jumps into the XMS function */
      if (lina == (unsigned char *)XMSTrap_ADD) 
	{
	  REG(eip)+=2;  /* skip halt and info byte to point to FAR RET */
	  xms_control();
	} 
#if BIOSSEG != 0xf000
      else if ((REG(cs) & 0xffff) == 0xf000)
	{
	  /* if BIOSSEG = 0xf000, then jumps here will be legit */
	  error("jump into BIOS %04x:%04x...simulating IRET\n",
		REG(cs), LWORD(eip));
	  /* simulate IRET */
	  ssp = SEG_ADR((us *), ss, sp);
	  REG(eip) = *ssp++;
	  REG(cs) = *ssp++;
	  REG(eflags) = (REG(eflags) & 0xffff0000) | *ssp++;
	  (us)REG(esp) += 6;	
	} 
#endif
      else
	{
   error("HLT requested: lina=%p!\n", lina);
	  show_regs();
	  haltcount++;
	  if (haltcount > MAX_HALT_COUNT) fatalerr=0xf4;
	  REG(eip) += 1;
	}
      break;
      
    case 0xf0: /* lock */
    default:
      /* er, why don't we advance eip here, and
	 why does it work??  */
      error("general protection %x\n", *csp);
      show_regs();
      show_ints(0,0x33);
      error("ERROR: SIGSEGV, protected insn...exiting!\n");
      fatalerr = 4;
      leavedos(fatalerr);  /* shouldn't return */
      _exit(1000);
    } /* end of switch() */	
  

#ifndef PROPER_STI  /* do_sti() sets TF bit */
  if (REG(eflags) & TF) {
    g_printf("TF: trap done");
    show_regs(); 
  }
#endif
  
  in_sigsegv--;
  in_sighandler=0;
}



void 
sigill(SIGSTACK)
{
  unsigned char *csp;
  int i, dee;

  if (! in_vm86) /* test VM bit */
      error("ERROR: NON-VM86 illegal insn!\n");

  in_vm86 = 0;

  error("SIGILL %d received\n", sig);
  show_regs();
  csp = SEG_ADR((unsigned char *), cs, ip);

/* Some db commands start with 2e (use cs segment) and thus is accounted 
   for here */

  if (csp[0] == 0x2e){
	csp++;
	_regs.eip++;
  }
  if (csp[0] == 0xf0)
    {
      dbug_printf("ERROR: LOCK prefix not permitted!\n");
      REG(eip)++;
      return;
    }

/* look at this with Checkit...the new code just sits in a loop */
#define OLD_MATH_CODE 
#ifdef OLD_MATH_CODE
  i = (csp[0] << 8) + csp[1]; /* swapped */
  if ((i & 0xf800) != 0xd800) { /* not FPU insn */
    error("ERROR: not an FPU instruction, real illegal opcode!\n");
#if 0
    do_int(0x10);	/* Coprocessor error */
#else
    fatalerr = 4;
#endif
    return;
  }

  /* I don't know what this code does. -Robert */
  switch(i & 0xc0) {
  case 0x00:
    if ((i & 0x7) == 0x6) {
      dee = *(short *)(csp +2);
      REG(eip) += 4;
    } else {
      REG(eip) += 2;
      dee = 0;
    }
    break;
  case 0x40:
    dee = (signed)csp[2];
    REG(eip) += 3;
    break;
  case 0x80:
    dee = *(short *)(csp +2);
    REG(eip) += 4;
    break;
  default:
    REG(eip) += 2;
    dee = 0;
  }

  warn("MATH: emulation %x d=%x\n", i, dee);

#else  /* this is the new, stupid MATH-EMU code. it doesn't work. */

  if ((*csp >= 0xd8) && (*csp <= 0xdf))  /* the FPU insn prefix bytes */
    {
      error("MATH: math emulation for (first 2 bytes) %02x %02x...\n",
	    *csp, *(csp+1));

      /* this is the coprocessor-not-available int. to use this, you
       * should compile your kernel with FPU-emu support in, and you
       * should install a DOS-based FPU emulator within dosemu.
       * this is untested, and I CAN'T test it, as I have a 486.
       */
      do_int(7);  
    }
#endif
}


void 
sigfpe(SIGSTACK)
{
  if (! in_vm86)
      error("ERROR: NON-VM86 SIGFPE insn!\n");

  in_vm86 = 0;

  error("SIGFPE %d received\n", sig);
  show_regs();
  if (cpu.iflag) do_int(0); 
  else error("ERROR: FPE happened but interrupts disabled\n");
}



void 
sigtrap(SIGSTACK)
{
  in_vm86 = 0;

#ifdef PROPER_STI
  if (cpu.sti)
    {
      cpu.iflag=1;
      cpu.sti=0;
      REG(eflags) &= ~(TF);
      return;
    }
#endif

  if (REG(eflags) & TF)  /* trap flag */
    REG(eip)++;
  
  do_int(3);
}

#if AJT

struct port_struct
{
  int start;
  int size;
  int permission;
  int ormask,andmask;
} *ports = NULL;
int num_ports = 0;

/* find a matching entry in the ports array */
int 
find_port(int port,int permission)
{
  static int last_found = 0;
  int i;
  for (i=last_found;i<num_ports;i++)
    if (port >= ports[i].start &&
	port < (ports[i].start + ports[i].size) &&
	((ports[i].permission & permission) == permission))
      {
	last_found = i;
	return(i);
      }

  for (i=0;i<last_found;i++)
    if (port >= ports[i].start &&
	port < (ports[i].start + ports[i].size) &&
	((ports[i].permission & permission) == permission))
      {
	last_found = i;
	return(i);
      }
  return(-1);
}

/* control the io permissions for ports */
int 
allow_io(int start,int size,int permission,int ormask,int andmask)
{

  /* first the simplest case...however, this simplest case does
   * not apply to ports above 0x3ff--the maximum ioperm()able
   * port under Linux--so we must handle some of that ourselves.
   */
  if (permission == IO_RDWR && (ormask == 0 && andmask == 0xFFFF)) 
    {
      if ((start + size - 1) <= 0x3ff)
	{
	  c_printf("giving hardware access to ports 0x%x -> 0x%x\n",
		   start,start+size-1);
	  set_ioperm(start,size,1);
	  return(1);
	}

      /* allow direct I/O to all the ports that *are* below 0x3ff 
       * and add the rest to our list
       */
      else if (start <= 0x3ff)
	{
	  warn("PORT: s-s: %d %d\n", start, size);
	  set_ioperm(start, 0x400 - start, 1);
	  size = start + size - 0x400;
	  start = 0x400;
	}
    }


  /* we'll need to add an entry */
  if (ports)
    ports = (struct port_struct *)
      realloc(ports,sizeof(struct port_struct)*(num_ports+1));
  else
    ports = (struct port_struct *)
      malloc(sizeof(struct port_struct)*(num_ports+1));
  num_ports++;
  if (!ports)
    {
      error("allocation error for ports!\n");
      num_ports = 0;
      return(0);
    }

  ports[num_ports-1].start = start;
  ports[num_ports-1].size = size;
  ports[num_ports-1].permission = permission;
  ports[num_ports-1].ormask = ormask;
  ports[num_ports-1].andmask = andmask;
      
  c_printf("added port %x size=%d permission=%d ormask=%x andmask=%x\n",
	   start,size,permission,ormask,andmask);      

  return(1);
}

int 
port_readable(int port)
{
  return(find_port(port,IO_READ) != -1);
}


int 
port_writeable(int port)
{
  return(find_port(port,IO_WRITE) != -1);
}


int read_port(int port)
{
  int r;
  int i = find_port(port,IO_READ);
  if (i == -1) return(0);

  if (port <= 0x3ff)
    set_ioperm(port,1,1);
  else
    iopl(3);

  r = port_in(port);

  if (port <= 0x3ff)
    set_ioperm(port,1,0);
  else
    iopl(0);

  r &= ports[i].andmask;
  r |= ports[i].ormask;
  h_printf("read port 0x%x gave %02x at %04x:%04x\n",
    port, r, LWORD(cs), LWORD(eip));
  return(r);
}

int write_port(int value,int port)
{
  int i = find_port(port,IO_WRITE);
  if (i == -1) return(0);

  value &= ports[i].andmask;
  value |= ports[i].ormask;

  h_printf("write port 0x%x value %02x at %04x:%04x\n",
    port, value, LWORD(cs), LWORD(eip));

  if (port <= 0x3ff)
    set_ioperm(port,1,1);
  else
    iopl(3);

  port_out(value,port);
  
  if (port <= 0x3ff)
    set_ioperm(port,1,0);
  else
    iopl(0);

  return(1);
}

#endif


#if AJT

int monitor = 0;
int mcs,mip;

int_start(int i)
{
#if 0
  int ah = (REG(eax)>>8)&0xFF;
  int al = REG(eax)&0xFF;
  if ((i == 0x2f && ah == 0x11 && (al == 0x17 || al == 0x18)) ||
      (i == 0x21 && ah == 0x3c))
    {
      monitor = 1;
      mcs = REG(cs);
      mip = REG(eip);
      d_printf("INTSTART(%x)\n",i);
      show_regs();
    }
#endif
}

int_end()
{
  if (monitor && REG(cs) == mcs && REG(eip) == mip)
    {
      d_printf("INTEND\n");
      show_regs();
      d_printf("\n");
      monitor = 0;
    }
}
#endif

char
pop_byte(struct vm86_regs *pregs)
{
  char *stack_byte = (char *)((pregs->ss << 4) + pregs->esp);
  pregs->esp += 1;
  return *stack_byte;
}

short
pop_word(struct vm86_regs *pregs)
{
  short *stack_word = (short *)((pregs->ss << 4) + pregs->esp);
  pregs->esp += 2;
  return *stack_word;
}

long
pop_long(struct vm86_regs *pregs)
{
  long *stack_long = (long *)((pregs->ss << 4) + pregs->esp);
  pregs->esp += 4;
  return *stack_long;
}


void
push_byte(struct vm86_regs *pregs, char byte)
{
  pregs->esp -= 1;
  *(char *)((pregs->ss << 4) + pregs->esp) = byte;
}

void
push_word(struct vm86_regs *pregs, short word)
{
  pregs->esp -= 2;
  *(short *)((pregs->ss << 4) + pregs->esp) = word;
}

void
push_long(struct vm86_regs *pregs, long longword)
{
  pregs->esp -= 4;
  *(long *)((pregs->ss << 4) + pregs->esp) = longword;
}


interrupt_stack_frame
pop_isf(struct vm86_regs *pregs)
{
  interrupt_stack_frame isf;

  show_regs();
  isf.eip = pop_word(pregs);
  isf.cs = pop_word(pregs);
  isf.flags = pop_word(pregs);
  error("cs: 0x%x, eip: 0x%x, flags: 0x%x\n", isf.cs, isf.eip,
	 isf.flags);
}


void
push_isf(struct vm86_regs *pregs, interrupt_stack_frame isf)
{
  push_word(pregs, isf.flags);
  push_word(pregs, isf.cs);
  push_word(pregs, isf.eip);
}

#undef CPU_C


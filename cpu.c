/* CPU/V86 support for dosemu
 * much of this code originally written by Matthias Lautner
 * taken over by:
 *          Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1994/06/12 23:15:37 $
 * $Source: /home/src/dosemu0.60/RCS/cpu.c,v $
 * $Revision: 2.1 $
 * $State: Exp $
 *
 * $Log: cpu.c,v $
 * Revision 2.1  1994/06/12  23:15:37  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.49  1994/05/26  23:15:01  root
 * Prep. for pre51_21.
 *
 * Revision 1.48  1994/05/24  01:23:00  root
 * Lutz's latest, int_queue_run() update.
 *
 * Revision 1.47  1994/05/21  23:39:19  root
 * PRE51_19.TGZ with Lutz's latest updates.
 *
 * Revision 1.46  1994/05/16  23:13:23  root
 * Prep for pre51_16.
 *
 * Revision 1.45  1994/05/13  17:21:00  root
 * pre51_15.
 *
 * Revision 1.44  1994/05/10  23:08:10  root
 * pre51_14.
 *
 * Revision 1.43  1994/05/04  21:56:55  root
 * Prior to Alan's mouse patches.
 *
 * Revision 1.42  1994/04/30  22:12:30  root
 * Prep for pre51_11.
 *
 * Revision 1.41  1994/04/30  01:05:16  root
 * Lutz's Latest
 *
 * Revision 1.40  1994/04/29  23:52:06  root
 * Prior to Lutz's latest 94/04/29.
 *
 * Revision 1.39  1994/04/27  23:42:52  root
 * Lutz's patches to get dosemu up under 1.1.9. and Jochen's Cleanups
 *
 * Revision 1.37  1994/04/20  21:05:01  root
 * Prep for Rob's patches to linpkt...
 *
 * Revision 1.36  1994/04/18  22:52:19  root
 * Ready pre51_7.
 *
 * Revision 1.35  1994/04/18  20:57:34  root
 * Checkin prior to Jochen's latest patches.
 *
 * Revision 1.34  1994/04/16  01:28:47  root
 * Prep for pre51_6.
 *
 * Revision 1.33  1994/04/13  00:07:09  root
 * Lutz's patches
 * ,
 *
 * Revision 1.32  1994/04/04  22:51:55  root
 * Better checks for allowvideoportaccess.
 *
 * Revision 1.31  1994/03/30  22:12:30  root
 * Prep for 0.51 pre 2.
 *
 * Revision 1.30  1994/03/23  23:24:51  root
 * Prepare to split out do_int.
 *
 * Revision 1.29  1994/03/18  23:17:51  root
 * Some DPMI, some prep for 0.50pl1.
 *
 * Revision 1.28  1994/03/15  01:38:20  root
 * DPMI,serial, other changes.
 *
 * Revision 1.27  1994/03/14  00:35:44  root
 * Unsucessful speed enhancements
 *
 * Revision 1.26  1994/03/13  21:52:02  root
 * More speed testing :-(
 *
 * Revision 1.25  1994/03/13  01:07:31  root
 * Poor attempts to optimize.
 *
 * Revision 1.24  1994/03/10  23:52:52  root
 * Lutz DPMI patches
 *
 * Revision 1.23  1994/03/04  15:23:54  root
 * Run through indent.
 *
 * Revision 1.22  1994/03/04  00:01:58  root
 * Readying for 0.50
 *
 * Revision 1.21  1994/02/21  20:28:19  root
 * DPMI update by Lutz.
 *
 * Revision 1.20  1994/02/09  20:10:24  root
 * Added dosbanner config option for optionally displaying dosemu bannerinfo.
 * Added allowvideportaccess config option to deal with video ports.
 *
 * Revision 1.19  1994/02/05  21:45:55  root
 * Changed all references within ssp's from !REG to !LWORD.
 *
 * Revision 1.18  1994/01/27  19:43:54  root
 * Preparing for IPX of Tim's.
 *
 * Revision 1.17  1994/01/25  20:02:44  root
 * Exchange stderr <-> stdout.
 *
 * Revision 1.16  1994/01/20  21:14:24  root
 * Indent. Added SS overflow check to show_regs().
 *
 * Revision 1.15  1994/01/19  17:51:14  root
 * Added call in SIGILL to check if illegal op-code is just another hard
 * interrupt ending. It calls int_queue_run() and sees if such was the case.
 * Added more checks for SS being improperly addressed.
 * Added WORD macro in push/pop functions.
 * Added check for DPMI far call.
 *
 * Revision 1.14  1994/01/03  22:19:25  root
 * Fixed problem popping from stack not acking word ss.
 *
 * Revision 1.13  1994/01/01  17:06:19  root
 * Attempt to fix EMS stacking ax problem.
 *
 * Revision 1.12  1993/12/30  11:18:32  root
 * Diamond Fixes.
 *
 * Revision 1.11  1993/12/27  19:06:29  root
 * Tracking down SIGFPE errors from mft.exe divide overflow.
 *
 * Revision 1.10  1993/12/22  11:45:36  root
 * Added more debug
 *
 * Revision 1.9  1993/12/05  20:59:03  root
 * Diamond Card testing
 *
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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "config.h"
#include "memory.h"
#include "cpu.h"
#include "termio.h"
#include "emu.h"
#include "port.h"

/* Needed for DIAMOND define */
#include "video.h"

#ifdef DPMI
#include "dpmi/dpmi.h"
#endif

extern void queue_hard_int(int i, void (*), void (*));
extern inline void do_int(int);
extern int int_queue_running;
extern void int_queue_run();
extern void xms_control(void);

extern inline int can_revector(int);

struct vm86_struct vm86s;

struct vec_t orig[256];		/* "original" interrupt vectors */
struct vec_t snapshot[256];	/* vectors from last snapshot */

extern u_char in_sigsegv, in_sighandler, ignore_segv;
extern int fatalerr;

extern struct config_info config;

inline void set_revectored(int nr, struct revectored_struct * bitmap)
{
	__asm__ __volatile__("btsl %1,%0"
		: /* no output */
		:"m" (*bitmap),"r" (nr));
}

void
cpu_init(void)
{
  int i;
  /* cpu_type set in emulate() via getopt */
  REG(eflags) = VIF | IF;

  /* make ivecs array point to low page (real mode IDT) */
  ivecs = 0;
  /* set up the redirection array */
  memset(&vm86s.int_revectored, 0x00, 32);
  memset(&vm86s.int21_revectored, 0x00, 32);
  for (i=0; i<0x100; i++)
    if (!can_revector(i) && i!=0x21)
      set_revectored(i, &vm86s.int_revectored);
  set_revectored(0x0c, &vm86s.int21_revectored);
  if(config.hogthreshold)
    set_revectored(0x16, &vm86s.int_revectored);

#ifdef INTERNAL_EMS
  set_revectored(0x3e, &vm86s.int21_revectored);
  set_revectored(0x44, &vm86s.int21_revectored);
#endif
  if(config.emusys || config.emubat)
    set_revectored(0x3d, &vm86s.int21_revectored);
}

void
show_regs(void)
{
  int i;
  unsigned char *sp;
  unsigned char *cp = SEG_ADR((unsigned char *), cs, ip);

  if (!LWORD(esp))
    sp = (SEG_ADR((u_char *), ss, sp)) + 0x8000;
  else
    sp = SEG_ADR((u_char *), ss, sp);

  dbug_printf("\nEIP: %04x:%08lx", LWORD(cs), REG(eip));
  dbug_printf(" ESP: %04x:%08lx", LWORD(ss), REG(esp));
  dbug_printf("         VFLAGS(b): ");
  for (i = (1 << 0x11); i > 0; i = (i >> 1))
    dbug_printf((vflags & i) ? "1" : "0");

  dbug_printf("\nEAX: %08lx EBX: %08lx ECX: %08lx EDX: %08lx VFLAGS(h): %08lx",
	      REG(eax), REG(ebx), REG(ecx), REG(edx), (unsigned long)vflags);
  dbug_printf("\nESI: %08lx EDI: %08lx EBP: %08lx",
	      REG(esi), REG(edi), REG(ebp));
  dbug_printf(" DS: %04x ES: %04x FS: %04x GS: %04x\n",
	      LWORD(ds), LWORD(es), LWORD(fs), LWORD(gs));

  /* display vflags symbolically...the #f "stringizes" the macro name */
#define PFLAG(f)  if (REG(eflags)&(f)) dbug_printf(#f" ")

  dbug_printf("FLAGS: ");
  PFLAG(CF);
  PFLAG(PF);
  PFLAG(AF);
  PFLAG(ZF);
  PFLAG(SF);
  PFLAG(TF);
  PFLAG(IF);
  PFLAG(DF);
  PFLAG(OF);
  PFLAG(NT);
  PFLAG(RF);
  PFLAG(VM);
  PFLAG(AC);
  PFLAG(VIF);
  PFLAG(VIP);
  dbug_printf(" IOPL: %u\n", (unsigned) ((vflags & IOPL_MASK) >> 12));

  /* display the 10 bytes before and after CS:EIP.  the -> points
   * to the byte at address CS:EIP
   */
  dbug_printf("STACK: ");
  sp -= 10;
  for (i = 0; i < 10; i++)
    dbug_printf("%02x ", *sp++);
  dbug_printf("-> ");
  for (i = 0; i < 10; i++)
    dbug_printf("%02x ", *sp++);
  dbug_printf("\n");
  dbug_printf("OPS  : ");
  cp -= 10;
  for (i = 0; i < 10; i++)
    dbug_printf("%02x ", *cp++);
  dbug_printf("-> ");
  for (i = 0; i < 10; i++)
    dbug_printf("%02x ", *cp++);
  dbug_printf("\n");
}

void
show_ints(int min, int max)
{
  int i, b;

  max = (max - min) / 3;
  for (i = 0, b = min; i <= max; i++, b += 3) {
    dbug_printf("%02x| %04x:%04x->%05x    ", b, ISEG(b), IOFF(b),
		IVEC(b));
    dbug_printf("%02x| %04x:%04x->%05x    ", b + 1, ISEG(b + 1), IOFF(b + 1),
		IVEC(b + 1));
    dbug_printf("%02x| %04x:%04x->%05x\n", b + 2, ISEG(b + 2), IOFF(b + 2),
		IVEC(b + 2));
  }
}

inline int
do_hard_int(int intno)
{
#ifdef DPMI
  /* send hardware interrupts first to protected mode */
#endif /* DPMI */
  queue_hard_int(intno, NULL, NULL);
  return (1);
}

inline int
do_soft_int(int intno)
{
  do_int(intno);
  return 1;
}

void
sigill(int sig, struct sigcontext_struct context)
{
  unsigned char *csp;
  int i, dee;

#ifdef DPMI
  if (in_dpmi && !in_vm86)
    return dpmi_sigill(&context);
#endif /* DPMI */

  csp = SEG_ADR((unsigned char *), cs, ip);

  if (!in_vm86)			/* test VM bit */
    error("ERROR: NON-VM86 illegal insn!\n");

  in_vm86 = 0;

  error("SIGILL %d received\n", sig);
  show_regs();

  /* Some db commands start with 2e (use cs segment) and thus is accounted
   for here */

  if (csp[0] == 0x2e) {
    csp++;
    LWORD(eip)++;
  }
  if (csp[0] == 0xf0) {
    dbug_printf("ERROR: LOCK prefix not permitted!\n");
    LWORD(eip)++;
    return;
  }

  /* look at this with Checkit...the new code just sits in a loop */
#define OLD_MATH_CODE
#ifdef OLD_MATH_CODE
  i = (csp[0] << 8) + csp[1];	/* swapped */
  if ((i & 0xf800) != 0xd800) {	/* not FPU insn */
    error("ERROR: not an FPU instruction, real illegal opcode!\n");
#if 0
    do_int(0x6);		/* Coprocessor error */
#else
    fatalerr = 4;
#endif
    return;
  }

  /* I don't know what this code does. -Robert */
  switch (i & 0xc0) {
  case 0x00:
    if ((i & 0x7) == 0x6) {
      dee = *(short *) (csp + 2);
      REG(eip) += 4;
    }
    else {
      REG(eip) += 2;
      dee = 0;
    }
    break;
  case 0x40:
    dee = (signed) csp[2];
    REG(eip) += 3;
    break;
  case 0x80:
    dee = *(short *) (csp + 2);
    REG(eip) += 4;
    break;
  default:
    REG(eip) += 2;
    dee = 0;
  }

  warn("MATH: emulation %x d=%x\n", i, dee);

#else /* this is the new, stupid MATH-EMU code. it doesn't work. */

  if ((*csp >= 0xd8) && (*csp <= 0xdf)) {	/* the FPU insn prefix bytes */
    error("MATH: math emulation for (first 2 bytes) %02x %02x...\n",
	  *csp, *(csp + 1));

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
sigfpe(int sig)
{
  if (!in_vm86)
    error("ERROR: NON-VM86 SIGFPE insn!\n");

  in_vm86 = 0;

  error("SIGFPE %d received\n", sig);
  show_regs();
  if (REG(eflags) & VIF) {
    LWORD(eip) -= 1;
    do_int(0);
  }
  else
    error("ERROR: FPE happened but interrupts disabled\n");
}

void
sigtrap(int sig, struct sigcontext_struct context)
{
  struct sigcontext_struct *scp = &context;

#if 0
#ifdef DPMI
  if (in_dpmi && !in_vm86)
    return dpmi_sigtrap(scp);
#endif /* DPMI */
#endif

  if (!in_vm86)
    error("ERROR: NON-VM86 SIGTRAP insn!\n");

  in_vm86 = 0;

  if (_trapno == 3)
    return (void) do_int(3);

  do_int(1);
}

struct port_struct {
  int start;
  int size;
  int permission;
  int ormask, andmask;
}

*ports = NULL;
int num_ports = 0;

extern struct screen_stat scr_state;
u_char video_port_io = 0;

/* find a matching entry in the ports array */
int
find_port(int port, int permission)
{
  static int last_found = 0;
  int i;

  for (i = last_found; i < num_ports; i++)
    if (port >= ports[i].start &&
	port < (ports[i].start + ports[i].size) &&
	((ports[i].permission & permission) == permission)) {
      last_found = i;
      return (i);
    }

  for (i = 0; i < last_found; i++)
    if (port >= ports[i].start &&
	port < (ports[i].start + ports[i].size) &&
	((ports[i].permission & permission) == permission)) {
      last_found = i;
      return (i);
    }

/*
   for now, video_port_io flag allows special access for video regs
*/
  if (   ((LWORD(cs) & 0xf000) == config.vbios_seg) 
      && config.allowvideoportaccess 
      && scr_state.current) {
#if 0
    v_printf("VID: checking port %x\n", port);
#endif
    if (port > 0x4000)
      return (-1);
    port &= 0xfff;
    if (    (port > 0x300 && port < 0x3f8) 
         ||  port > 0x400
         || port == 0x42 || port == 0x1ce || port == 0x1cf) {
      video_port_io = 1;
      return 1;
    }
  }

  return (-1);
}

/* control the io permissions for ports */
int
allow_io(int start, int size, int permission, int ormask, int andmask)
{

  /* first the simplest case...however, this simplest case does
   * not apply to ports above 0x3ff--the maximum ioperm()able
   * port under Linux--so we must handle some of that ourselves.
   */
  if (permission == IO_RDWR && (ormask == 0 && andmask == 0xFFFF)) {
    if ((start + size - 1) <= 0x3ff) {
      c_printf("giving hardware access to ports 0x%x -> 0x%x\n",
	       start, start + size - 1);
      set_ioperm(start, size, 1);
      return (1);
    }

    /* allow direct I/O to all the ports that *are* below 0x3ff
       * and add the rest to our list
       */
    else if (start <= 0x3ff) {
      warn("PORT: s-s: %d %d\n", start, size);
      set_ioperm(start, 0x400 - start, 1);
      size = start + size - 0x400;
      start = 0x400;
    }
  }

  /* we'll need to add an entry */
  if (ports)
    ports = (struct port_struct *)
      realloc(ports, sizeof(struct port_struct) * (num_ports + 1));

  else
    ports = (struct port_struct *)
      malloc(sizeof(struct port_struct) * (num_ports + 1));

  num_ports++;
  if (!ports) {
    error("allocation error for ports!\n");
    num_ports = 0;
    return (0);
  }

  ports[num_ports - 1].start = start;
  ports[num_ports - 1].size = size;
  ports[num_ports - 1].permission = permission;
  ports[num_ports - 1].ormask = ormask;
  ports[num_ports - 1].andmask = andmask;

  c_printf("added port %x size=%d permission=%d ormask=%x andmask=%x\n",
	   start, size, permission, ormask, andmask);

  return (1);
}

int
port_readable(int port)
{
  return (find_port(port, IO_READ) != -1);
}

int
port_writeable(int port)
{
  return (find_port(port, IO_WRITE) != -1);
}

int
read_port(int port)
{
  int r;
  int i = find_port(port, IO_READ);

  if (i == -1)
    return (0);

  if (port <= 0x3ff)
    set_ioperm(port, 1, 1);
  else
    iopl(3);

  r = port_in(port);

  if (port <= 0x3ff)
    set_ioperm(port, 1, 0);
  else
    iopl(0);

  if (!video_port_io) {
    r &= ports[i].andmask;
    r |= ports[i].ormask;
  }
  else
    video_port_io = 0;

  h_printf("read port 0x%x gave %02x at %04x:%04x\n",
	   port, r, LWORD(cs), LWORD(eip));
  return (r);
}

int
write_port(int value, int port)
{
  int i = find_port(port, IO_WRITE);

  if (i == -1)
    return (0);

  if (!video_port_io) {
    value &= ports[i].andmask;
    value |= ports[i].ormask;
  }
  else
    video_port_io = 0;

  h_printf("write port 0x%x value %02x at %04x:%04x\n",
	   port, value, LWORD(cs), LWORD(eip));

  if (port <= 0x3ff)
    set_ioperm(port, 1, 1);
  else
    iopl(3);

  port_out(value, port);

  if (port <= 0x3ff)
    set_ioperm(port, 1, 0);
  else
    iopl(0);

  return (1);
}

#undef CPU_C

/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * Stupid Simple Cooperative Thread Dispatcher (SSCTD;-)
 *
 * Copyright (c) 1994 .. 2001 Hans Lermen <lermen@fgan.de>
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "config.h"
#include "emu.h"
#include "cpu.h"
#include "memory.h"
#include "doshelpers.h"
#include "bitops.h"
#include "utilities.h"
#include "../commands/comcom.h"

#define COOPTHREADS_H_ITSELF
#include "coopthreads.h"

#define DEBUG 0


#if DEBUG > 0
#include <ctype.h>

void com_hexdump(void *buf_, int off, int len)
{
	char *buf = buf_;
	int i,j;
	for (i=off; i<(off+len); i+=16) {
		fprintf(stderr, "%06x ", i);
		for (j=0; j<16; j++) {
			if (j) fprintf(stderr, " ");
			fprintf(stderr, "%02x",(unsigned char)buf[i+j]);
			if ((j & 3) == 3) fprintf(stderr, " ");
			if ((j & 7) == 7) fprintf(stderr, " ");
		}
		for (j=0; j<16; j++) {
			if (isprint(buf[i+j]) && (buf[i+j] >=32)) fprintf(stderr, "%c", buf[i+j]);
			else fprintf(stderr, ".");
		}
		fprintf(stderr, "\n");
	}
}
#endif

#if DEBUG > 0
void dump_regs(char *file, int line)
{
  int i;
  unsigned char *sp;
  unsigned char *cp;

  cp = SEG_ADR((unsigned char *), cs, ip);
  if (!cp) {
    fprintf(stderr, "Ain't gonna do it, cs=0x%x, eip=0x%x\n",REG(cs),LWORD(eip));
    return;
  }

  if (!LWORD(esp))
    sp = (SEG_ADR((u_char *), ss, sp)) + 0x8000;
  else
    sp = SEG_ADR((u_char *), ss, sp);

  fprintf(stderr, "Program=%s, Line=%d\n", file, line);
  fprintf(stderr, "EIP: %04x:%08lx", LWORD(cs), REG(eip));
  fprintf(stderr, " ESP: %04x:%08lx", LWORD(ss), REG(esp));
#if 1
  fprintf(stderr, "  VFLAGS(b): ");
  for (i = (1 << 0x14); i > 0; i = (i >> 1)) {
    fprintf(stderr, (vflags & i) ? "1" : "0");
    if (i & 0x10100) fprintf(stderr, " ");
  }
#else
  fprintf(stderr, "         VFLAGS(b): ");
  for (i = (1 << 0x11); i > 0; i = (i >> 1))
    fprintf(stderr, (vflags & i) ? "1" : "0");
#endif
  fprintf(stderr, "\nEAX: %08lx EBX: %08lx ECX: %08lx EDX: %08lx VFLAGS(h): %08lx",
	      REG(eax), REG(ebx), REG(ecx), REG(edx), (unsigned long)vflags);
  fprintf(stderr, "\nESI: %08lx EDI: %08lx EBP: %08lx",
	      REG(esi), REG(edi), REG(ebp));
  fprintf(stderr, " DS: %04x ES: %04x FS: %04x GS: %04x\n",
	      LWORD(ds), LWORD(es), LWORD(fs), LWORD(gs));

  /* display vflags symbolically...the #f "stringizes" the macro name */
#define PFLAG(f)  if (REG(eflags)&(f)) fprintf(stderr, #f" ")

  fprintf(stderr, "FLAGS: ");
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
  fprintf(stderr, " IOPL: %u\n", (unsigned) ((vflags & IOPL_MASK) >> 12));

  /* display the 10 bytes before and after CS:EIP.  the -> points
   * to the byte at address CS:EIP
   */
  fprintf(stderr, "STACK: ");
  sp -= 10;
  for (i = 0; i < 10; i++)
    fprintf(stderr, "%02x ", *sp++);
  fprintf(stderr, "-> ");
  for (i = 0; i < 10; i++)
    fprintf(stderr, "%02x ", *sp++);
  fprintf(stderr, "\n");
  fprintf(stderr, "OPS  : ");
  cp -= 10;
  for (i = 0; i < 10; i++)
    fprintf(stderr, "%02x ", *cp++);
  fprintf(stderr, "-> ");
  for (i = 0; i < 10; i++)
    fprintf(stderr, "%02x ", *cp++);
  fprintf(stderr, "\n\t%s\n", emu_disasm(1,0));
}
#endif


/* --------------------------------------------------------------- */

/* --------------------- for stack alloc stuff */

#define STACK_GRAN_SHIFT	17	/* 128K address space granularity */
#define STACK_GRAN		(1U << STACK_GRAN_SHIFT)
#define STACK_TOP		0xc0000000U
#define STACK_BOTTOM		0xa0000000U
#define STACK_TOP_PADDING	STACK_GRAN
#define STACK_SLOTS		((STACK_TOP-STACK_BOTTOM) >> STACK_GRAN_SHIFT)

#define roundup_stacksize(size) ((size + STACK_GRAN - 1) & (-((int)STACK_GRAN)))

static unsigned int stack_pool_bottom=0;
static char *force_stack_expand(unsigned long address);
static void *alloc_stack_page(int size);

static unsigned int stack_alloc_map[STACK_SLOTS / 32];

/* ----------------------- inlines --------------------- */

/*
 * Some hacks to defeat gcc over-optimizations..
 */
struct coop__dummy {
    unsigned long   a[100];
};
#define DUMMYADDR (*(struct coop__dummy *) addr)

/* --------------------- stack alloc stuff ------------------ */

/* SIDOC_BEGIN_FUNCTION Making room on the stack
 * PROTO */
static char *force_stack_expand(unsigned long address)
{
	/* /PROTO
	 * The kernel has a feature, such that it doesn't
	 * allow grow down the stack below the current stack pointer.
	 * This makes sense in single threaded applications,
	 * but is bad for threading purposes:
VERB	 *
	 *   1.	You are forced to put the stacks into the heap (below 1st giga)
	 *	hence it is less protected against overwriting then normally.
	 *   2.	The 1 giga stack address space isn't used any more, and
	 *	you may have problems with address space, on huge programs
	 *	that use huge arrays (data bases, matrices).
/VERB	 *
	 * We trick out the kernel by expanding the stack vma to a given
	 * value (setting ESP to the bottom and thouching it).
	 * ( look at do_page_fault in arch/i386/mm/fault.c how it
	 *   treats growing down of stack )
 * SIDOC_END_FUNCTION */
	__asm__ volatile (" \
		xchgl	%%esp,%%eax\n \
		movb	$1,(%%esp)\n \
		xchgl	%%esp,%%eax" \
		: : "a" (address)
	);
	return (char *)address;
}

static void update_stack_alloc_map(unsigned int bottom, unsigned int top, int bit)
{
	int i;

	bottom = (bottom - STACK_BOTTOM) >> STACK_GRAN_SHIFT;
	top = (top - STACK_BOTTOM) >> STACK_GRAN_SHIFT;
	for (i=bottom; (i < top) && (i < STACK_SLOTS); i++) {
		if (bit) set_bit(i, stack_alloc_map);
		else clear_bit(i, stack_alloc_map);
	}
}

static unsigned int find_free_stack_slots(int numslots)
{
	int i, count, bottom;

	if (!stack_pool_bottom) return 0;
	bottom = (stack_pool_bottom-STACK_BOTTOM) >> STACK_GRAN_SHIFT;
	count = 0;
	for (i=STACK_SLOTS-1; i>=bottom; i--) {
		if (test_bit(i, stack_alloc_map)) count = 0;
		else if (++count >= numslots) {
			return (((unsigned int)i) << STACK_GRAN_SHIFT) + STACK_BOTTOM;
		}
	}
	return 0;
}


static void free_stack_page(void *page, int size)
{
	size = roundup_stacksize(size);
	update_stack_alloc_map((unsigned int)page, ((unsigned int)page)+size, 0);
}

static void *alloc_stack_page(int size)
{
	int gran = STACK_GRAN;
	unsigned int bottom;

	size = roundup_stacksize(size);
	if ((bottom = find_free_stack_slots(size >> STACK_GRAN_SHIFT)) != 0) {
		update_stack_alloc_map(bottom, bottom + size, 1);
		return (void *)bottom;
	}
	if (!stack_pool_bottom) {
		/* first time (thread0) creation */
		memset(stack_alloc_map, 0, sizeof(stack_alloc_map));
		stack_pool_bottom = STACK_TOP - STACK_TOP_PADDING;
		if ((STACKPTR-256) < (STACK_TOP - STACK_TOP_PADDING))
			stack_pool_bottom = (STACKPTR-256-size) & (-gran);
		update_stack_alloc_map(stack_pool_bottom, STACK_TOP, 1);
	}
	bottom = (stack_pool_bottom - size) & (-gran);
	if (bottom < STACK_BOTTOM)
		return 0;
	update_stack_alloc_map(bottom, stack_pool_bottom, 1);
	stack_pool_bottom = bottom;
	return (void *)force_stack_expand((unsigned long)stack_pool_bottom);
}


void switch_to(struct tcb* new)
{
	if (setjmp(NONVOLATILE_OWNTCB->context)) return;
	owntcb = new;
	longjmp(new->context, 1);
	/* we never come here */
}


static void prepare_thread_start(struct tcb* new)
{
	static unsigned long saved_stackptr, new_stackptr;
	new_stackptr = (unsigned long)(new->stack + new->stack_size);
	saved_stackptr = STACKPTR;
	STACKPTR = new_stackptr;
	if (setjmp(new->context)) {
		(*owntcb->thread_code)(owntcb->params);
		/* we never should come here !!! */
	}
	else {
		STACKPTR = saved_stackptr;
	}
}

static struct tcb *create_thread(thread_function_type *thread_code,
		int stacksize, void *params)
{
	struct tcb *tcb;

	stacksize = roundup_stacksize(stacksize);
	tcb = (void *)malloc(sizeof(struct tcb));
	if (!tcb) return 0;
	tcb->thread_code = thread_code;
	tcb->params = params;
	tcb->stack_size = stacksize;
	tcb->stack = alloc_stack_page(stacksize);
	prepare_thread_start(tcb);
	return tcb;
}


static int delete_thread(struct tcb *tcb, struct tcb *successor)
{
	if (!tcb) (volatile struct tcb *)tcb = owntcb;
	if (!successor) (volatile struct tcb *)successor = owntcb;
	if (tcb == successor) return 0;
	free_stack_page(tcb->stack, tcb->stack_size);
	free(tcb);
	if (owntcb == successor) return 1;
	if (owntcb == tcb) {
		/* we deleted ourself, switching to other thread */
		owntcb = successor;
		longjmp(successor->context, 1);
		/* doesn't return here */
	}
	/* deleted and successor threads both are not owntcb */
	switch_to(successor);
	return 1;
}


static struct tcb *init_zero_thread(int stacksize)
{
	struct tcb *tcb;

	tcb = (void *)malloc(sizeof(struct tcb));
	if (!tcb) return 0;

	stacksize = roundup_stacksize(stacksize);
	/* first we have to reserve our own stack
	 * We leave the STACKPTR untouched, but force the bottom to STACK_GRAN
	 * NOTE: we need being called by the main process with STACKPTR
	 * being within our limits
	 */
	if ((STACKPTR-0x1000) < (STACK_TOP - STACK_GRAN) ) return 0;
	stack_pool_bottom = STACK_TOP;
	tcb->stack_size = stacksize;
	tcb->stack = alloc_stack_page(stacksize);
	tcb->thread_code = 0;
	tcb->params = 0;
	owntcb = tcb;
	return tcb;
}


/* ================================================================ */



static void update_lowmem_alloc_map(int slot, int numslots, int bit)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	int i, top;

	top = slot + numslots;
	for (i=slot; i < top; i++) {
		if (bit) set_bit(i, ctcb->heapmap);
		else clear_bit(i, ctcb->heapmap);
	}
#if DEBUG > 2
	fprintf(stderr, "lowmem heapstart= %x, heapend= %x\n",
		 /*((char *)ctcb) +*/ ctcb->heapstart,
		 /*((char *)ctcb) +*/ ctcb->heapend
	);
	com_hexdump(ctcb->heapmap, 0, top);
#endif
}

char *lowmem_alloc(int size)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	int i, j, count, maxslots, numslots;


	numslots = (size + LOWMEM_HEAP_GRAN - 1) >> LOWMEM_HEAP_SHIFT;
	maxslots = (ctcb->heapend - ctcb->heapstart) >> LOWMEM_HEAP_SHIFT;
	count = 0;
	for (i=0; i < maxslots; i++) {
		if (test_bit(i, ctcb->heapmap)) count = 0;
		else if (++count >= numslots) {
			i -= count - 1;
			update_lowmem_alloc_map(i, count, 1);
			i = ctcb->heapstart + (i << LOWMEM_HEAP_SHIFT);
			j = i + (count << LOWMEM_HEAP_SHIFT);
			if (j > ctcb->heapptr) {
				 ctcb->heapptr = j;
#if DEBUG > 1
			fprintf(stderr, "lowmem_alloc, PSP %04x max avail mem 0x%x, max used mem = 0x%x\n",
		 			(unsigned)ctcb>>4,
		 			ctcb->heapend - ctcb->heapstart,
		 			ctcb->heapptr - ctcb->heapstart);
#endif
			}

#if DEBUG >4
			fprintf(stderr, "psp %x: lowmem_alloc(%d) got %p ; called by %x\n",
				(unsigned)ctcb>>4, size, ((char *)ctcb) + i,
				CALLER(size))
			);
#endif
			return ((char *)ctcb) + i;
		}
	}
	return 0;
}

void lowmem_free(char *p, int size)
{
	int slot, numslots, maxslots;
	struct com_starter_seg  *ctcb = owntcb->params;

#if DEBUG >4
	fprintf(stderr, "psp %x: lowmem_free(%p, %d); called by %x\n",
		(unsigned)ctcb)>>4, p, size, CALLER(p)
	);
#endif

	if (!p) {
		error("coopthreads: trying to lowmem_free NULL pointer\n");
		leavedos(99);
	}
	numslots = (size + LOWMEM_HEAP_GRAN - 1) >> LOWMEM_HEAP_SHIFT;
	maxslots = (ctcb->heapend - ctcb->heapstart) >> LOWMEM_HEAP_SHIFT;
	slot = (p - (((char *)ctcb)+ctcb->heapstart)) >> LOWMEM_HEAP_SHIFT;
	if (slot+numslots > maxslots) numslots = maxslots - slot;
	if (slot < 0 || slot > maxslots) {
		error("coopthreads: trying to lowmem_free bogus pointer\n");
		leavedos(99);
	}
	update_lowmem_alloc_map(slot, numslots, 0);
}

/* ================================================================ */

static char *scratch = 0;
static char *scratch2 = 0;

static void mkscratch(void)
{
	if (scratch) return;
	scratch = malloc(0x10000);
	if (!scratch) {
		error("coopthreads: out of memory\n");
		leavedos(99);
	}
	scratch2 = malloc(0x10000);
	if (!scratch2) {
		error("coopthreads: out of memory\n");
		leavedos(99);
	}
}

void com_exit(int exitcode)
{
	LWORD(eax) = exitcode;
	longjmp(NONVOLATILE_OWNTCB->exit_context, 1);
}

void com_delete_other_thread(struct com_starter_seg *other)
{
	delete_thread(other->tcb, 0);
}

static int get_current_program_mem(void)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	struct com_MCB *mcb = (void *)((ctcb->real_psp - 0x1) << 4);
	return mcb->size;
}

void com_free_unused_dosmem(void)
{
	struct com_starter_seg  *ctcb = owntcb->params;
#if 1
	LWORD(ebx) = ctcb->brkpoint ? (ctcb->brkpoint+15) >> 4 : 0x1000;
#else
	int brk_point = ((int)((char *)ctcb + ctcb->oldsp + 128)) >> 4;
	LWORD(ebx) = brk_point - ctcb->real_psp;
#endif
	LWORD(es) = ctcb->real_psp;
	HI(ax) = 0x4a;
	call_msdos();
}

int load_and_run_DOS_program(char *command, char *cmdline)
{
	static char runcode_template [] = {
			/* int 0x21 */
		0xcd, 0x21,
			/* lss sp, cs:[0x10c] */
		0x2E, 0x0F, 0xB2, 0x26, 0x10, 0x01,
			/* retf */
		0xcb
	};
	struct param4a {
		unsigned short envframe;
		FAR_PTR cmdline;
		FAR_PTR fcb1;
		FAR_PTR fcb2;
	} __attribute__((packed));
	struct com_starter_seg  *ctcb = owntcb->params;
	int current_mem_size;
	char *runcode, *cmd, *cmdl;
	struct param4a *pa4;
	struct vm86_regs save_regs;
	int ret, retax;

	runcode = lowmem_alloc(128);
	if (!runcode) return -1;
	memset(runcode, 0, 128);
	memcpy(runcode, runcode_template, sizeof(runcode_template));
	pa4 = (void *)(runcode + 16);

	cmd = com_strdup(command);
	if (!cmd) {
		lowmem_free(runcode, 128);
		com_errno = 8;
		return -1;
	}
	cmdl = lowmem_alloc(256);
	if (!cmdl) {
		lowmem_free(runcode, 128);
		com_strfree(cmd);
		com_errno = 8;
		return -1;
	}
	if (!cmdline) cmdline = "";
	snprintf(cmdl, 256, "%c %s\r", (char)(strlen(cmdline)+1), cmdline);

	current_mem_size = get_current_program_mem();

	save_regs = REGS;	/* save all registers */
	/* free the DOS memory we don't need currently */
	com_free_unused_dosmem();


	/* prepare param block */
	pa4->envframe = 0; // ctcb->envir_frame;
	pa4->cmdline = MK_FP(COM_SEG, COM_OFFS_OF(cmdl));
	pa4->fcb1 = MK_FP(COM_SEG, COM_OFFS_OF(ctcb->FCB1));
	pa4->fcb2 = MK_FP(COM_SEG, COM_OFFS_OF(ctcb->FCB2));
	LWORD(ds) = COM_SEG;
	LWORD(ebx) = COM_OFFS_OF(pa4);

	/* path of programm to load */
	LWORD(es) = COM_SEG;
	LWORD(edx) = COM_OFFS_OF(cmd);
	

	/* save our stack pointer,
	 * this stupid DOS function does save nothing, such that we
	 * would not be able to do a return
	 */
	ctcb->saved_sp = LWORD(esp) -4; /* '-4' because we 'push' later
					 * in call_vm86_from_com_thread()
					 */
	ctcb->saved_ss = LWORD(ss);

	LWORD(eax) = 0x4b00;
	call_vm86_from_com_thread(COM_SEG, COM_OFFS_OF(runcode));
	ret = LWORD(eflags);
	retax = LWORD(eax);
	REGS = save_regs;       /* restore all registers */

	/* now restore the memory block to its old size
	 * As we don't expect TSR to be loaded, this is save
	 */
	LWORD(ebx) = current_mem_size;
	LWORD(es) = ctcb->real_psp;
	HI(ax) = 0x4a;
	call_msdos();

	/* set our DTA back, DOS doesn't do that */
	LWORD(ds) = COM_SEG;
	LWORD(edx) = 0x80;
	HI(ax) = 0x1a;
	call_msdos();	/* call MSDOS */

	/* free the memory on our heap */
	lowmem_free(runcode, 128);
	com_strfree(cmd);
	lowmem_free(cmdl,256);
	if (ret & CF) {
		com_errno = retax;
		return -1;
	}

	/* ok, program executed and returned to us
	 * now get the exitcode
	 */
	HI(ax) = 0x4d;
	call_msdos();
	return LWORD(eax);
}

char *com_getenv(char *keyword)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	char *env = SEG2LINEAR(ctcb->envir_frame);
	char key[128];
	int len;

	len = strlen(keyword);
	memcpy(key, keyword, len+1);
	strupr(key);
	while (*env) {
		if (!strncmp(key, env, len) && (env[len] == '=')) {
			return env + len + 1;
		}
		env += strlen(env) + 1;
	}
	return 0;
}

int com_system(char *command)
{
	char *program = com_getenv("COMSPEC");
	char cmdline[256];

	snprintf(cmdline, sizeof(cmdline), "/C %s", command);
	if (!program) program = "\\COMMAND.COM";
	return load_and_run_DOS_program(program, cmdline);
}


int com_dosallocmem(int size)
{
	/* size in bytes */
	LWORD(ebx) = (size +15) >> 4;
	HI(ax) = 0x48;
	call_msdos();    /* call MSDOS */
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		return (0x80000000 | (((int)LWORD(ebx)) << 4));
	}
	return (((int)LWORD(eax)) << 4);
}

int com_dosfreemem(int frame)
{
	LWORD(es) = frame;
	HI(ax) = 0x49;
	call_msdos();    /* call MSDOS */
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		return -1;
	}
	return 0;
}

int com_dosreallocmem(int frame, int newsize)
{
	/* size in bytes */
	LWORD(ebx) = (newsize +15) >> 4;
	LWORD(es) = frame;
	HI(ax) = 0x4a;
	call_msdos();
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		return (0x80000000 | (((int)LWORD(ebx)) << 4));
	}
	return 0;
}


int com_dosopen(char *fname, int access)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	char *s;

	if (!fname || !fname[0]) return -1;
	com_errno = 8;
	s = com_strdup(fname);
	if (!s) return -1;
	if (access & DOS_ACCESS_CREATE) HI(ax) = 0x3c;
	else HI(ax) = 0x3d;
	LO(ax) = access;
	LWORD(ecx) = (access & DOS_ACCESS_ATTR ) >> 8;
	LWORD(ds) = COM_SEG;
	LWORD(edx) = COM_OFFS_OF(s);
	call_msdos();	/* call MSDOS */
	com_strfree(s);
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		return -1;
	}
	return LWORD(eax);
}

int com_doscreatetempfile(char *pathtemplate /* needs 13 bytes tail behind */)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	char *s;
	int len = strlen(pathtemplate);

	s = lowmem_alloc(128);
	if (!s) return -1;
	memcpy(s, pathtemplate, len+1);
	strcpy(s, pathtemplate);
	HI(ax) = 0x5a;
	LWORD(ds) = COM_SEG;
	LWORD(edx) = COM_OFFS_OF(s);
	call_msdos();    /* call MSDOS */
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		lowmem_free(s, 128);
		return -1;
	}
	s[len] = '\\';
	memcpy(pathtemplate+len, s+len, 14);
	lowmem_free(s, 128);
	return LWORD(eax);
}

int com_dosrenamefile(char *old, char *new)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	char *o, *n;

	com_errno = 8;
	o = com_strdup(old);
	if (!o) return -1;
	n = com_strdup(new);
	if (!n) return -1;
	HI(ax) = 0x56;
	LWORD(ds) = LWORD(es) = COM_SEG;
	LWORD(edx) = COM_OFFS_OF(o);
	LWORD(edi) = COM_OFFS_OF(n);
	call_msdos();    /* call MSDOS */
	com_strfree(o);
	com_strfree(n);
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		return -1;
	}
	return 0;
}

int com_dosclose(int dosfilefd)
{
	HI(ax) = 0x3e;
	LWORD(ebx) = dosfilefd;
	call_msdos();	/* call MSDOS */
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		return -1;
	}
	return 0;
}

int com_dosdeletefile(char *file)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	char *s;

	s = com_strdup(file);
	if (!s) return -1;
	HI(ax) = 0x41;
	LWORD(ds) = COM_SEG;
	LWORD(edx) = COM_OFFS_OF(s);
	call_msdos();    /* call MSDOS */
	com_strfree(s);
	if (LWORD(eflags) & CF) return -1;
	return 0;
}

int com_doswrite(int dosfilefd, char *buf32, int size)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	char *s;

	if (!size) return 0;
	com_errno = 8;
	s = lowmem_alloc(size);
	if (!s) return -1;
	memcpy(s, buf32, size);
	LWORD(ecx) = size;
	LWORD(ebx) = dosfilefd;
	LWORD(ds) = COM_SEG;
	LWORD(edx) = COM_OFFS_OF(s);
	LWORD(eax) = 0x4000;	/* write handle */
	call_msdos();	/* call MSDOS */
	lowmem_free(s, size);
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		return -1;
	}
	return  LWORD(eax);
}


int com_doswriteconsole(char *string)
{
	char *s = string;

	while (*s) {
		HI(ax) = 0x06;
		LO(dx) = *s++;
		call_msdos();    /* call MSDOS */
	}
	return 0;
}


int com_dosread(int dosfilefd, char *buf32, int size)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	char *s;

	if (!size) return 0;
	com_errno = 8;
	s = lowmem_alloc(size);
	if (!s) return -1;
	LWORD(ecx) = size;
	LWORD(ebx) = dosfilefd;
	LWORD(ds) = COM_SEG;
	LWORD(edx) = COM_OFFS_OF(s);
	LWORD(eax) = 0x3f00;	/* read handle */
	call_msdos();	/* call MSDOS */
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		lowmem_free(s, size);
		return -1;
	}
	memcpy(buf32, s, LWORD(eax));
	lowmem_free(s, size);
	return  LWORD(eax);
}

int com_doscopy(int writefd, int readfd, int size, int textmode)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	char *s, *p;

	if (!size) return 0;
	s = lowmem_alloc(size);
	com_errno = 8;
	if (!s) return -1;
	LWORD(ecx) = size;
	LWORD(ebx) = readfd;
	LWORD(ds) = COM_SEG;
	LWORD(edx) = COM_OFFS_OF(s);
	LWORD(eax) = 0x3f00;	/* read handle */
	call_msdos();	/* call MSDOS */
	if (LWORD(eflags) & CF || !LWORD(eax)) {
		com_errno = LWORD(eax);
		lowmem_free(s, size);
		if (LWORD(eax)) return -1;
		return 0;
	}
	if (textmode) {
		p = memchr(s, 0x1a, LWORD(eax));
		if (p) LWORD(eax) = p - s;
		if (!LWORD(eax)) {
			lowmem_free(s, size);
			return 0;
		}
	}
	LWORD(ecx) = LWORD(eax);
	LWORD(ebx) = writefd;
	LWORD(eax) = 0x4000;	/* write handle */
	call_msdos();	/* call MSDOS */
	lowmem_free(s, size);
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		return -1;
	}
	return  LWORD(eax);
}

long com_dosseek(int dosfilefd, unsigned long offset, int whence)
{
	HI(ax) = 0x42;
	LO(ax) = whence;
	LWORD(ebx) = dosfilefd;
	LWORD(ecx) = offset >> 16;
	LWORD(edx) = offset;
	call_msdos();    /* call MSDOS */
	if (LWORD(eflags) & CF) return -1;
	return ((long)LWORD(edx)) << 16 | LWORD(eax);
}


long com_seektotextend(int fd)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	char *s, *p;
	long pos = 0;

	s = lowmem_alloc(128);
	com_errno = 8;
	if (!s) return -1;

	do {
		LWORD(ecx) = 128;
		LWORD(ebx) = fd;
		LWORD(ds) = COM_SEG;
		LWORD(edx) = COM_OFFS_OF(s);
		LWORD(eax) = 0x3f00;	/* read handle */
		call_msdos();	/* call MSDOS */
		if (LWORD(eflags) & CF) {
			com_errno = LWORD(eax);
			lowmem_free(s, 128);
			return -1;
		}
		p = memchr(s, 0x1a, LWORD(eax));
		if (p) {
			pos += p - s;
			com_dosseek(fd, pos, 0);
			lowmem_free(s, 128);
			return pos;
		}
		pos += LWORD(eax);
	} while (LWORD(eax));
	lowmem_free(s, 128);
	return pos;
}

int com_dosreadtextline(int fd, char * buf, int bsize, long *lastseek)
{
	static char eolpattern[] = {'\r', '\n', 0x1a, 0};
	int size;
	long newseek;
	char *s, *p;

	s = lowmem_alloc(bsize);
	if(!s) return -1;
	if (lastseek) newseek = com_dosseek(fd, *lastseek, 0);
	else newseek = com_dosseek(fd, 0, 1);
	size = com_dosread(fd, s, bsize);
	if (size  <= 0 || s[0] == 0x1a) {
		lowmem_free(s, bsize);
		buf[0] = 0;
		return -1;
	}
	p = strpbrk(s, eolpattern);
	if (p && (p - s) < size) {
		size = (p - s);
		if (*p == '\r') p +=2;
		else if (*p != 0x1a) p++;
		newseek += p - s;
	}
	else {
		newseek += size;
	}
	newseek =  com_dosseek(fd, newseek, 0);
	if (lastseek) *lastseek = newseek;
	memcpy(buf, s, size);
	if (size < bsize) buf[size] = 0;
	lowmem_free(s, bsize);
	return size;
}

int com_dosfindfirst(char *path, int attr)
{
	/* NOTE, the result is stored in ctcb->DTA */
	struct com_starter_seg  *ctcb = owntcb->params;
	char *s;

	com_errno = 8;
	s = com_strdup(path);
	if (!s) return -1;

	HI(ax) = 0x4e;
	LWORD(ecx) = attr;
	LWORD(ds) = COM_SEG;
	LWORD(edx) = COM_OFFS_OF(s);
	call_msdos();	/* call MSDOS */
	com_strfree(s);
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		return -1;
	}
	return 0;
}

int com_dosfindnext(void)
{
	/* NOTE, the result is stored in ctcb->DTA */

	HI(ax) = 0x4f;
	call_msdos();	/* call MSDOS */
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		return -1;
	}
	return 0;
}

int com_dosgetcurrentdir(int drive, char *buf)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	char *s = lowmem_alloc(128);

	com_errno = 8;
	if (!s) return -1;
	HI(ax) = 0x47;
	LO(dx) = drive;		/* 0=current, 1=A, 2=B ... */
	LWORD(ds) = COM_SEG;
	LWORD(esi) = COM_OFFS_OF(s);
	call_msdos();    /* call MSDOS */
	if (LWORD(eflags) & CF) {
		lowmem_free(s, 128);
		com_errno = LWORD(eax);
		return -1;
	}
	strcpy(buf, s);
	lowmem_free(s, 128);
	return 0;
}

int com_dosgetsetfilestamp(int fd, int ftime)
{
	if (ftime) {
		/* set file stamp */
		LO(ax) = 1;
		LWORD(ecx) = ftime;		/* time */
		LWORD(edx) = ftime >> 16;	/* date */
	}
	HI(ax) = 0x57;
	LWORD(ebx) = fd;
	call_msdos();
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		return -1;
	}
	if (!ftime) ftime = ((unsigned)LWORD(edx) << 16) | LWORD(ecx);
	return ftime;
}

int com_dosgetfreediskspace(int drive, unsigned result[])
{
	HI(ax) = 0x36;
	LO(dx) = drive;	/* 0 = default, 1 = A: ... */
	if (LWORD(eax) == 0xffff) return -1;
	call_msdos();    /* call MSDOS */
	result[0] = LWORD(eax);	/* sectors per cluster */
	result[1] = LWORD(ebx);	/* available clusters */
	result[2] = LWORD(ecx);	/* bytes per sector */
	result[3] = LWORD(edx);	/* clusters per drive */
	return 0;
}

int com_doscanonicalize(char *buf, char *path)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	char *s = lowmem_alloc(256);

	com_errno = 8;
	if (!s) return -1;
	strcpy(s, path);
	HI(ax) = 0x60;
	LWORD(ds) = LWORD(es) = COM_SEG;
	LWORD(esi) = COM_OFFS_OF(s);
	LWORD(edi) = COM_OFFS_OF(s+128);
	call_msdos();    /* call MSDOS */
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		lowmem_free(s, 256);
		return -1;
	}
	strcpy(buf, s+128);
	lowmem_free(s, 256);
	return 0;
}

int com_getdriveandpath(int drive, char *buf)
{
	if (!drive) drive = com_dosgetdrive() +1;
	sprintf(buf, "%c:\\", 'A' + drive -1);
	return com_dosgetcurrentdir(drive, buf+3);
}

int com_dosmkrmdir(char *path, int remove)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	char *s = com_strdup(path);

	com_errno = 8;
	if (!s) return -1;
	HI(ax) = remove ? 0x3a: 0x39;
	LWORD(ds) = COM_SEG;
	LWORD(edx) = COM_OFFS_OF(s);
	call_msdos();    /* call MSDOS */
	com_strfree(s);
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		return -1;
	}
	return 0;
}

int com_dosduphandle(int fd)
{
	HI(ax) = 0x45;
	LWORD(ebx) = fd;
	call_msdos();    /* call MSDOS */
	if (LWORD(eflags) & CF) return -1;
	return LWORD(eax);
}

int com_dosforceduphandle(int open_fd, int new_fd)
{
	HI(ax) = 0x46;
	LWORD(ebx) = open_fd;
	LWORD(ecx) = new_fd;
	call_msdos();    /* call MSDOS */
	if (LWORD(eflags) & CF) return -1;
	return 0;
}

int com_dosgetdate(void)
{
	HI(ax) = 0x2a;
	call_msdos();	/* call MSDOS */
	/* returned format: 0x0YYYWMDD
	 * YYY	= year (1980 .. 2099)
	 * W	= day of week (0=Sun, 1=Mon, ... , 6=Sat)
	 * M	= month (1 .. 12)
	 * DD	= day within month (1 ..31)
	 */
	return (LWORD(ecx) << 16) | (LO(ax) << 12) | (LWORD(edx) & 0xfff);
}

int com_dossetdate(int date)
{
	/* input format: 0x0YYY0MDD
	 * YYY	= year (1980 .. 2099)
	 * M	= month (1 .. 12)
	 * DD	= day within month (1 ..31)
	 */
	HI(ax) = 0x2b;
	LWORD(ecx) = date >> 16;
	LWORD(edx) = date & 0xfff;
	call_msdos();	/* call MSDOS */
	return LO(ax) ? -1 : 0;
}

int com_dosgettime(void)
{
	HI(ax) = 0x2c;
	call_msdos();	/* call MSDOS */

	/* returned format: 0xHHMMSSss
	 * HH = hours (0 .. 24)
	 * MM = minutes (0 .. 59)
	 * SS = seconds (0 .. 59)
	 * ss = hundredths (0 .. 99)
	 */
	return (LWORD(ecx) << 16) | LWORD(edx);
}


int com_dossettime(int time)
{
	/* Input format: 0xHHMMSSss
	 * HH = hours (0 .. 24)
	 * MM = minutes (0 .. 59)
	 * SS = seconds (0 .. 59)
	 * ss = hundredths (0 .. 99)
	 */
	LWORD(ecx) = time >> 16;
	LWORD(edx) = time;
	HI(ax) = 0x2d;
	call_msdos();	/* call MSDOS */
	return LO(ax) ? -1 : 0;
}


void com_intr(int intno, struct REGPACK *regpack)
{
	LWORD(eax) = regpack->r_ax;
	LWORD(ebx) = regpack->r_bx;
	LWORD(ecx) = regpack->r_cx;
	LWORD(edx) = regpack->r_dx;
	LWORD(ebp) = regpack->r_bp;
	LWORD(esi) = regpack->r_si;
	LWORD(edi) = regpack->r_di;
	LWORD(ds) = regpack->r_ds;
	LWORD(es) = regpack->r_es;
	LWORD(eflags) = regpack->r_flags;
	call_vm86_from_com_thread(-1, intno);
	regpack->r_ax =  LWORD(eax);
	regpack->r_bx =  LWORD(ebx);
	regpack->r_cx =  LWORD(ecx);
	regpack->r_dx =  LWORD(edx);
	regpack->r_bp =  LWORD(ebp);
	regpack->r_si =  LWORD(esi);
	regpack->r_di =  LWORD(edi);
	regpack->r_ds =  LWORD(ds);
	regpack->r_es =  LWORD(es);
	regpack->r_flags =  LWORD(eflags);
}

int com_int86x(int intno, union com_REGS *inregs,
		union com_REGS *outregs, struct SREGS *segregs)
{
	LWORD(eax) = inregs->x.ax;
	LWORD(ebx) = inregs->x.bx;
	LWORD(ecx) = inregs->x.cx;
	LWORD(edx) = inregs->x.dx;
	LWORD(esi) = inregs->x.si;
	LWORD(edi) = inregs->x.di;
	LWORD(eflags) = inregs->x.flags;
	if (segregs) {
		LWORD(ds) = segregs->ds;
		LWORD(es) = segregs->es;
	}

	call_vm86_from_com_thread(-1, intno);

	outregs->x.ax =  LWORD(eax);
	outregs->x.bx =  LWORD(ebx);
	outregs->x.cx =  LWORD(ecx);
	outregs->x.dx =  LWORD(edx);
	outregs->x.si =  LWORD(esi);
	outregs->x.di =  LWORD(edi);
	outregs->x.flags =  LWORD(eflags);
	if (segregs) {
		segregs->ds =  LWORD(ds);
		segregs->es =  LWORD(es);
	}
	outregs->x.cflag = LWORD(eflags) & CF;
	return LWORD(eax);
}

int com_int86(int intno, union com_REGS *inregs, union com_REGS *outregs)
{
	return com_int86x(intno, inregs, outregs, 0);
}

int com_intdos(union com_REGS *inregs, union com_REGS *outregs)
{
	return com_int86x(0x21, inregs, outregs, 0);
}

int com_intdosx(union com_REGS *inregs, union com_REGS *outregs,
		struct SREGS *segregs)
{
	return com_int86x(0x21, inregs, outregs, segregs);
}

unsigned short com_peek(int seg, int off)
{
	return *((unsigned short *)SEGOFF2LINEAR(seg, off));
}

int com_bioskbd(int ax_value)
{
	int oldhog = config.hogthreshold;
	if (!ax_value) config.hogthreshold = 1;
	LWORD(eax) = ax_value;
	call_vm86_from_com_thread(-1, 0x16);    /* call BIOS */
	config.hogthreshold = oldhog;
	return LWORD(eax);
}

int com_biosvideo(int ax_value)
{
	LWORD(eax) = ax_value;
	call_vm86_from_com_thread(-1, 0x10);    /* call BIOS */
	return LWORD(eax);
}

int com_vsprintf(char *str, char *format, va_list ap)
{
	char *s = str;
	int i, size;

	mkscratch();
	size = vsnprintf(scratch, 0x10000, format, ap);
	for (i=0; i < size; i++) {
		if (scratch[i] == '\n') *s++ = '\r';
		*s++ = scratch[i];
	}
	*s = 0;
	return s - str;
}

int com_sprintf(char *str, char *format, ...)
{
	va_list ap;
	int ret;
	va_start(ap, format);

	ret = com_vsprintf(str, format, ap);
	va_end(ap);
	return ret;
}

int com_vfprintf(int dosfilefd, char *format, va_list ap)
{
	int size;

	mkscratch();
	size = com_vsprintf(scratch2, format, ap);
	if (!size) return 0;
	return com_doswrite(dosfilefd, scratch2, size);
}

int com_vprintf(char *format, va_list ap)
{
	return com_vfprintf(1, format, ap);
}

int com_fprintf(int dosfilefd, char *format, ...)
{
	va_list ap;
	int ret;
	va_start(ap, format);

	ret = com_vfprintf(dosfilefd, format, ap);
	va_end(ap);
	return ret;
}

int com_printf(char *format, ...)
{
	va_list ap;
	int ret;
	va_start(ap, format);

	ret = com_vfprintf(1, format, ap);
	va_end(ap);
	return ret;
}

int com_puts(char *s)
{
	return com_printf("%s", s);
}


struct lowstring {
	unsigned short len;
	char s[0];
} __attribute__((packed));

char * com_strdup(char *s)
{
	struct lowstring *p;
	int len = strlen(s) +1 + sizeof(struct lowstring);

	p = (void *)lowmem_alloc(len);
	if (!p) return 0;
	p->len = len;
	memcpy(p->s, s, len - sizeof(struct lowstring));
	return p->s;
}

void com_strfree(char *s)
{
	struct lowstring *p = (void *)(s -2);
	lowmem_free((char *)p, p->len);
}

/* ================================================================ */


static void install_hook_vector(void)
{
	char template[] = {
		0x2E, 0xA3, 0x08, 0x01,		/* movw	%ax, %cs:(0x108) */
		0xB8, DOS_HELPER_COOP, 0x02,	/* movw	$0x2c0,%ax */
		0xCD, DOS_HELPER_INT,		/* int	$0xe6 */
		0xCF				/* iret */
	};
	struct com_starter_seg  *ctcb = owntcb->params;
	struct com_hook_entry *ctable;
	int i, d, count;

	if (ctcb->hooks) return;

	count = NUM_COM_HOOKS;	/* number of hook entries */
	ctable = ctcb->hooks = (void *)lowmem_alloc((count*4)+sizeof(template));

	d = ((count -1) * 4) +1;
	for (i=0; i < count; i++, d -=4) {
		ctable[i].nearcallop = 0xe8;
		ctable[i].distpl = d;
		ctable[i].param = 0;
	}
	memcpy(ctable+count, template, sizeof(template));
	owntcb->hook_functions = malloc(count*4);
	memset(owntcb->hook_functions, 0, count*4);
}

static void recursive_call_handler(void)
{
	int fn = (int)HI(ax) - 3;
	if (fn < 0 && owntcb->hook_functions) {
		/* we come here on AH=2 for calling hook functions */
		struct com_starter_seg  *ctcb = owntcb->params;
		int addr;
		unsigned char * ssp = (unsigned char *)(LWORD(ss)<<4);
		unsigned long sp = (unsigned long) LWORD(esp);

		addr = popw(ssp, sp);
		LWORD(esp) += 2;
		LWORD(eax) = ctcb->saved_ax;
		fn = (addr - COM_OFFS_OF(ctcb->hooks) - 3) >> 2;
		if (fn >= NUM_COM_HOOKS) {
			com_fprintf(2, "weird calling of com_hooks\n");
			com_exit(1);
		}
		if ( ctcb->hooks[fn].param
				&& owntcb->hook_functions[fn]) {
			(*(owntcb->hook_functions[fn]))(ctcb->hooks[fn].param);
		}
		/* here now cs:ip are pointing to an iret in our stub
		 * so we do not need to emulate one
		 */
		return;
	}
	if (fn < owntcb->pent->num_functions) (*owntcb->pent->functions[fn])();
	else {
		com_fprintf(2, "non-registered plugin function called\n");
		com_exit(1);
	}
}


static void enter_vm86_from_com_thread(void)
{
	while (1) {
		/* we switch back to the main DOSEMU loop */
		switch_to(tcb0);

		/* When we come we have two conditions to hande:
		 *   A) return from a vm86 routine _we_ called_,
		 *	we then have AH == 1.
		 *   B) being called _by_ a vm86 routine (recursive entrency),
		 *	we then have AH > 1.
		 */
		if (HI(ax) <= 1) {
			return;
		}
		recursive_call_handler();
	}
}


void call_vm86_from_com_thread(int seg, int offs)
{
	/*
	 * Call a far/near or interrupt routine in vm86
	 *
	 *   seg  > 0	do a far call to seg:offs
	 *   seg == 0	do a near call to offs
	 *   seg  < 0	do an 'INT offs'
	 *
	 * On return all vm86 registers and flags are set as returned
	 * by the called routine/interrupt.
	 */
	volatile struct com_starter_seg  *ctcb;
	volatile unsigned short cs, ip;

	ctcb = owntcb->params;
	cs = LWORD(cs);
	ip = LWORD(eip);

	LWORD(cs) = COM_SEG;
	if (seg < 0) {
		LWORD(eip) = COM_OFFS_OF(&ctcb->intop);
		ctcb->intnum = offs;
	}
	else {
		unsigned char *ssp = (unsigned char *)(LWORD(ss)<<4);
		unsigned long sp = (unsigned long) LWORD(esp);

		if (seg >0) {
			pushw(ssp, sp, LWORD(cs));
			LWORD(cs) = seg;
			LWORD(esp) = (LWORD(esp) - 2) & 0xffff;
		}
		pushw(ssp, sp, COM_OFFS_OF(&ctcb->rethook));
		LWORD(eip) = offs;
		LWORD(esp) = (LWORD(esp) - 2) & 0xffff;
	}

	enter_vm86_from_com_thread();
	/* we have regain ctcb, because switches may have happened
	 * in between. To use owntcb is save, though.
	 */
	ctcb = owntcb->params;

	LWORD(eax) = ctcb->saved_ax;
	REG(cs) = cs;
	LWORD(eip) = ip;
}

void call_msdos(void)
{
	call_vm86_from_com_thread(-1, 0x21);
}

static struct com_program_entry *com_program_list = 0;


static struct com_program_entry *find_com_program(char *name)
{
	struct com_program_entry *com = com_program_list;

	while (com) {
		if (!strcmp(com->name, name)) return com;
		com = com->next;
	}
	return 0;
}

FAR_PTR register_com_hook_function(int intnum,
		com_hook_call_type *funct, int num, int param)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	struct vm86_regs saved_regs;
	FAR_PTR oldvec;

	if (owntcb == tcb0) return -1;
	if (num <0 || num >= NUM_COM_HOOKS || !ctcb->hooks) return -1;
	if (!funct) {
		/* unregister, but force param=0 too */
		param = 0;
	}
	ctcb->hooks[num].param = param;
	owntcb->hook_functions[num] = funct;

	if (!intnum) return 0;

	/* now establish the DOS interrupt and return the old vector */
	saved_regs = REGS; /* first save all x86 regs */
	LWORD(eax) = 0x3500 + intnum;
	call_msdos();
	oldvec = MK_FP(LWORD(es), LWORD(ebx));
	LWORD(ds) = COM_SEG;
	LWORD(edx) = COM_OFFS_OF(&ctcb->hooks[num]);
	LWORD(eax) = 0x2500 + intnum;
	call_msdos();
	REGS = saved_regs;
	return oldvec;
}



void register_com_program(char *name, com_program_type *program,
						char * flags, ...)
{
	va_list ap;
	struct com_program_entry *com;
	function_call_type **functions = 0;
	int stacksize = 512;
	int heapsize = 0x400;

	va_start(ap, flags);
	while (flags && *flags) switch (*flags++) {
	    case 'f':
		functions = va_arg(ap, function_call_type **); break;
	    case 's':
		stacksize = va_arg(ap, int); break;
	    case 'h':
		heapsize = va_arg(ap, int); break;
	}
	va_end(ap);

	if ((com = find_com_program(name)) == 0) {
		com = malloc(sizeof(struct com_program_entry));
		if (!com) return;
		com->next = com_program_list;
		com_program_list = com;
	}
	com->name = name;
	com->program = program;
	com->functions = functions;
	com->stacksize = stacksize;
	com->heapsize = heapsize;
	com->num_functions = 0;
	if (functions) while (*(functions++)) com->num_functions++;
}

static void com_fatal_handler(int param)
{
   switch(param) {
     case 0x23:	/* Ctrl-C */
	/*
	 * Ok, MSDOS docs says you have to return with CARRY if you
	 * want to abort the program in an CtrlC handler.
	 * However, it also says "there is _no_ restriction in what
	 * the handler can do, including MSDOS functions".
	 * Now, we make use of this and terminate by an MSDOS function;-)
	 * The problem with returning to DOS is, that we don't get called
	 * later to delete our thread. Hence we do it here.
	 */
	com_exit(1);

     case 0x24: { /* critical error */
	/* We hook this one only to get 'early information' about
	 * aborting the program and forcing 'call int23' in doubtful
	 * cases.
	 */
	struct vm86_regs saved_regs = REGS; /* first save all x86 regs */
	struct com_starter_seg  *ctcb = owntcb->params;
	unsigned char *ssp = (unsigned char *)(LWORD(ss)<<4);
	unsigned long sp = (unsigned long) LWORD(esp);
	int promptreturn;

	pushw(ssp, sp, LWORD(eflags));
	LWORD(esp) -= 2;
	call_vm86_from_com_thread(
		FP_SEG16(ctcb->saved_int24),
		FP_OFF16(ctcb->saved_int24));
	promptreturn = LO(ax);
	REGS = saved_regs;
	switch (promptreturn) {
		case 1:	/* retry operation */
			break;
		case 0:	/* ignore */
		case 2:	/* Abort issuing interrupt 23. */
		case 3: /* fail system call in progress */
		default:
			LO(ax) = 2;
			break;
	}
     }
   }
}

static void install_com_fatal_handlers(void)
{
	struct com_starter_seg  *ctcb = owntcb->params;

	/* install our own INT 23/24 handlers */
	ctcb->saved_int23 =
		register_com_hook_function(0x23, com_fatal_handler, 0, 0x23);
	ctcb->saved_int24 =
		register_com_hook_function(0x24, com_fatal_handler, 1, 0x24);
}

int com_argparse(char *s, char **argvx, int maxarg)
{
   int mode = 0;
   int argcx = 0;
   char delim = 0;
   char *p;

   p = strchr(s+1, '\r');
   if (p && ((p - (s+1)) < 128)) {
     *p = 0;
   }
   else s[1+(unsigned char)s[0]] = 0;
   s++;

   /* transform:
    *    dir/p to dir /p
    *    cd\ to cd \
    *    cd.. to cd ..
    */
   p = s + strcspn(s, "\\/. ");
   if (*p && *p != ' ' && (*p != '.' || (p[1] && strchr("\\/.", p[1])))) {
      memmove(p+1, p, s [-1] - (p - s) + 1/*NUL*/);
      *p = ' ';
      s[-1]++; /* update length */
   }

   maxarg --;
   for ( ; *s; s++) {
      if (!mode) {
         if (*s > ' ') {
            mode = 1;
            switch (*s) {
              case '"':
              case '\'':
                delim = *s;
                mode = 2;
                argvx[argcx++] = s+1;
                break;
              default:
                argvx[argcx++] = s;
                break;
            }
            if (argcx >= maxarg)
               break;
         }
      } else if (mode == 1) {
         if (*s <= ' ') {
            mode = 0;
            *s = 0;
         }
      } else {
         if (*s == delim) {
           *s = 0;
           mode = 1;
         }
      }
   }
   argvx[argcx] = 0;
   return(argcx);
}

static void starter_thread(void *params)
{
    {
	struct com_starter_seg  *ctcb = params;
	int argc;
	char *cmdline;
	char **argv;

	/* save regs we will store on exit */
	ctcb->oldcs = REG(cs);
	ctcb->oldip = LWORD(eip);
	ctcb->oldsp = LWORD(esp);

	/* do 'crt0' equivalent stuff
	 */

	/* first parse commandline */
	cmdline = lowmem_alloc(128);
	memcpy(cmdline, ctcb->DTA, 128);
	argv = (void *)(cmdline+128);
	argc = com_argparse(cmdline, argv+1, 62) + 1;
	lowmem_alloc((argc+1)*4);
	argv[0] = owntcb->pent->name;

	/* now set the DOS version */
	HI(ax) = 0x30;
	call_msdos();
	_osmajor = LO(ax);
	_osminor = HI(ax);

	/* prepare the hooks vector */
	install_hook_vector();

	/* install own INT 23/24 handlers */
	install_com_fatal_handlers();

	/* call the program, but do setjmp for 'exit()' */
	if (!setjmp(NONVOLATILE_OWNTCB->exit_context)) {
		LWORD(eax) = (*(owntcb->pent->program))(argc, argv);
	}
    }
    /* optimization barrier */
    {
	/* we have to regain ctcb, because switches may have happened
	 * in between. To use owntcb is save, though.
	 */
	struct com_starter_seg  *ctcb = owntcb->params;

	if (ctcb->mode_flags & CDEBUG || DEBUG > 3) {
		c_printf("coopthreads: PSP %04x terminated, heap used 0x%x bytes\n",
			ctcb->real_psp, ctcb->heapptr - ctcb->heapstart);
	}

	LWORD(cs) = ctcb->oldcs;
	LWORD(ss) = ctcb->oldcs;	/* ... just to be save */
	LWORD(eip) = ctcb->oldip;
	LWORD(esp) = ctcb->oldsp;

	/* We now delete our own thread and switch to thread0.
	 * The DOSEMU mainloop (thread0) will return from dosemu_helper
	 * and restart DOS at the point we set the CS:IP to (see above).
	 */
	delete_thread(0, tcb0);

	/* we should never come here */
	while (1);
    }
}

static char *skip_white_and_delim(char *s, int delim)
{
	while (*s && isspace(*s)) s++;
	if (*s == delim) s++;
	while (*s && isspace(*s)) s++;
	return s;
}

static void create_com_thread(void)
{
	static char hooktemplate[] = {
				    /* intop:				*/
	  0xcd, 3,			/* int <intnum>			*/
				    /* rethook:				*/
	  0x2e, 0xa3, 0x08, 0x01,	/* mov cs:[108h],ax		*/
	  0xb8, DOS_HELPER_COOP, 0x01,	/* mov ax,0x100+DOS_HELPER_COOP */
	  0xcd, DOS_HELPER_INT		/* int DOS_HELPER_INT		*/
	};
	struct com_starter_seg  *ctcb = (void *)(LWORD(cs)<<4);
	struct com_MCB *mcb = SEG2LINEAR(ctcb->real_psp-1);
	char *s, pname_[256], *pname = "";
	int stacksize = 0;
	int heapsize = 0;

	if (ctcb->magic != COM_THREAD_MAGIC) return;
	if (ctcb->tcb) return;	/* we are wrong here */
	ctcb->tcb = create_thread(starter_thread, 0x40000, ctcb);

	/* At start of a com program we expect execution parameters
	 * on the heap: a string such as
	 *
	 *   stacksize, heapsize, flags, programname
	 *
	 * Any of it may be missing and will be replaced by defaults,
	 * if a leading or in-between number is missing, the comma (',')
	 * as place holder has to be there at least.
	 *
	 * stacksize	Number, space between top of stack and heapend
	 *		(default MIN_STACK_HEAP_PADDING = 1024)
	 * heapsize	Number, size of the lowmem heap
	 *		(default: what's left in 64K)
	 *		granularity is LOWMEM_HEAP_GRAN = 128
	 * flags	Number, will be copied to ctcb->mode_flags
	 * programname  String, name of the registered 32bit code to execute
	 *		This may differ from the DOS name. Code must have
	 *		been registered by register_com_program().
	 *		(default: the name found in the mcb)
	 */
	s = ((char *)ctcb) + ctcb->heapstart;

		/* try to get stacksize */
	s = skip_white_and_delim(s, ' ');
	stacksize = strtoul(s, &s, 0);

		/* try to get heapsize */
	s = skip_white_and_delim(s, ',');
	heapsize = strtoul(s, &s, 0);

		/* try to get mode_flags */
	s = skip_white_and_delim(s, ',');
	ctcb->mode_flags = strtoul(s, &s, 0);

		/* try to get program name */
	s = skip_white_and_delim(s, ',');
	pname = s;
	while (*s && !isspace(*s)) s++;
	*s = 0;

	if (!pname[0]) {
		pname = strncpy(pname_, mcb->name, 8);
		pname[8] = 0;
		s = pname;
		while (*s && !isspace(*s)) {
			*s = tolower(*s);
			s++;
		}
		*s = 0;
	}
	if (!pname[0]) pname = "default_com_program";
	ctcb->tcb->pent = find_com_program(pname);
	if (!ctcb->tcb->pent) ctcb->tcb->pent = find_com_program("default_com_program");

	if (!stacksize) stacksize = ctcb->tcb->pent->stacksize;
	if (!heapsize)  heapsize = ctcb->tcb->pent->heapsize;

	ctcb->heapstart = roundup_lowmem_heap_size(ctcb->heapstart);
	if (stacksize < MIN_STACK_HEAP_PADDING)
		stacksize = MIN_STACK_HEAP_PADDING;
	if (!heapsize) {
		ctcb->brkpoint = (LWORD(esp) + 15) & -16;
		ctcb->heapend = (ctcb->brkpoint-stacksize) & -LOWMEM_HEAP_GRAN;
	}
	else {
		stacksize &= -4;
		heapsize = roundup_lowmem_heap_size(heapsize + ctcb->heapstart);
		if ((heapsize + stacksize) >=  LWORD(esp)) {
			error("coopthreads: lowmem heap + stack are too big\n");
			leavedos(99);
		}
		ctcb->heapend = heapsize;
		ctcb->brkpoint = LWORD(esp) = ctcb->heapend + stacksize;
	}
	ctcb->heapptr = ctcb->heapstart;

	if (ctcb->mode_flags & CDEBUG || DEBUG > 3) {
	    c_printf(
		"coopthreads: %s started, PSP %04x, heapsize 0x%x, stacksize 0x%x, "
		"thread %p, stack32bottom=%p, stack32size=0x%lx\n",
		pname, ctcb->real_psp, ctcb->heapend - ctcb->heapstart, stacksize,
		ctcb->tcb, ctcb->tcb->stack, ctcb->tcb->stack_size
	    );
	}

	memset(ctcb->heapmap, 0, sizeof(ctcb->heapmap));

	/* we now set our return hook */
	memcpy(&ctcb->intop, hooktemplate, sizeof(hooktemplate));

	/* now we switch to the just created thread
	 * and come not back until this thread itself has switch
	 * to us, either for calling a DOS service or for termination
	 * Whenever we come here, we are running thread0 (aka DOSEMU mainloop)
	 */
	switch_to(ctcb->tcb);
}

static void handle_com_thread_return(void)
{
	struct com_starter_seg  *ctcb = (void *)(LWORD(cs)<<4);
	switch_to(ctcb->tcb);
}

/* ================================================================ */

static int default_com_program(int argc, char **argv)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	char *p;

	p = lowmem_alloc(256);
	LWORD(ecx) = sprintf(p, "attempt to execute unknow builtin\r\n");
	LWORD(ebx) = 1;	/* stdout */
	LWORD(ds) = COM_SEG;
	LWORD(edx) = COM_OFFS_OF(p);
	LWORD(eax) = 0x4000;
	call_msdos();	/* MSDOS write handle */
	lowmem_free(p, 256);
	return 0;
}

static void default_functions(void)
{
#if 0
	fprintf(stderr, "default_function, cs:ip = %x:%x, sp = %x\n",
		LWORD(cs), LWORD(eip), LWORD(esp));
	call_vm86_from_com_thread(LWORD(cs), 0x1a0);
	fprintf(stderr, "back from call_vm86_from_com_thread(CS, 0x1a0)\n");
#endif
}

static function_call_type *default_com_program_functions[] = {
	default_functions,
	0
};


/* ================================================================ */
static char *chistname = 0;

void coopthreads_plugin_init(void)
{
	COM_CHECK_PERMISSIONS

		/* 16 Meg stack should be enough for main DOSEMU thread */
	tcb0 = init_zero_thread(0x100000);
	if (!tcb0) return;
	register_com_program("default_com_program",
		default_com_program, "f", default_com_program_functions);

	/* try to load old history file for comcom */
	chistname = get_path_in_HOME(".dosemu/comcom.history");
	if (exists_file(chistname)) {
		load_comcom_history(chistname);
	}

	register_com_program("comcom", comcom_main, 0);

#if 0
	fprintf(stderr, "PLUGIN: coopthreads_plugin_init called\n");
	fprintf(stderr, "PLUGIN: thread0=%p, stackptr=0x%lx, stackbottom=%p, stacksize=0x%lx\n",
		tcb0, STACKPTR, tcb0->stack, tcb0->stack_size);
#endif
}


void coopthreads_plugin_close(void)
{
	if (!tcb0) return;
	save_comcom_history(chistname);
#if 0
	fprintf(stderr, "PLUGIN: coopthreads_plugin_close called\n");
#endif
}


int coopthreads_plugin_inte6(void)
{
	if (!tcb0) coopthreads_plugin_init();

	switch (HI(ax)) {
		case 0: {
			/* program start */
			create_com_thread();
			break;
		}
		default: {
			/* return from vm86 service, which _we_ called
			 * out of a com thread, or handle a recursive
			 * call to us, which was called by the service
			 * we called.
			 */
			handle_com_thread_return();
			break;
		}
	}
	return 1;
}

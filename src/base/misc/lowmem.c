/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Author: Stas Sergeev <stsp@users.sourceforge.net>
 *
 * Management for the static 32K heap in a low memory.
 * Used by various dosemu internal subsystems.
 */

#include <signal.h>
#include <assert.h>
#include "emu.h"
#include "memory.h"
#include "smalloc.h"
#include "utilities.h"
#include "dpmi.h"
#include "lowmem.h"

static smpool mp;
unsigned char *dosemu_lmheap_base;

static void do_sm_error(int prio, char *fmt, ...)
{
    char buf[1024];
    va_list al;

    va_start(al, fmt);
    vsnprintf(buf, sizeof(buf), fmt, al);
    va_end(al);
    if (prio)
	dosemu_error("%s\n", buf);
    else
	dbug_printf("%s\n", buf);
}

int lowmem_heap_init()
{
    dosemu_lmheap_base = MK_FP32(DOSEMU_LMHEAP_SEG, DOSEMU_LMHEAP_OFF);
    smregister_default_error_notifier(do_sm_error);
    sminit(&mp, dosemu_lmheap_base, DOSEMU_LMHEAP_SIZE);
    return 1;
}

void * lowmem_heap_alloc(int size)
{
	char *ptr = smalloc(&mp, size);
	if (!ptr) {
		error("lowmem_heap: OOM, size=%i\n", size);
		leavedos(86);
	}
	return ptr;
}

void lowmem_heap_free(void *p)
{
	return smfree(&mp, p);
}


#define RM_STACK_SIZE 0x200
static void *rm_stack;
static int in_rm_stack;

int get_rm_stack(Bit16u *ss_p, Bit16u *sp_p)
{
	if (!(in_rm_stack++)) {
		rm_stack = lowmem_heap_alloc(RM_STACK_SIZE);
		assert(rm_stack);
		*ss_p = DOSEMU_LMHEAP_SEG;
		*sp_p = DOSEMU_LMHEAP_OFFS_OF(rm_stack) + RM_STACK_SIZE;
		return 1;
	}
	return 0;
}

void put_rm_stack(void)
{
	assert(in_rm_stack > 0);
	if (!(--in_rm_stack))
		lowmem_heap_free(rm_stack);
}

/* recursion is _very_unlikely, but define an array */
#define MAX_SAVED_REGS 5
static struct vm86_regs rm_regs_stack[MAX_SAVED_REGS];

static void switch_stack(struct vm86_regs *regs)
{
	Bit16u new_ss, new_sp;
	int stk;
	stk = get_rm_stack(&new_ss, &new_sp);
	if (stk) {
		regs->ss = new_ss;
		regs->esp = new_sp;
	}
}

void get_rm_stack_regs(struct vm86_regs *regs, struct vm86_regs *saved_regs)
{
	if(in_dpmi_pm())
		fake_pm_int();
	*saved_regs = REGS;
	switch_stack(regs);
}

void rm_stack_enter(void)
{
	assert(in_rm_stack < MAX_SAVED_REGS);
	get_rm_stack_regs(&REGS, &rm_regs_stack[in_rm_stack]);
}

void rm_stack_leave(void)
{
	put_rm_stack();
	REGS = rm_regs_stack[in_rm_stack];
}

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
 * Purpose: coopthreadized DPMI API that resembles one of djgpp's dpmi.h.
 *
 * Author: Stas Sergeev
 */
#include <stdint.h>
#include "smalloc.h"
#include "emudpmi.h"
#include "dpmisel.h"
#include "coopth.h"
#include "dosemu_debug.h"
#include "dpmi_api.h"

short __dpmi_int_ss, __dpmi_int_sp, __dpmi_int_flags;

static uint16_t data_sel;
static unsigned char *pool_base;
static smpool apool;

#define POOL_OFS(p) ((unsigned char *)(p) - pool_base)

static void do_callf(cpuctx_t *scp, int is_32, struct pmaddr_s pma)
{
    void *sp = SEL_ADR(_ss, _esp);
    if (is_32) {
	unsigned int *ssp = sp;
	*--ssp = _cs;
	*--ssp = _eip;
	_esp -= 8;
    } else {
	unsigned short *ssp = sp;
	*--ssp = _cs;
	*--ssp = _LWORD(eip);
	_LWORD(esp) -= 4;
    }
    _cs = pma.selector;
    _eip = pma.offset;
    coopth_sched();
}

static void do_dpmi_callf(cpuctx_t *scp, int is_32)
{
    struct pmaddr_s pma = {
	.offset = DPMI_SEL_OFF(DPMI_call),
	.selector = dpmi_sel(),
    };
    do_callf(scp, is_32, pma);
}

void _dpmi_yield(cpuctx_t *scp, int is_32)
{									/* INT 0x2F AX=1680 */
}

int _dpmi_allocate_ldt_descriptors(cpuctx_t *scp, int is_32, int _count)
{						/* DPMI 0.9 AX=0000 */
    return 0;
}

int _dpmi_free_ldt_descriptor(cpuctx_t *scp, int is_32, int _descriptor)
{						/* DPMI 0.9 AX=0001 */
    return 0;
}

int _dpmi_segment_to_descriptor(cpuctx_t *scp, int is_32, int _segment)
{						/* DPMI 0.9 AX=0002 */
    return 0;
}

int _dpmi_get_selector_increment_value(cpuctx_t *scp, int is_32)
{						/* DPMI 0.9 AX=0003 */
    return 0;
}

int _dpmi_get_segment_base_address(cpuctx_t *scp, int is_32, int _selector, ULONG *_addr)
{			/* DPMI 0.9 AX=0006 */
    return 0;
}

int _dpmi_set_segment_base_address(cpuctx_t *scp, int is_32, int _selector, ULONG _address)
{			/* DPMI 0.9 AX=0007 */
    return 0;
}

ULONG _dpmi_get_segment_limit(cpuctx_t *scp, int is_32, int _selector)
{						/* LSL instruction  */
    return 0;
}

int _dpmi_set_segment_limit(cpuctx_t *scp, int is_32, int _selector, ULONG _limit)
{				/* DPMI 0.9 AX=0008 */
    return 0;
}

int _dpmi_get_descriptor_access_rights(cpuctx_t *scp, int is_32, int _selector)
{					/* LAR instruction  */
    return 0;
}

int _dpmi_set_descriptor_access_rights(cpuctx_t *scp, int is_32, int _selector, int _rights)
{			/* DPMI 0.9 AX=0009 */
    return 0;
}

int _dpmi_create_alias_descriptor(cpuctx_t *scp, int is_32, int _selector)
{						/* DPMI 0.9 AX=000a */
    return 0;
}

int _dpmi_get_descriptor(cpuctx_t *scp, int is_32, int _selector, void *_buffer)
{					/* DPMI 0.9 AX=000b */
    return 0;
}

int _dpmi_set_descriptor(cpuctx_t *scp, int is_32, int _selector, void *_buffer)
{					/* DPMI 0.9 AX=000c */
    return 0;
}

int _dpmi_allocate_specific_ldt_descriptor(cpuctx_t *scp, int is_32, int _selector)
{					/* DPMI 0.9 AX=000d */
    return 0;
}

int _dpmi_get_multiple_descriptors(cpuctx_t *scp, int is_32, int _count, void *_buffer)
{				/* DPMI 1.0 AX=000e */
    return 0;
}

int _dpmi_set_multiple_descriptors(cpuctx_t *scp, int is_32, int _count, void *_buffer)
{				/* DPMI 1.0 AX=000f */
    return 0;
}

int _dpmi_allocate_dos_memory(cpuctx_t *scp, int is_32, int _paragraphs, int *_ret_selector_or_max)
{			/* DPMI 0.9 AX=0100 */
    return 0;
}

int _dpmi_free_dos_memory(cpuctx_t *scp, int is_32, int _selector)
{							/* DPMI 0.9 AX=0101 */
    return 0;
}

int _dpmi_resize_dos_memory(cpuctx_t *scp, int is_32, int _selector, int _newpara, int *_ret_max)
{			/* DPMI 0.9 AX=0102 */
    return 0;
}

int _dpmi_get_real_mode_interrupt_vector(cpuctx_t *scp, int is_32, int _vector, __dpmi_raddr *_address)
{		/* DPMI 0.9 AX=0200 */
    return 0;
}

int _dpmi_set_real_mode_interrupt_vector(cpuctx_t *scp, int is_32, int _vector, __dpmi_raddr *_address)
{		/* DPMI 0.9 AX=0201 */
    return 0;
}

int _dpmi_get_processor_exception_handler_vector(cpuctx_t *scp, int is_32, int _vector, __dpmi_paddr *_address)
{	/* DPMI 0.9 AX=0202 */
    return 0;
}

int _dpmi_set_processor_exception_handler_vector(cpuctx_t *scp, int is_32, int _vector, __dpmi_paddr *_address)
{	/* DPMI 0.9 AX=0203 */
    return 0;
}

int _dpmi_get_protected_mode_interrupt_vector(cpuctx_t *scp, int is_32, int _vector, __dpmi_paddr *_address)
{	/* DPMI 0.9 AX=0204 */
    return 0;
}

int _dpmi_set_protected_mode_interrupt_vector(cpuctx_t *scp, int is_32, int _vector, __dpmi_paddr *_address)
{	/* DPMI 0.9 AX=0205 */
    return 0;
}

int _dpmi_get_extended_exception_handler_vector_pm(cpuctx_t *scp, int is_32, int _vector, __dpmi_paddr *_address)
{	/* DPMI 1.0 AX=0210 */
    return 0;
}

int _dpmi_get_extended_exception_handler_vector_rm(cpuctx_t *scp, int is_32, int _vector, __dpmi_paddr *_address)
{	/* DPMI 1.0 AX=0211 */
    return 0;
}

int _dpmi_set_extended_exception_handler_vector_pm(cpuctx_t *scp, int is_32, int _vector, __dpmi_paddr *_address)
{	/* DPMI 1.0 AX=0212 */
    return 0;
}

int _dpmi_set_extended_exception_handler_vector_rm(cpuctx_t *scp, int is_32, int _vector, __dpmi_paddr *_address)
{	/* DPMI 1.0 AX=0213 */
    return 0;
}

int _dpmi_simulate_real_mode_interrupt(cpuctx_t *scp, int is_32, int _vector, __dpmi_regs *__regs)
{			/* DPMI 0.9 AX=0300 */
    cpuctx_t sa = *scp;
    __dpmi_regs *regs = smalloc(&apool, sizeof(*regs));

    _eax = 0x300;
    _ebx = _vector;
    _ecx = 0;
    _es = data_sel;
    _edi = POOL_OFS(regs);
    memcpy(regs, __regs, sizeof(*regs));
    D_printf("MSDOS: sched to dos thread for int 0x%x\n", _vector);
    do_dpmi_callf(scp, is_32);
    D_printf("MSDOS: return from dos thread\n");
    memcpy(__regs, regs, sizeof(*regs));
    smfree(&apool, regs);
    *scp = sa;
    return 0;
}

int _dpmi_int(cpuctx_t *scp, int is_32, int _vector, __dpmi_regs *__regs)
{ /* like above, but sets ss sp fl */	/* DPMI 0.9 AX=0300 */
    cpuctx_t sa = *scp;
    __dpmi_regs *regs = smalloc(&apool, sizeof(*regs));

    _eax = 0x300;
    _ebx = _vector;
    _ecx = 0;
    _es = data_sel;
    _edi = POOL_OFS(regs);
    __regs->x.ss = __dpmi_int_ss;
    __regs->x.sp = __dpmi_int_sp;
    __regs->x.flags = __dpmi_int_flags;
    memcpy(regs, __regs, sizeof(*regs));
    D_printf("MSDOS: sched to dos thread for int 0x%x\n", _vector);
    do_dpmi_callf(scp, is_32);
    D_printf("MSDOS: return from dos thread\n");
    memcpy(__regs, regs, sizeof(*regs));
    smfree(&apool, regs);
    *scp = sa;
    return 0;
}

static void do_procedure_retf(cpuctx_t *scp,
	int is_32, __dpmi_regs *__regs, int words)
{				/* DPMI 0.9 AX=0301 */
    struct pmaddr_s pma = {
	.offset = DPMI_SEL_OFF(DPMI_call_args),
	.selector = dpmi_sel(),
    };
    struct pmaddr_s pma16 = {
	.offset = DPMI_SEL_OFF(DPMI_call_args16),
	.selector = dpmi_sel(),
    };
    cpuctx_t sa = *scp;
    __dpmi_regs *regs = smalloc(&apool, sizeof(*regs));

    _eax = 0x301;
    _ebx = 0;
    _ecx = words;
    _es = data_sel;
    _edi = POOL_OFS(regs);
    memcpy(regs, __regs, sizeof(*regs));
    D_printf("MSDOS: sched to dos thread for call to %x:%x\n",
	    __regs->x.cs, __regs->x.ip);
    do_callf(scp, is_32, is_32 ? pma : pma16);
    D_printf("MSDOS: return from dos thread\n");
    memcpy(__regs, regs, sizeof(*regs));
    smfree(&apool, regs);
    *scp = sa;
}

int _dpmi_simulate_real_mode_procedure_retf(cpuctx_t *scp, int is_32,
	__dpmi_regs *__regs)
{				/* DPMI 0.9 AX=0301 */
    do_procedure_retf(scp, is_32, __regs, 0);
    return 0;
}

int _dpmi_simulate_real_mode_procedure_retf_stack(cpuctx_t *scp, int is_32,
	__dpmi_regs *__regs, int stack_words_to_copy, const void *stack_data)
{ /* DPMI 0.9 AX=0301 */
    unsigned short *sp = SEL_ADR_CLNT(_ss, _esp, is_32);
    const unsigned short *d = stack_data;
    int i;

    for (i = 0; i < stack_words_to_copy; i++)
	*--sp = d[stack_words_to_copy - 1 - i];
    if (is_32)
	_esp -= stack_words_to_copy * 2;
    else
	_LWORD(esp) -= stack_words_to_copy * 2;
    do_procedure_retf(scp, is_32, __regs, stack_words_to_copy);
    if (is_32)
	_esp += stack_words_to_copy * 2;
    else
	_LWORD(esp) += stack_words_to_copy * 2;
    return 0;
}

int _dpmi_simulate_real_mode_procedure_iret(cpuctx_t *scp, int is_32, __dpmi_regs *__regs)
{				/* DPMI 0.9 AX=0302 */
    cpuctx_t sa = *scp;
    __dpmi_regs *regs = smalloc(&apool, sizeof(*regs));

    _eax = 0x302;
    _ebx = 0;
    _ecx = 0;
    _es = data_sel;
    _edi = POOL_OFS(regs);
    memcpy(regs, __regs, sizeof(*regs));
    D_printf("MSDOS: sched to dos thread for call to %x:%x\n",
	    __regs->x.cs, __regs->x.ip);
    do_dpmi_callf(scp, is_32);
    D_printf("MSDOS: return from dos thread\n");
    memcpy(__regs, regs, sizeof(*regs));
    smfree(&apool, regs);
    *scp = sa;
    return 0;
}

int _dpmi_allocate_real_mode_callback(cpuctx_t *scp, int is_32, void (*_handler)(void), __dpmi_regs *__regs, __dpmi_raddr *_ret)
{ /* DPMI 0.9 AX=0303 */
    return 0;
}

int _dpmi_free_real_mode_callback(cpuctx_t *scp, int is_32, __dpmi_raddr *_addr)
{					/* DPMI 0.9 AX=0304 */
    return 0;
}

int _dpmi_get_state_save_restore_addr(cpuctx_t *scp, int is_32, __dpmi_raddr *_rm, __dpmi_paddr *_pm)
{		/* DPMI 0.9 AX=0305 */
    return 0;
}

int _dpmi_get_raw_mode_switch_addr(cpuctx_t *scp, int is_32, __dpmi_raddr *_rm, __dpmi_paddr *_pm)
{			/* DPMI 0.9 AX=0306 */
    return 0;
}

int _dpmi_get_version(cpuctx_t *scp, int is_32, __dpmi_version_ret *_ret)
{						/* DPMI 0.9 AX=0400 */
    return 0;
}

int _dpmi_get_capabilities(cpuctx_t *scp, int is_32, int *_flags, char *vendor_info)
{				/* DPMI 1.0 AX=0401 */
    return 0;
}

int _dpmi_get_free_memory_information(cpuctx_t *scp, int is_32, __dpmi_free_mem_info *_info)
{			/* DPMI 0.9 AX=0500 */
    return 0;
}

int _dpmi_allocate_memory(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info)
{						/* DPMI 0.9 AX=0501 */
    return 0;
}

int _dpmi_free_memory(cpuctx_t *scp, int is_32, ULONG _handle)
{						/* DPMI 0.9 AX=0502 */
    return 0;
}

int _dpmi_resize_memory(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info)
{						/* DPMI 0.9 AX=0503 */
    return 0;
}

int _dpmi_allocate_linear_memory(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info, int _commit)
{			/* DPMI 1.0 AX=0504 */
    return 0;
}

int _dpmi_resize_linear_memory(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info, int _commit)
{			/* DPMI 1.0 AX=0505 */
    return 0;
}

int _dpmi_get_page_attributes(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info, short *_buffer)
{			/* DPMI 1.0 AX=0506 */
    return 0;
}

int _dpmi_set_page_attributes(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info, short *_buffer)
{			/* DPMI 1.0 AX=0507 */
    return 0;
}

int _dpmi_map_device_in_memory_block(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info, ULONG _physaddr)
{	/* DPMI 1.0 AX=0508 */
    return 0;
}

int _dpmi_map_conventional_memory_in_memory_block(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info, ULONG _physaddr)
{ /* DPMI 1.0 AX=0509 */
    return 0;
}

int _dpmi_get_memory_block_size_and_base(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info)
{				/* DPMI 1.0 AX=050a */
    return 0;
}

int _dpmi_get_memory_information(cpuctx_t *scp, int is_32, __dpmi_memory_info *_buffer)
{				/* DPMI 1.0 AX=050b */
    return 0;
}

int _dpmi_lock_linear_region(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info)
{					/* DPMI 0.9 AX=0600 */
    return 0;
}

int _dpmi_unlock_linear_region(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info)
{					/* DPMI 0.9 AX=0601 */
    return 0;
}

int _dpmi_mark_real_mode_region_as_pageable(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info)
{			/* DPMI 0.9 AX=0602 */
    return 0;
}

int _dpmi_relock_real_mode_region(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info)
{					/* DPMI 0.9 AX=0603 */
    return 0;
}

int _dpmi_get_page_size(cpuctx_t *scp, int is_32, ULONG *_size)
{						/* DPMI 0.9 AX=0604 */
    return 0;
}

int _dpmi_mark_page_as_demand_paging_candidate(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info)
{			/* DPMI 0.9 AX=0702 */
    return 0;
}

int _dpmi_discard_page_contents(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info)
{					/* DPMI 0.9 AX=0703 */
    return 0;
}

int _dpmi_physical_address_mapping(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info)
{					/* DPMI 0.9 AX=0800 */
    return 0;
}

int _dpmi_free_physical_address_mapping(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info)
{				/* DPMI 0.9 AX=0801 */
    return 0;
}

/* These next four functions return the old state */
int _dpmi_get_and_disable_virtual_interrupt_state(cpuctx_t *scp, int is_32)
{					/* DPMI 0.9 AX=0900 */
    return 0;
}

int _dpmi_get_and_enable_virtual_interrupt_state(cpuctx_t *scp, int is_32)
{					/* DPMI 0.9 AX=0901 */
    return 0;
}

int _dpmi_get_and_set_virtual_interrupt_state(cpuctx_t *scp, int is_32, int _old_state)
{				/* DPMI 0.9 AH=09   */
    return 0;
}

int _dpmi_get_virtual_interrupt_state(cpuctx_t *scp, int is_32)
{						/* DPMI 0.9 AX=0902 */
    return 0;
}

int _dpmi_get_vendor_specific_api_entry_point(cpuctx_t *scp, int is_32, char *_id, __dpmi_paddr *_api)
{		/* DPMI 0.9 AX=0a00 */
    return 0;
}

int _dpmi_set_debug_watchpoint(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info, int _type)
{				/* DPMI 0.9 AX=0b00 */
    return 0;
}

int _dpmi_clear_debug_watchpoint(cpuctx_t *scp, int is_32, ULONG _handle)
{					/* DPMI 0.9 AX=0b01 */
    return 0;
}

int _dpmi_get_state_of_debug_watchpoint(cpuctx_t *scp, int is_32, ULONG _handle, int *_status)
{		/* DPMI 0.9 AX=0b02 */
    return 0;
}

int _dpmi_reset_debug_watchpoint(cpuctx_t *scp, int is_32, ULONG _handle)
{					/* DPMI 0.9 AX=0b03 */
    return 0;
}

int _dpmi_install_resident_service_provider_callback(cpuctx_t *scp, int is_32, __dpmi_callback_info *_info)
{		/* DPMI 1.0 AX=0c00 */
    return 0;
}

int _dpmi_terminate_and_stay_resident(cpuctx_t *scp, int is_32, int return_code, int paragraphs_to_keep)
{		/* DPMI 1.0 AX=0c01 */
    return 0;
}

int _dpmi_allocate_shared_memory(cpuctx_t *scp, int is_32, __dpmi_shminfo *_info)
{					/* DPMI 1.0 AX=0d00 */
    return 0;
}

int _dpmi_free_shared_memory(cpuctx_t *scp, int is_32, ULONG _handle)
{					/* DPMI 1.0 AX=0d01 */
    return 0;
}

int _dpmi_serialize_on_shared_memory(cpuctx_t *scp, int is_32, ULONG _handle, int _flags)
{			/* DPMI 1.0 AX=0d02 */
    return 0;
}

int _dpmi_free_serialization_on_shared_memory(cpuctx_t *scp, int is_32, ULONG _handle, int _flags)
{		/* DPMI 1.0 AX=0d03 */
    return 0;
}

int _dpmi_get_coprocessor_status(cpuctx_t *scp, int is_32)
{							/* DPMI 1.0 AX=0e00 */
    return 0;
}

int _dpmi_set_coprocessor_emulation(cpuctx_t *scp, int is_32, int _flags)
{						/* DPMI 1.0 AX=0e01 */
    return 0;
}

void dpmi_api_init(uint16_t selector, dosaddr_t pool, int pool_size)
{
    data_sel = selector;
    pool_base = MEM_BASE32(pool);
    sminit(&apool, pool_base, pool_size);
}

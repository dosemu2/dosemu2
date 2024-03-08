#ifndef DPMI_API_H
#define DPMI_API_H

#include "cpu.h"
#include "memory.h"
#include "djdpmi.h"

void	_dpmi_yield(cpuctx_t *scp, int is_32);									/* INT 0x2F AX=1680 */

int	_dpmi_allocate_ldt_descriptors(cpuctx_t *scp, int is_32, int _count);						/* DPMI 0.9 AX=0000 */
int	_dpmi_free_ldt_descriptor(cpuctx_t *scp, int is_32, int _descriptor);						/* DPMI 0.9 AX=0001 */
int	_dpmi_segment_to_descriptor(cpuctx_t *scp, int is_32, int _segment);						/* DPMI 0.9 AX=0002 */
int	_dpmi_get_selector_increment_value(cpuctx_t *scp, int is_32);						/* DPMI 0.9 AX=0003 */
int	_dpmi_get_segment_base_address(cpuctx_t *scp, int is_32, int _selector, ULONG *_addr);			/* DPMI 0.9 AX=0006 */
int	_dpmi_set_segment_base_address(cpuctx_t *scp, int is_32, int _selector, ULONG _address);			/* DPMI 0.9 AX=0007 */
ULONG _dpmi_get_segment_limit(cpuctx_t *scp, int is_32, int _selector);						/* LSL instruction  */
int	_dpmi_set_segment_limit(cpuctx_t *scp, int is_32, int _selector, ULONG _limit);				/* DPMI 0.9 AX=0008 */
int	_dpmi_get_descriptor_access_rights(cpuctx_t *scp, int is_32, int _selector);					/* LAR instruction  */
int	_dpmi_set_descriptor_access_rights(cpuctx_t *scp, int is_32, int _selector, int _rights);			/* DPMI 0.9 AX=0009 */
int	_dpmi_create_alias_descriptor(cpuctx_t *scp, int is_32, int _selector);						/* DPMI 0.9 AX=000a */
int	_dpmi_get_descriptor(cpuctx_t *scp, int is_32, int _selector, void *_buffer);					/* DPMI 0.9 AX=000b */
int	_dpmi_set_descriptor(cpuctx_t *scp, int is_32, int _selector, void *_buffer);					/* DPMI 0.9 AX=000c */
int	_dpmi_allocate_specific_ldt_descriptor(cpuctx_t *scp, int is_32, int _selector);					/* DPMI 0.9 AX=000d */

int	_dpmi_get_multiple_descriptors(cpuctx_t *scp, int is_32, int _count, void *_buffer);				/* DPMI 1.0 AX=000e */
int	_dpmi_set_multiple_descriptors(cpuctx_t *scp, int is_32, int _count, void *_buffer);				/* DPMI 1.0 AX=000f */

int	_dpmi_allocate_dos_memory(cpuctx_t *scp, int is_32, int _paragraphs, int *_ret_selector_or_max);			/* DPMI 0.9 AX=0100 */
int	_dpmi_free_dos_memory(cpuctx_t *scp, int is_32, int _selector);							/* DPMI 0.9 AX=0101 */
int	_dpmi_resize_dos_memory(cpuctx_t *scp, int is_32, int _selector, int _newpara, int *_ret_max);			/* DPMI 0.9 AX=0102 */

int	_dpmi_get_real_mode_interrupt_vector(cpuctx_t *scp, int is_32, int _vector, __dpmi_raddr *_address);		/* DPMI 0.9 AX=0200 */
int	_dpmi_set_real_mode_interrupt_vector(cpuctx_t *scp, int is_32, int _vector, __dpmi_raddr *_address);		/* DPMI 0.9 AX=0201 */
int	_dpmi_get_processor_exception_handler_vector(cpuctx_t *scp, int is_32, int _vector, __dpmi_paddr *_address);	/* DPMI 0.9 AX=0202 */
int	_dpmi_set_processor_exception_handler_vector(cpuctx_t *scp, int is_32, int _vector, __dpmi_paddr *_address);	/* DPMI 0.9 AX=0203 */
int	_dpmi_get_protected_mode_interrupt_vector(cpuctx_t *scp, int is_32, int _vector, __dpmi_paddr *_address);	/* DPMI 0.9 AX=0204 */
int	_dpmi_set_protected_mode_interrupt_vector(cpuctx_t *scp, int is_32, int _vector, __dpmi_paddr *_address);	/* DPMI 0.9 AX=0205 */

int	_dpmi_get_extended_exception_handler_vector_pm(cpuctx_t *scp, int is_32, int _vector, __dpmi_paddr *_address);	/* DPMI 1.0 AX=0210 */
int	_dpmi_get_extended_exception_handler_vector_rm(cpuctx_t *scp, int is_32, int _vector, __dpmi_paddr *_address);	/* DPMI 1.0 AX=0211 */
int	_dpmi_set_extended_exception_handler_vector_pm(cpuctx_t *scp, int is_32, int _vector, __dpmi_paddr *_address);	/* DPMI 1.0 AX=0212 */
int	_dpmi_set_extended_exception_handler_vector_rm(cpuctx_t *scp, int is_32, int _vector, __dpmi_paddr *_address);	/* DPMI 1.0 AX=0213 */

int	_dpmi_simulate_real_mode_interrupt(cpuctx_t *scp, int is_32, int _vector, __dpmi_regs *__regs);			/* DPMI 0.9 AX=0300 */
int	_dpmi_int(cpuctx_t *scp, int is_32, int _vector, __dpmi_regs *__regs); /* like above, but sets ss sp fl */	/* DPMI 0.9 AX=0300 */

int	_dpmi_simulate_real_mode_procedure_retf(cpuctx_t *scp, int is_32, __dpmi_regs *__regs);				/* DPMI 0.9 AX=0301 */
int	_dpmi_simulate_real_mode_procedure_retf_stack(cpuctx_t *scp, int is_32, __dpmi_regs *__regs, int stack_words_to_copy, const void *stack_data); /* DPMI 0.9 AX=0301 */
int	_dpmi_simulate_real_mode_procedure_iret(cpuctx_t *scp, int is_32, __dpmi_regs *__regs);				/* DPMI 0.9 AX=0302 */
int	_dpmi_allocate_real_mode_callback(cpuctx_t *scp, int is_32, void (*_handler)(void), __dpmi_regs *__regs, __dpmi_raddr *_ret); /* DPMI 0.9 AX=0303 */
int	_dpmi_free_real_mode_callback(cpuctx_t *scp, int is_32, __dpmi_raddr *_addr);					/* DPMI 0.9 AX=0304 */
int	_dpmi_get_state_save_restore_addr(cpuctx_t *scp, int is_32, __dpmi_raddr *_rm, __dpmi_paddr *_pm);		/* DPMI 0.9 AX=0305 */
int	_dpmi_get_raw_mode_switch_addr(cpuctx_t *scp, int is_32, __dpmi_raddr *_rm, __dpmi_paddr *_pm);			/* DPMI 0.9 AX=0306 */

int	_dpmi_get_version(cpuctx_t *scp, int is_32, __dpmi_version_ret *_ret);						/* DPMI 0.9 AX=0400 */

int	_dpmi_get_capabilities(cpuctx_t *scp, int is_32, int *_flags, char *vendor_info);				/* DPMI 1.0 AX=0401 */

int	_dpmi_get_free_memory_information(cpuctx_t *scp, int is_32, __dpmi_free_mem_info *_info);			/* DPMI 0.9 AX=0500 */
int	_dpmi_allocate_memory(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info);						/* DPMI 0.9 AX=0501 */
int	_dpmi_free_memory(cpuctx_t *scp, int is_32, ULONG _handle);						/* DPMI 0.9 AX=0502 */
int	_dpmi_resize_memory(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info);						/* DPMI 0.9 AX=0503 */

int	_dpmi_allocate_linear_memory(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info, int _commit);			/* DPMI 1.0 AX=0504 */
int	_dpmi_resize_linear_memory(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info, int _commit);			/* DPMI 1.0 AX=0505 */
int	_dpmi_get_page_attributes(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info, short *_buffer);			/* DPMI 1.0 AX=0506 */
int	_dpmi_set_page_attributes(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info, short *_buffer);			/* DPMI 1.0 AX=0507 */
int	_dpmi_map_device_in_memory_block(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info, ULONG _physaddr);	/* DPMI 1.0 AX=0508 */
int	_dpmi_map_conventional_memory_in_memory_block(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info, ULONG _physaddr); /* DPMI 1.0 AX=0509 */
int	_dpmi_get_memory_block_size_and_base(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info);				/* DPMI 1.0 AX=050a */
int	_dpmi_get_memory_information(cpuctx_t *scp, int is_32, __dpmi_memory_info *_buffer);				/* DPMI 1.0 AX=050b */

int	_dpmi_lock_linear_region(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info);					/* DPMI 0.9 AX=0600 */
int	_dpmi_unlock_linear_region(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info);					/* DPMI 0.9 AX=0601 */
int	_dpmi_mark_real_mode_region_as_pageable(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info);			/* DPMI 0.9 AX=0602 */
int	_dpmi_relock_real_mode_region(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info);					/* DPMI 0.9 AX=0603 */
int	_dpmi_get_page_size(cpuctx_t *scp, int is_32, ULONG *_size);						/* DPMI 0.9 AX=0604 */

int	_dpmi_mark_page_as_demand_paging_candidate(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info);			/* DPMI 0.9 AX=0702 */
int	_dpmi_discard_page_contents(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info);					/* DPMI 0.9 AX=0703 */

int	_dpmi_physical_address_mapping(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info);					/* DPMI 0.9 AX=0800 */
int	_dpmi_free_physical_address_mapping(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info);				/* DPMI 0.9 AX=0801 */

/* These next four functions return the old state */
int	_dpmi_get_and_disable_virtual_interrupt_state(cpuctx_t *scp, int is_32);					/* DPMI 0.9 AX=0900 */
int	_dpmi_get_and_enable_virtual_interrupt_state(cpuctx_t *scp, int is_32);					/* DPMI 0.9 AX=0901 */
int	_dpmi_get_and_set_virtual_interrupt_state(cpuctx_t *scp, int is_32, int _old_state);				/* DPMI 0.9 AH=09   */
int	_dpmi_get_virtual_interrupt_state(cpuctx_t *scp, int is_32);						/* DPMI 0.9 AX=0902 */

int	_dpmi_get_vendor_specific_api_entry_point(cpuctx_t *scp, int is_32, char *_id, __dpmi_paddr *_api);		/* DPMI 0.9 AX=0a00 */

int	_dpmi_set_debug_watchpoint(cpuctx_t *scp, int is_32, __dpmi_meminfo *_info, int _type);				/* DPMI 0.9 AX=0b00 */
int	_dpmi_clear_debug_watchpoint(cpuctx_t *scp, int is_32, ULONG _handle);					/* DPMI 0.9 AX=0b01 */
int	_dpmi_get_state_of_debug_watchpoint(cpuctx_t *scp, int is_32, ULONG _handle, int *_status);		/* DPMI 0.9 AX=0b02 */
int	_dpmi_reset_debug_watchpoint(cpuctx_t *scp, int is_32, ULONG _handle);					/* DPMI 0.9 AX=0b03 */

int	_dpmi_install_resident_service_provider_callback(cpuctx_t *scp, int is_32, __dpmi_callback_info *_info);		/* DPMI 1.0 AX=0c00 */
int	_dpmi_terminate_and_stay_resident(cpuctx_t *scp, int is_32, int return_code, int paragraphs_to_keep);		/* DPMI 1.0 AX=0c01 */

int	_dpmi_allocate_shared_memory(cpuctx_t *scp, int is_32, __dpmi_shminfo *_info);					/* DPMI 1.0 AX=0d00 */
int	_dpmi_free_shared_memory(cpuctx_t *scp, int is_32, ULONG _handle);					/* DPMI 1.0 AX=0d01 */
int	_dpmi_serialize_on_shared_memory(cpuctx_t *scp, int is_32, ULONG _handle, int _flags);			/* DPMI 1.0 AX=0d02 */
int	_dpmi_free_serialization_on_shared_memory(cpuctx_t *scp, int is_32, ULONG _handle, int _flags);		/* DPMI 1.0 AX=0d03 */

int	_dpmi_get_coprocessor_status(cpuctx_t *scp, int is_32);							/* DPMI 1.0 AX=0e00 */
int	_dpmi_set_coprocessor_emulation(cpuctx_t *scp, int is_32, int _flags);						/* DPMI 1.0 AX=0e01 */

void dpmi_api_init(uint16_t selector, dosaddr_t  pool, int pool_size);

__dpmi_paddr dapi_alloc(int len);
void dapi_free(__dpmi_paddr ptr);

#endif

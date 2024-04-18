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
#include "../emudpmi.h"
#include "../djdpmi.h"
#include "../dpmi_api.h"

#define DD(r, n, a, ...) \
static inline r __##n a { \
  return _##n(dpmi_get_scp(), dpmi_is_32(), __VA_ARGS__); \
}
#define DDv(r, n) \
static inline r __##n(void) { \
  return _##n(dpmi_get_scp(), dpmi_is_32()); \
}
#define vDD(n, a, ...) \
static inline void __##n a { \
  _##n(dpmi_get_scp(), dpmi_is_32(), __VA_ARGS__); \
}
#define vDDv(n) \
static inline void __##n(void) { \
  _##n(dpmi_get_scp(), dpmi_is_32()); \
}

vDDv(dpmi_yield)

DD(int, dpmi_allocate_ldt_descriptors, (int _count), _count);
DD(int, dpmi_free_ldt_descriptor, (int _descriptor), _descriptor);
DD(int, dpmi_segment_to_descriptor, (int _segment), _segment);
DDv(int, dpmi_get_selector_increment_value);
DD(int, dpmi_get_segment_base_address, (int _selector, ULONG *_addr), _selector, _addr);
DD(int, dpmi_set_segment_base_address, (int _selector, ULONG _address), _selector, _address);
DD(ULONG, dpmi_get_segment_limit, (int _selector), _selector);
DD(int, dpmi_set_segment_limit, (int _selector, ULONG _limit), _selector, _limit);
DD(int, dpmi_get_descriptor_access_rights, (int _selector), _selector);
DD(int, dpmi_set_descriptor_access_rights, (int _selector, int _rights), _selector, _rights);
DD(int, dpmi_create_alias_descriptor, (int _selector), _selector);
DD(int, dpmi_get_descriptor, (int _selector, void *_buffer), _selector, _buffer);
DD(int, dpmi_set_descriptor, (int _selector, void *_buffer), _selector, _buffer);
DD(int, dpmi_allocate_specific_ldt_descriptor, (int _selector), _selector);

DD(int, dpmi_get_multiple_descriptors, (int _count, void *_buffer), _count, _buffer);				/* DPMI 1.0 AX=000e */
DD(int, dpmi_set_multiple_descriptors, (int _count, void *_buffer), _count, _buffer);

DD(int, dpmi_allocate_dos_memory, (int _paragraphs, int *_ret_selector_or_max),
  _paragraphs, _ret_selector_or_max);
DD(int, dpmi_free_dos_memory, (int _selector), _selector);
DD(int, dpmi_resize_dos_memory, (int _selector, int _newpara, int *_ret_max),
  _selector, _newpara, _ret_max);

DD(int, dpmi_get_real_mode_interrupt_vector, (int _vector, __dpmi_raddr *_address),
  _vector, _address);
DD(int, dpmi_set_real_mode_interrupt_vector, (int _vector, __dpmi_raddr *_address),
  _vector, _address);
DD(int, dpmi_get_processor_exception_handler_vector, (int _vector, __dpmi_paddr *_address),
  _vector, _address);
DD(int, dpmi_set_processor_exception_handler_vector, (int _vector, __dpmi_paddr *_address),
  _vector, _address);
DD(int, dpmi_get_protected_mode_interrupt_vector, (int _vector, __dpmi_paddr *_address),
  _vector, _address);
DD(int, dpmi_set_protected_mode_interrupt_vector, (int _vector, __dpmi_paddr *_address),
  _vector, _address);

DD(int, dpmi_get_extended_exception_handler_vector_pm, (int _vector, __dpmi_paddr *_address),
  _vector, _address);
DD(int, dpmi_get_extended_exception_handler_vector_rm, (int _vector, __dpmi_paddr *_address),
  _vector, _address);
DD(int, dpmi_set_extended_exception_handler_vector_pm, (int _vector, __dpmi_paddr *_address),
  _vector, _address);
DD(int, dpmi_set_extended_exception_handler_vector_rm, (int _vector, __dpmi_paddr *_address),
  _vector, _address);

DD(int, dpmi_simulate_real_mode_interrupt, (int _vector, __dpmi_regs *__regs),
  _vector, __regs);
DD(int, dpmi_int, (int _vector, __dpmi_regs *__regs),
  _vector, __regs);

DD(int, dpmi_simulate_real_mode_procedure_retf, (__dpmi_regs *__regs), __regs);
DD(int, dpmi_simulate_real_mode_procedure_retf_stack, (__dpmi_regs *__regs, int stack_words_to_copy, const void *stack_data),
  __regs, stack_words_to_copy, stack_data);
DD(int, dpmi_simulate_real_mode_procedure_iret, (__dpmi_regs *__regs), __regs);
DD(int, dpmi_allocate_real_mode_callback, (void (*_handler)(void), __dpmi_regs *__regs, __dpmi_raddr *_ret),
  _handler, __regs, _ret);
DD(int, dpmi_free_real_mode_callback, (__dpmi_raddr *_addr), _addr);
DD(int, dpmi_get_state_save_restore_addr, (__dpmi_raddr *_rm, __dpmi_paddr *_pm), _rm, _pm);
DD(int, dpmi_get_raw_mode_switch_addr, (__dpmi_raddr *_rm, __dpmi_paddr *_pm), _rm, _pm);

DD(int, dpmi_get_version, (__dpmi_version_ret *_ret), _ret);

DD(int, dpmi_get_capabilities, (int *_flags, char *vendor_info),
  _flags, vendor_info);

DD(int, dpmi_get_free_memory_information, (__dpmi_free_mem_info *_info), _info);
DD(int, dpmi_allocate_memory, (__dpmi_meminfo *_info), _info);
DD(int, dpmi_free_memory, (ULONG _handle), _handle);
DD(int, dpmi_resize_memory, (__dpmi_meminfo *_info), _info);

DD(int, dpmi_allocate_linear_memory, (__dpmi_meminfo *_info, int _commit),
  _info, _commit);
DD(int, dpmi_resize_linear_memory, (__dpmi_meminfo *_info, int _commit),
  _info, _commit);
DD(int, dpmi_get_page_attributes, (__dpmi_meminfo *_info, short *_buffer),
  _info, _buffer);
DD(int, dpmi_set_page_attributes, (__dpmi_meminfo *_info, short *_buffer),
  _info, _buffer);
DD(int, dpmi_map_device_in_memory_block, (__dpmi_meminfo *_info, ULONG _physaddr),
  _info, _physaddr);
DD(int, dpmi_map_conventional_memory_in_memory_block, (__dpmi_meminfo *_info, ULONG _physaddr),
  _info, _physaddr);
DD(int, dpmi_get_memory_block_size_and_base, (__dpmi_meminfo *_info), _info);
DD(int, dpmi_get_memory_information, (__dpmi_memory_info *_buffer), _buffer);

DD(int, dpmi_lock_linear_region, (__dpmi_meminfo *_info), _info);
DD(int, dpmi_unlock_linear_region, (__dpmi_meminfo *_info), _info);
DD(int, dpmi_mark_real_mode_region_as_pageable, (__dpmi_meminfo *_info), _info);
DD(int, dpmi_relock_real_mode_region, (__dpmi_meminfo *_info), _info);
DD(int, dpmi_get_page_size, (ULONG *_size), _size);

DD(int, dpmi_mark_page_as_demand_paging_candidate, (__dpmi_meminfo *_info), _info);
DD(int, dpmi_discard_page_contents, (__dpmi_meminfo *_info), _info);

DD(int, dpmi_physical_address_mapping, (__dpmi_meminfo *_info), _info);
DD(int, dpmi_free_physical_address_mapping, (__dpmi_meminfo *_info), _info);

DDv(int, dpmi_get_and_disable_virtual_interrupt_state);
DDv(int, dpmi_get_and_enable_virtual_interrupt_state);
DD(int, dpmi_get_and_set_virtual_interrupt_state, (int _old_state), _old_state);
DDv(int, dpmi_get_virtual_interrupt_state);

DD(int, dpmi_get_vendor_specific_api_entry_point, (char *_id, __dpmi_paddr *_api),
  _id, _api);

DD(int, dpmi_set_debug_watchpoint, (__dpmi_meminfo *_info, int _type), _info, _type);
DD(int, dpmi_clear_debug_watchpoint, (ULONG _handle), _handle);
DD(int, dpmi_get_state_of_debug_watchpoint, (ULONG _handle, int *_status),
  _handle, _status);
DD(int, dpmi_reset_debug_watchpoint, (ULONG _handle), _handle);

DD(int, dpmi_install_resident_service_provider_callback, (__dpmi_callback_info *_info),
  _info);
DD(int, dpmi_terminate_and_stay_resident, (int return_code, int paragraphs_to_keep),
  return_code, paragraphs_to_keep);

DD(int, dpmi_allocate_shared_memory, (__dpmi_shminfo *_info), _info);
DD(int, dpmi_free_shared_memory, (LONG _handle), _handle);
DD(int, dpmi_serialize_on_shared_memory, (ULONG _handle, int _flags),
  _handle, _flags);
DD(int, dpmi_free_serialization_on_shared_memory, (ULONG _handle, int _flags),
  _handle, _flags);

DDv(int, dpmi_get_coprocessor_status);
DD(int, dpmi_set_coprocessor_emulation, (int _flags), _flags);

/* ripped from djgpp's dpmi.h */

#include <stdint.h>
#define LONG int32_t
#define ULONG uint32_t

typedef struct {
  unsigned short offset16;
  unsigned short segment;
} __dpmi_raddr;

typedef struct {
  ULONG  offset32;
  unsigned short selector;
} __dpmi_paddr;

typedef struct {
  ULONG handle;			/* 0, 2 */
  ULONG size; 	/* or count */	/* 4, 6 */
  ULONG address;		/* 8, 10 */
} __dpmi_meminfo;

typedef union {
  struct {
    ULONG edi;
    ULONG esi;
    ULONG ebp;
    ULONG res;
    ULONG ebx;
    ULONG edx;
    ULONG ecx;
    ULONG eax;
  } d;
  struct {
    unsigned short di, di_hi;
    unsigned short si, si_hi;
    unsigned short bp, bp_hi;
    unsigned short res, res_hi;
    unsigned short bx, bx_hi;
    unsigned short dx, dx_hi;
    unsigned short cx, cx_hi;
    unsigned short ax, ax_hi;
    unsigned short flags;
    unsigned short es;
    unsigned short ds;
    unsigned short fs;
    unsigned short gs;
    unsigned short ip;
    unsigned short cs;
    unsigned short sp;
    unsigned short ss;
  } x;
  struct {
    unsigned char edi[4];
    unsigned char esi[4];
    unsigned char ebp[4];
    unsigned char res[4];
    unsigned char bl, bh, ebx_b2, ebx_b3;
    unsigned char dl, dh, edx_b2, edx_b3;
    unsigned char cl, ch, ecx_b2, ecx_b3;
    unsigned char al, ah, eax_b2, eax_b3;
  } h;
} __attribute__((packed)) __dpmi_regs;

typedef struct {
  unsigned char  major;
  unsigned char  minor;
  unsigned short flags;
  unsigned char  cpu;
  unsigned char  master_pic;
  unsigned char  slave_pic;
} __dpmi_version_ret;

typedef struct {
  ULONG largest_available_free_block_in_bytes;
  ULONG maximum_unlocked_page_allocation_in_pages;
  ULONG maximum_locked_page_allocation_in_pages;
  ULONG linear_address_space_size_in_pages;
  ULONG total_number_of_unlocked_pages;
  ULONG total_number_of_free_pages;
  ULONG total_number_of_physical_pages;
  ULONG free_linear_address_space_in_pages;
  ULONG size_of_paging_file_partition_in_pages;
  ULONG reserved[3];
} __dpmi_free_mem_info;

typedef struct {
  ULONG total_allocated_bytes_of_physical_memory_host;
  ULONG total_allocated_bytes_of_virtual_memory_host;
  ULONG total_available_bytes_of_virtual_memory_host;
  ULONG total_allocated_bytes_of_virtual_memory_vcpu;
  ULONG total_available_bytes_of_virtual_memory_vcpu;
  ULONG total_allocated_bytes_of_virtual_memory_client;
  ULONG total_available_bytes_of_virtual_memory_client;
  ULONG total_locked_bytes_of_memory_client;
  ULONG max_locked_bytes_of_memory_client;
  ULONG highest_linear_address_available_to_client;
  ULONG size_in_bytes_of_largest_free_memory_block;
  ULONG size_of_minimum_allocation_unit_in_bytes;
  ULONG size_of_allocation_alignment_unit_in_bytes;
  ULONG reserved[19];
} __dpmi_memory_info;

typedef struct {
  ULONG data16[2];
  ULONG code16[2];
  unsigned short ip;
  unsigned short reserved;
  ULONG data32[2];
  ULONG code32[2];
  ULONG eip;
} __dpmi_callback_info;

typedef struct {
  ULONG size_requested;
  ULONG size;
  ULONG handle;
  ULONG address;
  ULONG name_offset;
  unsigned short name_selector;
  unsigned short reserved1;
  ULONG reserved2;
} __dpmi_shminfo;

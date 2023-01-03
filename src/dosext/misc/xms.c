/* xms.c for the DOS emulator
 *       Robert Sanders, gt8134b@prism.gatech.edu
 *
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * Currently the XMS 3.0 spec is covered in this file. XMS is fairly simple
 * as it only deals with allocating extended memory and then moving it
 * around in specific calls. This spec also includes the allocation of UMB's,
 * so they are also included as part of this file. The amount of xms memory
 * returned to DOS programs via the XMS requests, or int15 fnc88 is set in
 * "/etc/dosemu.conf" via the XMS parameter.
 *
 * /REMARK
 * DANG_END_MODULE
 *
 * NOTE: I keep the BYTE size of EMB's in a field called "size" in the EMB
 *    structure.  Most XMS calls specify/expect size in KILOBYTES.
 */


#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "emu.h"
#include "memory.h"
#include "xms.h"
#include "hlt.h"
#include "int.h"
#include "hma.h"
#include "emm.h"
#include "mapping.h"
#include "dos2linux.h"
#include "utilities.h"
#include "cpu-emu.h"
#include "smalloc.h"
#include "pgalloc.h"

#undef  DEBUG_XMS

/*
static char RCSxms[] = "$Id$";
*/

#define	 XMS_GET_VERSION		0x00
#define	 XMS_ALLOCATE_HIGH_MEMORY	0x01
#define	 XMS_FREE_HIGH_MEMORY		0x02
#define	 XMS_GLOBAL_ENABLE_A20		0x03
#define	 XMS_GLOBAL_DISABLE_A20		0x04
#define	 XMS_LOCAL_ENABLE_A20		0x05
#define	 XMS_LOCAL_DISABLE_A20		0x06
#define	 XMS_QUERY_A20			0x07
#define	 XMS_QUERY_FREE_EXTENDED_MEMORY	0x08
#define	 XMS_ALLOCATE_EXTENDED_MEMORY	0x09
#define	 XMS_FREE_EXTENDED_MEMORY	0x0a
#define	 XMS_MOVE_EXTENDED_MEMORY_BLOCK	0x0b
#define	 XMS_LOCK_EXTENDED_MEMORY_BLOCK	0x0c
#define	 XMS_UNLOCK_EXTENDED_MEMORY_BLOCK 0x0d
#define	 XMS_GET_EMB_HANDLE_INFORMATION	0x0e
#define	 XMS_RESIZE_EXTENDED_MEMORY_BLOCK 0x0f
#define	 XMS_ALLOCATE_UMB		0x10
#define	 XMS_DEALLOCATE_UMB		0x11
#define	 XMS_REALLOCATE_UMB		0x12

#define	HIGH_MEMORY_IN_USE		0x92
#define	HIGH_MEMORY_NOT_ALLOCATED	0x93
#define XMS_OUT_OF_SPACE		0xa0
#define XMS_INVALID_HANDLE		0xa2

static int a20_local, a20_global, freeHMA;	/* is HMA free? */

struct __attribute__ ((__packed__)) EMM {
   unsigned int Length;
   unsigned short SourceHandle;
   unsigned int SourceOffset;
   unsigned short DestHandle;
   unsigned int DestOffset;
};

struct pava {
  unsigned pa;
  dosaddr_t va;
};

struct Handle {
  unsigned short int num;
  void *addr;
  unsigned int size;
  int lockcount;
  struct pava dst;
};

static struct Handle handles[NUM_HANDLES];
static int handle_count;
static int intdrv;
static int ext_hooked_hma;
static int totalBytes;
static void *pgapool;

void show_emm(struct EMM);
static unsigned char xms_query_freemem(int), xms_allocate_EMB(int), xms_free_EMB(void),
 xms_move_EMB(void), xms_lock_EMB(int), xms_EMB_info(int), xms_realloc_EMB(int);

static int FindFreeHandle(int);
static void xx_printf(int prio, const char *fmt, ...) FORMAT(printf, 2, 3);

/* beginning of quote from Mach */

/* technically, UMBS needs to be UMB_SIZE/16 since some fool could allocate
   a million 16 byte entries.  In reality, there's usually about 128K of
   UMB's tops and people tend to allocate it in large chunks.  UMBS is
   currently 512 (as opposed to UMB_SIZE/16 or 16K entries) which is
   probably still insanely high.  Ah linear table searching... -- scottb */
#define UMBS 16
#define UMB_NULL -1

static smpool umbs[UMBS];
static int umbs_used;

#define x_Stub(arg1, s, a...)   x_printf("XMS: " s, ##a)
#define Debug0(args)		x_Stub args
#define Debug1(args)		x_Stub args
#define Debug2(args)		x_Stub args
/* #define dbg_fd stderr */

static void
umb_setup(int umb_ems)
{
  dosaddr_t addr_start;
  uint32_t size;

  memcheck_addtype('U', "Upper Memory Block (UMB, XMS 3.0)");

  addr_start = 0x00000;     /* start address */
  while ((size = memcheck_findhole(&addr_start, 1024, 0x100000)) != 0) {
    if (!umb_ems && emm_is_pframe_addr(addr_start, &size)) {
      addr_start += 16*1024;
      continue;
    }
    Debug0((dbg_fd, "findhole - from 0x%5.5X, %dKb\n", addr_start, size/1024));
    memcheck_map_reserve('U', addr_start, size);
#if 0
    if (addr_start == 0xa0000 && config.umb_a0 == 2) {
      // FreeDOS UMB bug, reserve 1 para
      const int rsv = 16;
      addr_start += rsv;
      size -= rsv;
    }
#endif
    assert(umbs_used < UMBS);
    sminit(&umbs[umbs_used], MEM_BASE32(addr_start), size);
    smregister_error_notifier(&umbs[umbs_used], xx_printf);
    umbs_used++;
    Debug0((dbg_fd, "umb_setup: addr %x size 0x%04x\n",
	      addr_start, size));
  }
#if 0
  /* need to memset UMBs as FreeDOS marks them as used */
  /* in fact smalloc does memset(), so this no longer needed */
  /* bug is fixed in FDPP to not require this */
  for (i = 0; i < umbs_used; i++)
    memset(smget_base_addr(&umbs[i]), 0, umbs[i].size);
#endif
}

static int
umb_find(int segbase)
{
  int i;
  dosaddr_t addr = SEGOFF2LINEAR(segbase, 0);

  for (i = 0; i < umbs_used; i++) {
    dosaddr_t base = DOSADDR_REL(smget_base_addr(&umbs[i]));
    if (addr >= base && addr < base + umbs[i].size)
      return (i);
  }
  return (UMB_NULL);
}

static unsigned int
umb_allocate(int size)
{
  int i;

  for (i = 0; i < umbs_used; i++) {
    if (smget_largest_free_area(&umbs[i]) >= size) {
      void *addr = smalloc(&umbs[i], size);
      assert(addr);
      return DOSADDR_REL(addr);
    }
  }
  return 0;
}

static void umb_free_all(void)
{
  int i;

  for (i = 0; i < umbs_used; i++) {
    e_invalidate_full(DOSADDR_REL(smget_base_addr(&umbs[i])), umbs[i].size);
    smfree_all(&umbs[i]);
  }
  umbs_used = 0;
}

static int umb_free(int segbase)
{
  int umb = umb_find(segbase);

  if (umb != UMB_NULL)
    return smfree(&umbs[umb], SEG2UNIX(segbase));
  return -1;
}

static int
umb_query(void)
{
  int i;
  int largest = 0;

  for (i = 0; i < umbs_used; i++) {
    size_t l = smget_largest_free_area(&umbs[i]);
    if (l > largest)
      largest = l;
  }
  Debug0((dbg_fd, "umb_query: largest UMB was %d(%#x) bytes\n",
	largest, largest));
  return (largest);
}

/* end of stuff from Mach */

static void *xms_alloc(unsigned size)
{
  void *ptr = alloc_mapping(MAPPING_OTHER, PAGE_ALIGN(size));
  if (ptr == MAP_FAILED)
    return 0;
  return ptr;
}

static void *xms_realloc(void *optr, unsigned osize, unsigned size)
{
  void *ptr = realloc_mapping(MAPPING_OTHER, optr, PAGE_ALIGN(osize),
	PAGE_ALIGN(size));
  if (ptr == MAP_FAILED)
    return 0;
  return ptr;
}

static void xms_free(void *addr, unsigned osize)
{
  free_mapping(MAPPING_OTHER, addr, PAGE_ALIGN(osize));
}

static struct pava map_EMB(void *addr, unsigned size, unsigned handle)
{
  int page;
  struct pava ret = {};

  page = pgaalloc(pgapool, PAGE_ALIGN(size) >> PAGE_SHIFT, handle);
  if (page < 0) {
    error("error allocating %i bytes for xms\n", size);
    return ret;
  }
  ret.va = alias_mapping_high(MAPPING_EXTMEM, PAGE_ALIGN(size),
		 PROT_READ | PROT_WRITE, addr);
  if (ret.va == (dosaddr_t)-1) {
    error("failure to map xms\n");
    leavedos(2);
  }
  ret.pa = xms_base + (page << PAGE_SHIFT);
  register_hardware_ram_virtual('x', ret.pa, size, addr, ret.va);
  return ret;
}

static void unmap_EMB(struct pava base, unsigned size)
{
  int err = unregister_hardware_ram_virtual(base.pa);
  if (err)
    error("error unregistering hwram at %#x\n", base.pa);
  unalias_mapping_high(MAPPING_OTHER, base.va, PAGE_ALIGN(size));
  pgafree(pgapool, (base.pa - xms_base) >> PAGE_SHIFT);
}

#if 0
/* unused */
void *xms_resolve_physaddr(unsigned addr)
{
  struct pgrm m;
  assert(addr >= xms_base && addr < xms_base + config.xms_map_size);
  m = pgarmap(pgapool, (addr - xms_base) >> PAGE_SHIFT);
  if (m.pgoff == -1)
    return MAP_FAILED;
  assert(m.id >= 0 && m.id < NUM_HANDLES && handles[m.id].addr);
  return (handles[m.id].addr + (m.pgoff << PAGE_SHIFT) +
      (addr & (PAGE_SIZE - 1)));
}
#endif

static void do_free_EMB(int h)
{
    if (handles[h].dst.pa)
      unmap_EMB(handles[h].dst, handles[h].size);
    handles[h].dst.pa = 0;
    if (handles[h].addr)
      xms_free(handles[h].addr, handles[h].size);
    handles[h].addr = NULL;
}

void xms_reset(void)
{
  int i;

  if (umbs_used) {
    umb_free_all();
    memcheck_map_free('U');
  }

  for (i = 0; i < NUM_HANDLES; i++)
    do_free_EMB(i);
  handle_count = 0;
  totalBytes = 0;
  intdrv = 0;
  freeHMA = 0;
  ext_hooked_hma = 0;
  pgareset(pgapool);
  if (config.ext_mem) {
    /* Register mem for himem.sys. Unregister later if internal driver loaded. */
    register_hardware_ram_virtual('m', LOWMEM_SIZE + HMASIZE, EXTMEM_SIZE,
	extmem_base, extmem_vbase);
  }
}

static void xms_local_reset(void)
{
  intdrv = 0;
}

static void xx_printf(int prio, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vlog_printf(0, fmt, args);
  va_end(args);
}

int xms_intdrv(void)
{
  return intdrv;
}

static int xms_helper_init(void)
{
  if (intdrv)
    return 1;

  x_printf("XMS: initializing XMS... %d handles\n", NUM_HANDLES);

  freeHMA = (config.hma && !ext_hooked_hma);
  ext_hooked_hma = 0;
  a20_global = a20_local = 0;

  if (!config.xms_size)
    return 0;
  if (config.ext_mem) {
    /* remove the mapping for external himem.sys */
    int err = unregister_hardware_ram_virtual(LOWMEM_SIZE + HMASIZE);
    if (err)
      error("error unregistering ext_mem\n");
  }
  intdrv = 1;
  return 1;
}

int xms_helper_init_ext(void)
{
  assert(!intdrv);
  ext_hooked_hma = (xms_helper_init() && freeHMA);
  return ext_hooked_hma;
}

void xms_helper(void)
{
  NOCARRY;

  switch (HI(ax)) {

  case XMS_HELPER_XMS_INIT:
    if (!xms_helper_init())
      CARRY;
    break;

  case XMS_HELPER_GET_ENTRY_POINT:
    WRITE_SEG_REG(es, XMSControl_SEG);
    LWORD(ebx) = XMSControl_OFF;
    LWORD(eax) = 0;	/* report success */
    break;

  case XMS_HELPER_UMB_INIT: {
    char *cmdl, *p, *p1;
    int umb_ems = 0;
    int unk_opt = 0;

    if (LO(bx) < UMB_DRIVER_VERSION) {
      error("UMB driver version mismatch: got %i, expected %i, disabling.\n"
            "Please update your ems.sys from the latest dosemu package.\n",
        HI(ax), UMB_DRIVER_VERSION);
      com_printf("\nUMB driver version mismatch, disabling.\n"
            "Please update your ems.sys from the latest dosemu package.\n"
            "\nPress any key!\n");
      set_IF();
      com_biosgetch();
      clear_IF();
      CARRY;
      LWORD(ebx) = UMB_ERROR_VERSION_MISMATCH;
      break;
    }

    if (umbs_used) {
      CARRY;
      LWORD(ebx) = UMB_ERROR_ALREADY_INITIALIZED;
      break;
    }

    /* parse command line only for umb.sys, not for ems.sys */
    if (HI(bx) == UMB_DRIVER_UMB_SYS) {
      /* ED:DI points to device request header */
      p = FAR2PTR(READ_DWORD(SEGOFF2LINEAR(_ES, _DI) + 18));
      p1 = strpbrk(p, "\r\n");
      if (p1)
        cmdl = strndup(p, p1 - p);	// who knows if DOS puts \0 at end
      else
        cmdl = strdup(p);
      p = cmdl + strlen(cmdl) - 1;
      while (*p == ' ') {
        *p = 0;
        p--;
      }
      p = strrchr(cmdl, ' ');
      if (p) {
        p++;
        if (strcasecmp(p, "/FULL") == 0)
          umb_ems = 1;
        else
          unk_opt = 1;
      }
      free(cmdl);
      if (unk_opt) {
        CARRY;
        LWORD(ebx) = UMB_ERROR_UNKNOWN_OPTION;
        return;
      }
    }

    umb_setup(umb_ems);
    LWORD(eax) = umbs_used;
    if (!umbs_used) {
      CARRY;
      LWORD(ebx) = UMB_ERROR_UMBS_UNAVAIL;
      return;
    }
    break;
  }
  }
}

void xms_init(void)
{
  pgapool = pgainit(xms_map_size >> PAGE_SHIFT);
}

void xms_done(void)
{
  pgadone(pgapool);
}

static void XMS_RET(int err)
{
  /* careful here: BL should NOT be set to 0 if there is no error,
     in particular for "get handle information" that is wrong,
     but also for locking a handle.
     An exception is xms_query_freemem, which sets BL to zero
     itself, and does not use AX to report an error */
  LWORD(eax) = err ? 0 : 1;
  if (err)
    LO(bx) = err;
}

void xms_control(void)
{
  int is_umb_fn = 0;

 clear_ZF();
 /* First do the UMB functions */
 switch (HI(ax)) {
  case XMS_ALLOCATE_UMB:
    {
      int size = LWORD(edx) << 4;
      unsigned int addr;

      if (size == 0) {
	int avail = umb_query();
	Debug0((dbg_fd, "Allocate UMB with 0 size\n"));
	XMS_RET(0xa7);
	LWORD(edx) = avail >> 4;
	break;
      }
      addr = umb_allocate(size);
      is_umb_fn = 1;
      Debug0((dbg_fd, "Allocate UMB memory: %#x\n", size));
      if (addr == 0) {
        int avail=umb_query();

	Debug0((dbg_fd, "Allocate UMB Failure\n"));
	XMS_RET(avail ? 0xb0 : 0xb1);
	LWORD(edx) = avail >> 4;
      }
      else {
	Debug0((dbg_fd, "Allocate UMB Success\n"));
	XMS_RET(0);
	LWORD(ebx) = addr >> 4;
	if (addr > 0xfffff)
	  CARRY;
	LWORD(edx) = size >> 4;
	Debug0((dbg_fd, "umb_allocated: %#x:%#x\n", addr, size));
      }
      /* retval = UNCHANGED; */
      break;
    }

  case XMS_DEALLOCATE_UMB:
    {
      int err;
      is_umb_fn = 1;
      Debug0((dbg_fd, "umb_free: 0x%04x\n", (unsigned) LWORD(edx)));
      err = umb_free(LWORD(edx));
      XMS_RET(err ? 0xb2 : 0);
      /* retval = UNCHANGED; */
      break;
    }

  case XMS_REALLOCATE_UMB:
    is_umb_fn = 1;
    x_printf("XMS realloc UMB segment 0x%04x size 0x%04x\n",
	     LWORD(edx), LWORD(ebx));
    XMS_RET(0x80);		        /* function unimplemented */
    break;
 }

 if (intdrv && !is_umb_fn) {
  switch (HI(ax)) {
  case XMS_GET_VERSION:			/* Get XMS Version Number */
    LWORD(eax) = XMS_VERSION;
    LWORD(ebx) = XMS_DRIVER_VERSION;
    LWORD(edx) = 1;		/* HMA exists */
    x_printf("XMS driver version. XMS Spec: %04x dosemu-XMS: %04x\n",
	     LWORD(eax), LWORD(ebx));
    break;

  case XMS_ALLOCATE_HIGH_MEMORY: /* Request High Memory Area */
    x_printf("XMS request HMA size 0x%04x\n", LWORD(edx));
    if (freeHMA) {
      x_printf("XMS: allocating HMA size 0x%04x\n", LWORD(edx));
      freeHMA = 0;
      XMS_RET(0);			/* no error */
      if (ext_hooked_hma)  // drop external hma hook
        xms_local_reset();
    }
    else {
      x_printf("XMS: HMA already allocated\n");
      XMS_RET(0x91);
    }
    break;

  case XMS_FREE_HIGH_MEMORY:	/* Release High Memory Area */
    x_printf("XMS release HMA\n");
    if (freeHMA) {
      x_printf("XMS: HMA already free\n");
      XMS_RET(0x93);
    }
    else {
      x_printf("XMS: freeing HMA\n");
      freeHMA = config.hma;
      XMS_RET(0);			/* no error */
    }
    break;

  case XMS_GLOBAL_ENABLE_A20:			/* Global Enable A20 */
    x_printf("XMS global enable A20\n");
    if (!a20_global)
      set_a20(1);		/* map in HMA */
    a20_global = 1;
    XMS_RET(0);			/* no error */
    break;

  case XMS_GLOBAL_DISABLE_A20:			/* Global Disable A20 */
    x_printf("XMS global disable A20\n");
    if (a20_global)
      set_a20(0);
    a20_global = 0;
    XMS_RET(0);			/* no error */
    break;

  case XMS_LOCAL_ENABLE_A20:			/* Local Enable A20 */
    x_printf("XMS LOCAL enable A20\n");
    if (!a20_local)
      set_a20(1);
    a20_local++;
    XMS_RET(0);			/* no error */
    break;

  case XMS_LOCAL_DISABLE_A20:			/* Local Disable A20 */
    x_printf("XMS LOCAL disable A20\n");
    if (a20_local)
      a20_local--;
    if (!a20_local)
      set_a20(0);
    XMS_RET(0);			/* no error */
    break;

    /* DOS, if loaded as "dos=high,umb" will continually poll
	  * the A20 line.
          */
  case XMS_QUERY_A20:			/* Query A20 */
    LWORD(eax) = a20 ? 1 : 0;
    LO(bx) = 0;			/* no error */
    break;

  case XMS_QUERY_FREE_EXTENDED_MEMORY:	/* Query Free Extended Memory */
    xms_query_freemem(OLDXMS);
    break;

  case XMS_ALLOCATE_EXTENDED_MEMORY:	/* Allocate Extended Memory Block */
    XMS_RET(xms_allocate_EMB(OLDXMS));
    break;

  case XMS_FREE_EXTENDED_MEMORY:	/* Free Extended Memory Block */
    XMS_RET(xms_free_EMB());
    break;

  case XMS_MOVE_EXTENDED_MEMORY_BLOCK:	/* Move Extended Memory Block */
    XMS_RET(xms_move_EMB());
    break;

  case XMS_LOCK_EXTENDED_MEMORY_BLOCK:	/* Lock Extended Memory Block */
    XMS_RET(xms_lock_EMB(1));
    break;

  case XMS_UNLOCK_EXTENDED_MEMORY_BLOCK: /* Unlock Extended Memory Block */
    XMS_RET(xms_lock_EMB(0));
    break;

  case XMS_GET_EMB_HANDLE_INFORMATION:	/* Get EMB Handle Information */
    XMS_RET(xms_EMB_info(OLDXMS));
    break;

  case XMS_RESIZE_EXTENDED_MEMORY_BLOCK: /* Reallocate Extended Memory Block */
    XMS_RET(xms_realloc_EMB(OLDXMS));
    break;

    /* the functions below are the newer, 32-bit XMS 3.0 interface */

  case 0x80|XMS_QUERY_FREE_EXTENDED_MEMORY: /* Query Any Free Extended Memory */
    xms_query_freemem(NEWXMS);
    break;

  case 0x80|XMS_ALLOCATE_EXTENDED_MEMORY:	/* Allocate Extended Memory */
    XMS_RET(xms_allocate_EMB(NEWXMS));
    break;

  case 0x80|XMS_GET_EMB_HANDLE_INFORMATION: /* Get EMB Handle Information */
    XMS_RET(xms_EMB_info(NEWXMS));
    break;

  case 0x80|XMS_RESIZE_EXTENDED_MEMORY_BLOCK:	/* Reallocate EMB */
    XMS_RET(xms_realloc_EMB(NEWXMS));
    break;

  default:
    x_printf("XMS: Unimplemented XMS function AX=0x%04x", LWORD(eax));
    show_regs();		/* if you delete this, put the \n on the line above */
    XMS_RET(0x80);		/* function not implemented */
  }
 }
 /* If this is the UMB request which came via the external himem.sys,
  * don't pass it back. */
 if (!intdrv) {
   if (is_umb_fn)
     set_ZF();  // tell our hook to skip external himem call
   else
     x_printf("XMS: skipping external request, ax=0x%04x, dx=0x%04x\n",
	      LWORD(eax), LWORD(edx));
 }
 LWORD(eip)++;  // skip hlt in bios.s
}

static int
FindFreeHandle(int start)
{
  int i, h = 0;

  /* first free handle is 1 */
  for (i = start; (i < NUM_HANDLES) && (h == 0); i++) {
    if (!handles[i].addr) {
      x_printf("XMS: found free handle: %d\n", i);
      h = i;
      break;
    }
    else
      x_printf("XMS: unfree handle %d ", i);
  }
  return h;
}

static int
ValidHandle(unsigned short h)
{
  if ((h < NUM_HANDLES) && (handles[h].addr))
    return 1;
  else
    return 0;
}

static unsigned char
xms_query_freemem(int api)
{
  unsigned subtotal, largest;

  if (!intdrv) {
    if (api == OLDXMS) {
      LWORD(eax) = 0;
      LWORD(edx) = 0;
    } else {
      REG(eax) = 0;
      REG(edx) = 0;
      REG(ecx) = 0;
    }
    LO(bx) = 0;
    return 0;
  }

  /* the new XMS api should actually work with the function as it
   * stands...
   */
  if (api == NEWXMS)
    x_printf("XMS: new XMS API query_freemem()!\n");

  /* xms_size is page-aligned in config.c */
  subtotal = config.xms_size - (totalBytes / 1024);
  /* total free is max allowable XMS - the number of K already allocated */
  largest = pgaavail_largest(pgapool) * 4;
  largest = largest ? _min(largest, subtotal) : subtotal;

  if (api == OLDXMS) {
    /* old XMS API uses only AX, while new API uses EAX. make
       * sure the old API sees 64 MB if the count >= 64 MB (instead of seeing
       * just the low 16 bits of some number > 65535 (KBytes) which is
       * probably < 64 MB...this makes sure that old API functions at least
       * know of 64MB of the > 64MB available memory.
       */

    if (largest > 65535)
      largest = 65535;
    if (subtotal > 65535)
      subtotal = 65535;
    LWORD(eax) = largest;
    LWORD(edx) = subtotal;
    x_printf("XMS query free memory(old): %dK %dK\n", LWORD(eax),
	     LWORD(edx));
  }
  else {
    REG(eax) = largest;
    REG(edx) = subtotal;
    REG(ecx) = main_pool.size - 1;
    x_printf("XMS query free memory(new): %dK %dK\n",
	     REG(eax), REG(edx));
  }
  /* the following line is NOT superfluous!! (see above) */
  LO(bx) = 0;
  return 0;
}

static unsigned char
xms_allocate_EMB(int api)
{
  unsigned int h;
  unsigned int kbsize, size;
  void *addr;

  if (!intdrv)
    return 0xa0;

  if (api == OLDXMS)
    kbsize = LWORD(edx);
  else
    kbsize = REG(edx);
  x_printf("XMS alloc EMB(%s) size %d KB\n", (api==OLDXMS)?"old":"new",kbsize);

  if (!(h = FindFreeHandle(FIRST_HANDLE))) {
    x_printf("XMS: out of handles\n");
    return 0xa1;
  }
  size = kbsize * 1024;
  if (kbsize == 0) {
    error("XMS WARNING: allocating 0 size EMB\n");
    return 0xa0;
  } else if (totalBytes + size > config.xms_size * 1024) {
    error("XMS: OOM allocating %i bytes EMB\n", size);
    return 0xa0;
  } else {
    addr = xms_alloc(size);
    if (!addr) {
      x_printf("XMS: out of memory\n");
      return 0xa0; /* Out of memory */
    }
    totalBytes += size;
  }
  handles[h].num = h;
  handles[h].size = size;
  handles[h].addr = addr;

  x_printf("XMS: EMB size %d bytes\n", handles[h].size);

    /* I could just rely on the behavior of malloc(0) here, but
       * I'd rather not.  I'm going to interpret the XMS 3.0 spec
       * to mean that reserving a handle of size 0 gives it no address
       */

  handles[h].lockcount = 0;
  handle_count++;

  x_printf("XMS: allocated EMB %u at %p\n", h, handles[h].addr);

  if (api == OLDXMS)
    LWORD(edx) = h;		/* handle # */
  else
    REG(edx) = h;
  return 0;
}

static unsigned char
xms_free_EMB(void)
{
  unsigned short int h = LWORD(edx);

  if (!ValidHandle(h)) {
    x_printf("XMS: free EMB bad handle %d\n", h);
    return 0xa2;
  }
  else {
    totalBytes -= handles[h].size;
    do_free_EMB(h);
    handle_count--;

    x_printf("XMS: free'd EMB %d\n", h);
    return 0;
  }
}

static unsigned char
xms_move_EMB(void)
{
  unsigned int src, dest;
  void *s, *d;
  struct EMM e;

  MEMCPY_2UNIX(&e, SEGOFF2LINEAR(SREG(ds), LWORD(esi)), sizeof e);
  x_printf("XMS move extended memory block\n");
  show_emm(e);

  /* Length must be even, XMS spec says, tested on himem too. */
  if (e.Length & 1)
    return 0xa7;

  if (e.SourceHandle == 0) {
    src = SEGOFF2LINEAR(e.SourceOffset >> 16, e.SourceOffset & 0xffff);
    if (src + e.Length > LOWMEM_SIZE + HMASIZE)
      return 0xa7;              /* invalid Length */
    s = LINEAR2UNIX(src);
  }
  else {
    if (e.SourceHandle >= NUM_HANDLES || !handles[e.SourceHandle].addr) {
      x_printf("XMS: invalid source handle\n");
      return 0xa3;
    }
    if (e.SourceOffset > handles[e.SourceHandle].size)
      return 0xa4;              /* invalid source offset */
    if (e.SourceOffset + e.Length > handles[e.SourceHandle].size)
      return 0xa7;              /* invalid Length */
    s = handles[e.SourceHandle].addr + e.SourceOffset;
  }

  if (e.DestHandle == 0) {
    dest = SEGOFF2LINEAR(e.DestOffset >> 16, e.DestOffset & 0xffff);
    if (dest + e.Length > LOWMEM_SIZE + HMASIZE)
      return 0xa7;              /* invalid Length */
    d = LINEAR2UNIX(dest);
  }
  else {
    if (!handles[e.DestHandle].addr) {
      x_printf("XMS: invalid dest handle\n");
      return 0xa5;
    }
    if (e.DestOffset > handles[e.DestHandle].size)
      return 0xa6;              /* invalid dest offset */
    if (e.DestOffset + e.Length > handles[e.DestHandle].size) {
      return 0xa7;		/* invalid Length */
    }
    d = handles[e.DestHandle].addr + e.DestOffset;
  }

  x_printf("XMS: block move from %p to %p len 0x%x\n",
	   s, d, e.Length);
  memcpy(d, s, e.Length);
  x_printf("XMS: block move done\n");

  return 0;
}

static unsigned char
xms_lock_EMB(int flag)
{
  int h = LWORD(edx);
  struct pava addr;

  if (ValidHandle(h)) {
    /* flag=1 locks, flag=0 unlocks */

    if (!flag) {
      if (handles[h].lockcount)
        handles[h].lockcount--;
      else {
        x_printf("XMS: Unlock handle %d already at 0\n", h);
        return 0xaa;		/* Block is not locked */
      }
      if (!handles[h].lockcount) {
        x_printf("XMS unlock EMB %d --> %#x\n", h, handles[h].dst.pa);
        unmap_EMB(handles[h].dst, handles[h].size);
        handles[h].dst.pa = 0;
      }
      return 0;
    }

    addr = map_EMB(handles[h].addr, handles[h].size, h);
    if (addr.pa) {
      handles[h].lockcount++;
      x_printf("XMS lock EMB %d --> %#x, va=%x\n", h, addr.pa, addr.va);
      handles[h].dst = addr;
      LWORD(edx) = addr.pa >> 16;
      LWORD(ebx) = addr.pa & 0xffff;
      return 0;
    }
    x_printf("XMS lock EMB %d failed\n", h);
    return 0xad;  /* lock failed */
  }
  else {
    x_printf("XMS: invalid handle %d, can't (un)lock\n", h);
    return 0xa2;		/* invalid handle */
  }
}

static unsigned char
xms_EMB_info(int api)
{
  int h = LWORD(edx);

  if (ValidHandle(h)) {
    if (api == OLDXMS) {
      LO(bx) = NUM_HANDLES - FIRST_HANDLE - handle_count;
      LWORD(edx) = handles[h].size / 1024;
      x_printf("XMS Get EMB info(old) %d\n", h);
    }
    else {			/* api == NEWXMS */
      LWORD(ecx) = NUM_HANDLES - FIRST_HANDLE - handle_count;
      REG(edx) = handles[h].size / 1024;
      x_printf("XMS Get EMB info(new) %d\n", h);
    }
    HI(bx) = handles[h].lockcount;
    return 0;
  }
  else {
    return 0xa2;		/* invalid handle */
  }
}

/* untested! if you test it, please tell me! */
static unsigned char
xms_realloc_EMB(int api)
{
  int h;
  unsigned int newsize;
  void *newaddr;

  h = LWORD(edx);

  if (!ValidHandle(h)) {
    x_printf("XMS: invalid handle %d, cannot realloc\n", h);
    return 0xa2;
  }
  else if (handles[h].lockcount) {
    x_printf("XMS: handle %d locked (%d), cannot realloc\n",
	     h, handles[h].lockcount);
    return 0xab;
  }

  /* BX is the 16-bit new size in KBytes for the old API, but the new
   * XMS 3.0 32-bit API uses EBX
   */
  if (api == OLDXMS)
    newsize = LWORD(ebx) * 1024;
  else
    newsize = REG(ebx) * 1024;
  if (newsize == handles[h].size)
    return 0;

  x_printf("XMS realloc EMB(%s) %d to size 0x%04x\n",
	   api == OLDXMS ? "old" : "new", h, newsize);

  newaddr = xms_realloc(handles[h].addr, handles[h].size, newsize);
  if (!newaddr) {
    x_printf("XMS: out of memory on realloc\n");
    return 0xa0; /* Out of memory */
  }
  handles[h].addr = newaddr;
  handles[h].size = newsize;

  return 0;
}

void
show_emm(struct EMM e)
{
  x_printf("XMS show_emm:\n");

  x_printf("L: 0x%08x SH: 0x%04x  SO: 0x%08x DH: 0x%04x  DO: 0x%08x\n",
	   e.Length, e.SourceHandle, e.SourceOffset,
	   e.DestHandle, e.DestOffset);
}


/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* This is a C implementation of the BIOS memory setup. The int vector
   table and variables at 0040:xxxx are initialized. */

#include "emu.h"
#include "config.h"
#include "bios.h"
#include "memory.h"
#include "hlt.h"
#include "coopth.h"
#include "lowmem.h"
#include "int.h"
#include "iodev.h"
#include "emm.h"
#include "xms.h"
#include "hma.h"
#include "dpmi.h"
#include "ipx.h"
#include "serial.h"
#include "utilities.h"
#include "doshelpers.h"
#include "mhpdbg.h"
#include "plugin_config.h"

static int li_tid;
unsigned int bios_configuration;

/*
 * install_int_10_handler - install a handler for the video-interrupt (int 10)
 *                          at address INT10_SEG:INT10_OFFS. Currently
 *                          it's f800:4200.
 *                          The new handler is only installed, if the bios
 *                          handler at f800:4200 is not the appropriate on
 *                          that means, if we use not mda with X
 */
static void install_int_10_handler (void)
{
  unsigned int ptr;

  if (config.vbios_seg == 0xe000 && config.vbios_post) {
    ptr = SEGOFF2LINEAR(BIOSSEG, ((long)bios_f000_int10ptr - (long)bios_f000));
    WRITE_DWORD(ptr, 0xe0000003);
    v_printf("VID: new int10 handler at %#x\n",ptr);
  }
  else
    v_printf("VID: install_int_10_handler: do nothing\n");
}

/*
 * DANG_BEGIN_FUNCTION bios_mem_setup
 *
 * description:
 *  Set up all memory areas as would be present on a typical i86 during
 * the boot phase.
 *
 * DANG_END_FUNCTION
 */
static inline void bios_mem_setup(void)
{
  int b;

  video_mem_setup();
  serial_mem_setup();
  printer_mem_setup();

  /* show 0 serial ports and 3 parallel ports, maybe a mouse, game card and the
   * configured number of floppy disks
   */
  CONF_NFLOP(bios_configuration, config.fdisks);
  CONF_NSER(bios_configuration, min(config.num_ser, NUM_COMS));
  CONF_NLPT(bios_configuration, min(config.num_lpt, NUM_LPTS));
  if (config.mouse.intdrv)
    bios_configuration |= CONF_MOUSE;

  bios_configuration |= CONF_GAME | CONF_DMA;

  if (config.mathco)
    bios_configuration |= CONF_MATHCO;

  g_printf("CONFIG: 0x%04x    binary: ", bios_configuration);
  for (b = 15; b >= 0; b--)
    g_printf("%s%s", (bios_configuration & (1 << b)) ? "1" : "0", (b%4) ? "" : " ");
  g_printf("\n");

  WRITE_WORD(BIOS_CONFIGURATION, bios_configuration);
  WRITE_WORD(BIOS_MEMORY_SIZE, config.mem_size);	/* size of memory */
}

static int initialized;
static void bios_reset(void);
static void bios_setup(void);
static void late_init_post(int tid);

static void late_init_thr(void *arg)
{
  if (initialized)
    return;
  /* if something else is to be added here,
   * add the "late_init" member into dev_list instead */
  video_late_init();
}

static void late_init_post(int tid)
{
  bios_reset();
  if (initialized)
    return;
  initialized = 1;
}

void post_hook(void)
{
  LWORD(eip)++; // skip hlt
  bios_setup();

  /* late_init can call int10, so make it a thread */
  coopth_start(li_tid, late_init_thr, NULL);
}

static void bios_setup(void)
{
  int i;

  /* initially, no HMA */
  set_a20(0);

  /* init trapped interrupts called via jump */
  for (i = 0; i < 256; i++) {
    if (config.vga && !config.vbios_post) {
      uint16_t seg, off;
      unsigned int addr;

      seg = int_bios_area[i] >> 16;
      off = int_bios_area[i] & 0xffff;
      v_printf("int0x%x was 0x%04x:0x%04x\n", i, seg, off);
      addr = SEGOFF2LINEAR(seg, off);
      if (addr >= VBIOS_START && addr < VBIOS_START + VBIOS_SIZE) {
	g_printf("Setting int0x%x to 0x%04x:0x%04x\n", i, seg, off);
	SETIVEC(i, seg, off);
	continue;
      }
    }

    /* interrupts >= 0xc0 are scratch (BIOS stack),
       unless defined by DOSEMU */
    if ((i & 0xf8) == 0x60 || (i >= 0x78 && i < 0xc0)) { /* user interrupts */
	/* show also EMS (int0x67) as disabled */
	SETIVEC(i, 0, 0);
    } else if ((i & 0xf8) == 0x68) {
	SETIVEC(i, IRET_SEG, IRET_OFF);
    } else if (i < 0x78 || i == DOS_HELPER_INT || i == 0xe7) {
	SETIVEC(i, BIOSSEG, INT_OFF(i));
    }
  }

  /* Let kernel handle this, no need to return to DOSEMU */
 #if 0
  SETIVEC(0x1c, BIOSSEG + 0x10, INT_OFF(0x1c) +2 - 0x100);
 #endif

  SETIVEC(0x09, INT09_SEG, INT09_OFF);
  SETIVEC(0x0a, BIOSSEG, EOI_OFF);
  SETIVEC(0x08, INT08_SEG, INT08_OFF);
  /* 0x30 and 0x31 are not vectors, they are the 5-byte long jump.
   * While 0x30 is overwritten entirely, only one byte is overwritten
   * in 0x31. We need to zero it out so that it at least does not
   * point into random bios location. */
  SETIVEC(0x31, 0, 0);
  SETIVEC(0x70, INT70_SEG, INT70_OFF);
  SETIVEC(0x71, INT71_SEG, INT71_OFF);
  SETIVEC(0x1e, INT1E_SEG, INT1E_OFF);
  SETIVEC(0x41, INT41_SEG, INT41_OFF);
  SETIVEC(0x46, INT46_SEG, INT46_OFF);
  SETIVEC(0x75, INT75_SEG, INT75_OFF);

#ifdef IPX
  /* IPX. Dummy but should not crash */
  if (config.ipxsup)
    SETIVEC(0x7a, BIOSSEG, INT_OFF(0x7a));
#endif

  /* Install new handler for video-interrupt into bios_f000_int10ptr,
   * for video initialization at f800:4200
   * If config_vbios_seg=0xe000 -> e000:3, else c000:3
   * Next will be the call to int0xe6,al=8 which starts video BIOS init
   */
  install_int_10_handler();

  {
    /* update boot drive in Banner-code */
    unsigned ptr;

    ptr = SEGOFF2LINEAR(BIOSSEG, ((long)bios_f000_bootdrive - (long)bios_f000));
    WRITE_BYTE(ptr, config.hdiskboot >= 2 || config.hdiskboot == -1 ? 0x80 : 0);
  }

  bios_mem_setup();		/* setup values in BIOS area */
}

static void bios_reset(void)
{
  dos_post_boot_reset();
  iodev_reset();		/* reset all i/o devices          */
  _AL = DOS_HELPER_COMMANDS_DONE;
  while (dos_helper());		/* release memory used by helper utilities */
#ifdef USE_MHPDBG
  mhp_debug(DBG_BOOT, 0, 0);
#endif
}

void bios_setup_init(void)
{
  li_tid = coopth_create("late_init");
  coopth_set_permanent_post_handler(li_tid, late_init_post);
}

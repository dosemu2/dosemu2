#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <errno.h>

#ifdef __linux__
#include <linux/version.h>
#endif

#include "config.h"
#include "version.h"
#include "emu.h"
#include "memory.h"
#include "dpmi.h"
#include "bios.h"
#include "int.h"
#include "timers.h"
#include "video.h"
#include "vc.h"
#include "mouse.h"
#include "port.h"
#include "joystick.h"
#include "pktdrvr.h"
#include "ipx.h"
#include "bitops.h"
#include "pic.h"
#include "dma.h"
#include "xms.h"
#include "lowmem.h"
#include "iodev.h"
#include "priv.h"
#include "doshelpers.h"
#include "cpu-emu.h"
#include "kvm.h"
#include "mapping.h"

#if 0
static inline void dbug_dumpivec(void)
{
  int i;

  for (i = 0; i < 256; i++) {
    int j;

    dbug_printf("%02x %08x", i, ((unsigned int *) 0)[i << 1]);
    for (j = 0; j < 8; j++)
      dbug_printf(" %02x", ((unsigned char *) (BIOSSEG * 16 + 16 * i))[j]);
    dbug_printf("\n");
  }
}
#endif

/*
 * DANG_BEGIN_FUNCTION stdio_init
 *
 * description:
 *  Initialize stdio, open debugging output file if user specified one
 *
 * DANG_END_FUNCTION
 */
void stdio_init(void)
{
  setbuf(stdout, NULL);

  if(dbg_fd) {
    warn("DBG_FD already set\n");
    return;
  }

  if(config.debugout)
  {
    dbg_fd=fopen(config.debugout,"w");
    if(!dbg_fd) {
      error("can't open \"%s\" for writing debug file\n",
	      config.debugout);
      exit(1);
    }
    free(config.debugout);
    config.debugout = NULL;
  }
  else
  {
    dbg_fd=0;
    warn("No debug output file specified, debugging information will not be printed");
  }
  sync();  /* for safety */
}

/*
 * DANG_BEGIN_FUNCTION time_setting_init
 *
 * description:
 *  Beats me
 *
 * DANG_END_FUNCTION
 */
void time_setting_init(void)
{
  initialize_timers();
}

/*
 * DANG_BEGIN_FUNCTION timer_interrupt_init
 *
 * description:
 *  Tells the OS to send us periodic timer messages
 *
 * DANG_END_FUNCTION
 */
void timer_interrupt_init(void)
{
  struct itimerval itv;
  int delta;

  delta = (config.update / TIMER_DIVISOR);
  /* Check that the kernel actually supports such a frequency - we
   * can't go faster than jiffies with setitimer()
   */
  if (((delta/1000)+1) < (1000/sysconf(_SC_CLK_TCK))) {
    c_printf("TIME: FREQ too fast, using defaults\n");
    config.update = 54925; config.freq = 18;
    delta = 54925 / TIMER_DIVISOR;
  }

  itv.it_interval.tv_sec = 0;
  itv.it_interval.tv_usec = delta;
  itv.it_value.tv_sec = 0;
  itv.it_value.tv_usec = delta;
  c_printf("TIME: using %d usec for updating ALRM timer\n", delta);

  setitimer(ITIMER_REAL, &itv, NULL);
}

/*
 * DANG_BEGIN_FUNCTION map_video_bios
 *
 * description:
 *  Map the video bios into main memory
 *
 * DANG_END_FUNCTION
 */
uint32_t int_bios_area[0x500/sizeof(uint32_t)];
void map_video_bios(void)
{
  v_printf("Mapping VBIOS = %d\n",config.mapped_bios);

  if (config.mapped_bios) {
    if (config.vbios_file) {
      warn("WARN: loading VBIOS %s into mem at %#x (%#X bytes)\n",
	   config.vbios_file, VBIOS_START, VBIOS_SIZE);
      load_file(config.vbios_file, 0, LINEAR2UNIX(VBIOS_START), VBIOS_SIZE);
    }
    else if (config.vbios_copy) {
      warn("WARN: copying VBIOS from /dev/mem at %#x (%#X bytes)\n",
	   VBIOS_START, VBIOS_SIZE);
      load_file("/dev/mem", VBIOS_START, LINEAR2UNIX(VBIOS_START), VBIOS_SIZE);
    }
    else {
      warn("WARN: copying VBIOS file from /dev/mem\n");
      load_file("/dev/mem", VBIOS_START, LINEAR2UNIX(VBIOS_START), VBIOS_SIZE);
    }

    /* copy graphics characters from system BIOS */
    load_file("/dev/mem", GFX_CHARS, LINEAR2UNIX(GFX_CHARS), GFXCHAR_SIZE);

    memcheck_addtype('V', "Video BIOS");
    memcheck_reserve('V', VBIOS_START, VBIOS_SIZE);
    if (!config.vbios_post || config.chipset == VESA)
      load_file("/dev/mem", 0, (unsigned char *)int_bios_area, sizeof(int_bios_area));
  }
}

/*
 * DANG_BEGIN_FUNCTION map_custom_bios
 *
 * description:
 *  Setup the dosemu amazing custom BIOS, quietly overwriting anything
 *  was copied there before. Do not overwrite graphic fonts!
 *
 * DANG_END_FUNCTION
 */
static inline void map_custom_bios(void)
{
  unsigned int ptr;
  u_long n;

  n = (u_long)bios_f000_endpart1 - (u_long)bios_f000;
  ptr = SEGOFF2LINEAR(BIOSSEG, 0);
  MEMCPY_2DOS(ptr, bios_f000, n);

  n = (u_long)bios_f000_end - (u_long)bios_f000_part2;
  ptr = SEGOFF2LINEAR(BIOSSEG, ((u_long)bios_f000_part2 - (u_long)bios_f000));
  MEMCPY_2DOS(ptr, bios_f000_part2, n);
  /* Initialize the lowmem heap that resides in a custom bios */
  lowmem_heap_init();
}

/*
 * DANG_BEGIN_FUNCTION memory_init
 *
 * description:
 *  Set up all memory areas as would be present on a typical i86 during
 * the boot phase.
 *
 * DANG_END_FUNCTION
 */
void memory_init(void)
{
  map_custom_bios();           /* map the DOSEMU bios */
  setup_interrupts();          /* setup interrupts */
  bios_setup_init();
}

/*
 * DANG_BEGIN_FUNCTION device_init
 *
 * description:
 *  Calls all initialization routines for devices (keyboard, video, serial,
 *    disks, etc.)
 *
 * DANG_END_FUNCTION
 */
void device_init(void)
{
  pit_init();		/* for native speaker */
  video_config_init();	/* privileged part of video init */
  keyb_priv_init();
  mouse_priv_init();
}

static void *mem_reserve_contig(void *base, uint32_t size, uint32_t dpmi_size,
	void **base2)
{
  void *result;

  result = mmap_mapping_ux(MAPPING_INIT_LOWRAM | MAPPING_SCRATCH |
      MAPPING_DPMI | MAPPING_NOOVERLAP, base, size + dpmi_size, PROT_NONE);
  if (result == MAP_FAILED)
    return result;

  *base2 = result + size;
  return result;
}

static void *mem_reserve_split(void *base, uint32_t size, uint32_t dpmi_size,
	void **base2)
{
  void *result;
  void *dpmi_base;

  /* lowmem_base is not yet available, so use _ux version */
  result = mmap_mapping_ux(MAPPING_INIT_LOWRAM | MAPPING_SCRATCH |
      MAPPING_NOOVERLAP, base, size, PROT_NONE);
  if (result == MAP_FAILED)
    return result;
  if (!config.dpmi)
    return result;
  assert(config.dpmi_lin_rsv_base != (dosaddr_t)-1);
  dpmi_base = (void*)(((uintptr_t)result + config.dpmi_lin_rsv_base) & 0xffffffff);
  dpmi_base = mmap_mapping_ux(MAPPING_DPMI | MAPPING_SCRATCH |
      MAPPING_NOOVERLAP, dpmi_base, dpmi_size, PROT_NONE);
  if (dpmi_base == MAP_FAILED) {
    perror ("DPMI mmap");
    exit(EXIT_FAILURE);
  }

  *base2 = dpmi_base;
  return result;
}

/*
 * DANG_BEGIN_FUNCTION mem_reserve
 *
 * description:
 *  reserves address space seen by DOS and DPMI apps
 *   There are three possibilities:
 *  1) 0-based mapping: one map at 0 of 1088k, the rest below 1G
 *     This is only used for i386 + vm86() (not KVM/CPUEMU)
 *  2) non-zero-based mapping: one combined mmap, everything below 4G
 *  3) config.dpmi_lin_rsv_base is set: honour it
 *
 * DANG_END_FUNCTION
 */
static void *mem_reserve(void **base2, void **r_dpmi_base)
{
  void *result;
  void *dpmi_base;
  uint32_t memsize = LOWMEM_SIZE + HMASIZE;
  uint32_t dpmi_size = PAGE_ALIGN(config.dpmi_lin_rsv_size * 1024);
  uint32_t dpmi_memsize = dpmi_mem_size();

#ifdef __i386__
  if (config.cpu_vm == CPUVM_VM86) {
    if (config.dpmi && config.dpmi_lin_rsv_base == (dosaddr_t)-1)
      result = mem_reserve_contig(0, memsize, dpmi_size, base2);
    else
      result = mem_reserve_split(0, memsize, dpmi_size, base2);
    if (result == MAP_FAILED) {
      const char *msg =
	"You can most likely avoid this problem by running\n"
	"sysctl -w vm.mmap_min_addr=0\n"
	"as root, or by changing the vm.mmap_min_addr setting in\n"
	"/etc/sysctl.conf or a file in /etc/sysctl.d/ to 0.\n"
	"If this doesn't help, disable selinux in /etc/selinux/config\n";
#ifdef X86_EMULATOR
      if (errno == EPERM || errno == EACCES) {
	/* switch on vm86-only JIT CPU emulation with non-zero base */
	config.cpu_vm = CPUVM_EMU;
	config.cpuemu = 3;
	init_emu_cpu();
	c_printf("CONF: JIT CPUEMU set to 3 for %d86\n", (int)vm86s.cpu_type);
	error("Using CPU emulation because vm.mmap_min_addr > 0\n");
	error("@%s", msg);
      } else
#endif
      {
	perror("LOWRAM mmap");
	error("Cannot map low DOS memory (the first 640k)\n");
	error("@%s", msg);
	exit(EXIT_FAILURE);
      }
    } else {
      /* some DPMI clients don't like negative memory pointers,
       * i.e. over 0x80000000. In fact, Screamer game won't work
       * with anything above 0x40000000 */
      dpmi_base = mapping_find_hole(LOWMEM_SIZE, 0x40000000, dpmi_memsize);
      if (dpmi_base == MAP_FAILED) {
        error("MAPPING: cannot find mem hole for DPMI pool of %x\n", dpmi_memsize);
        dump_maps();
        exit(EXIT_FAILURE);
      }
      dpmi_base = mmap_mapping_ux(MAPPING_DPMI | MAPPING_SCRATCH |
          MAPPING_NOOVERLAP, dpmi_base, dpmi_memsize, PROT_NONE);
      if (dpmi_base == MAP_FAILED) {
        error("MAPPING: cannot create mem pool for DPMI, size=%x\n", dpmi_memsize);
        dump_maps();
        exit(EXIT_FAILURE);
      }
      *r_dpmi_base = dpmi_base;
      return result;
    }
  }
#endif

  if (config.dpmi && config.dpmi_lin_rsv_base == (dosaddr_t)-1) { /* contiguous memory */
    result = mem_reserve_contig((void*)-1, memsize, dpmi_size + dpmi_memsize,
          base2);
    dpmi_base = *base2 + dpmi_size;
  } else {
    result = mem_reserve_split((void*)-1, memsize + dpmi_memsize, dpmi_size,
          base2);
    dpmi_base = result + memsize;
  }
  if (result == MAP_FAILED) {
    perror ("LOWRAM mmap");
    exit(EXIT_FAILURE);
  }
  *r_dpmi_base = dpmi_base;
  return result;
}

/*
 * DANG_BEGIN_FUNCTION low_mem_init
 *
 * description:
 *  Initializes the lower 1Meg via mmap & sets up the HMA region
 *
 * DANG_END_FUNCTION
 */
void low_mem_init(void)
{
  void *lowmem, *base2, *dpmi_base;
  int result;

  open_mapping(MAPPING_INIT_LOWRAM);
  g_printf ("DOS+HMA memory area being mapped in\n");
  lowmem = alloc_mapping(MAPPING_INIT_LOWRAM, LOWMEM_SIZE + HMASIZE);
  if (lowmem == MAP_FAILED) {
    perror("LOWRAM alloc");
    leavedos(98);
  }

  mem_base = mem_reserve(&base2, &dpmi_base);
  result = alias_mapping(MAPPING_INIT_LOWRAM, 0, LOWMEM_SIZE + HMASIZE,
			 PROT_READ | PROT_WRITE | PROT_EXEC, lowmem);
  if (result == -1) {
    perror ("LOWRAM mmap");
    exit(EXIT_FAILURE);
  }
  c_printf("Conventional memory mapped from %p to %p\n", lowmem, mem_base);
  dpmi_set_mem_bases(base2, dpmi_base);

  if (config.cpu_vm == CPUVM_KVM)
    init_kvm_monitor();

  /* keep conventional memory protected as long as possible to protect
     NULL pointer dereferences */
  mprotect_mapping(MAPPING_LOWMEM, 0, config.mem_size * 1024, PROT_NONE);

  /* R/O protect 0xf0000-0xf4000 */
  if (!config.umb_f0)
    mprotect_mapping(MAPPING_LOWMEM, 0xf0000, 0x4000, PROT_READ);
}

/*
 * DANG_BEGIN_FUNCTION version_init
 *
 * description:
 *  Find version of OS running and set necessary global parms.
 *
 * DANG_END_FUNCTION
 */
void version_init(void) {

#ifdef __linux__
  struct utsname unames;
  char *s;

  uname(&unames);
  kernel_version_code = strtol(unames.release, &s,0) << 16;
  kernel_version_code += strtol(s+1, &s,0) << 8;
  kernel_version_code += strtol(s+1, &s,0);

  if (kernel_version_code < KERNEL_VERSION(2, 6, 6)) {
    error("You are running a kernel older than 2.6.6.\n"
          "This may be very problematic for DOSEMU.\n"
          "Please upgrade to a newer Linux kernel before reporting\n"
          "problems.\n");
  }
#else
  kernel_version_code = 0;
#endif
}

void print_version(void)
{
  struct utsname unames;

  uname(&unames);
  warn("DOSEMU-%s is coming up on %s version %s %s %s\n", VERSTR,
       unames.sysname, unames.release, unames.version, unames.machine);
  warn("Compiled with "
#ifdef __clang__
  "clang version %d.%d.%d (gnuc %d.%d)", __clang_major__, __clang_minor__,
      __clang_patchlevel__, __GNUC__, __GNUC_MINOR__);
#else
  "gcc version %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif
#ifdef i386
  warn(" 32bit\n");
#else
  warn(" 64bit\n");
#endif
#ifdef CFLAGS_STR
#define __S(...) #__VA_ARGS__
#define _S(x) __S(x)
  warn("CFLAGS: %s\n", _S(CFLAGS_STR));
#endif
}

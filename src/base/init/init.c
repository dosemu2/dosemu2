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

#include "init.h"
#include "version.h"
#include "emu.h"
#include "memory.h"
#include "emudpmi.h"
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
#include "smalloc.h"
#include "vgaemu.h"
#include "cpi.h"

#define GFX_CHARS       0xffa6e

smpool main_pool;

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
    if (config.debugout == NULL) {
        char *home = getenv("HOME");
        if (home) {
            const static char *debugout = "/.dosemu/boot.log";
            config.debugout = malloc(strlen(home) + strlen(debugout) + 1);
            strcpy(config.debugout, home);
            strcat(config.debugout, debugout);
        }
    }
    if (config.debugout && config.debugout[0] != '-') {
        dbg_fd = fopen(config.debugout, "we");
        if (!dbg_fd)
            error("can't open \"%s\" for writing\n", config.debugout);
        else
            setlinebuf(dbg_fd);
    }

    real_stderr = stderr;
#if defined(HAVE_ASSIGNABLE_STDERR) && defined(HAVE_FOPENCOOKIE)
    if (dbg_fd)
        stderr = fstream_tee(stderr, dbg_fd);
#endif
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
    load_file("/dev/mem", GFX_CHARS, vga_rom_08, 128 * 8);

    memcheck_addtype('V', "Video BIOS");
    memcheck_reserve('V', VBIOS_START, VBIOS_SIZE);
    if (!config.vbios_post || config.chipset == VESA)
      load_file("/dev/mem", 0, (unsigned char *)int_bios_area, sizeof(int_bios_area));
  }
}

static void setup_fonts(void)
{
  uint8_t *f8, *f14, *f16;
  int l8, l14, l16;
  uint16_t cp;
  char *path;

  if (!config.internal_cset || strncmp(config.internal_cset, "cp", 2) != 0)
    return;
  cp = atoi(config.internal_cset + 2);
  if (!cp)
    return;
  c_printf("loading fonts for %s\n", config.internal_cset);
  path = assemble_path(dosemu_lib_dir_path, "cpi");
  f8 = cpi_load_font(path, cp, 8, 8, &l8);
  f14 = cpi_load_font(path, cp, 8, 14, &l14);
  f16 = cpi_load_font(path, cp, 8, 16, &l16);
  if (f8 && f14 && f16) {
    assert(l8 == 256 * 8);
    memcpy(vga_rom_08, f8, l8);
    assert(l14 == 256 * 14);
    memcpy(vga_rom_14, f14, l14);
    assert(l16 == 256 * 16);
    memcpy(vga_rom_16, f16, l16);
  } else {
    error("CPI not found for %s\n", config.internal_cset);
  }
  free(f8);
  free(f14);
  free(f16);
  free(path);
}

/*
 * DANG_BEGIN_FUNCTION map_custom_bios
 *
 * description:
 *  Setup the dosemu amazing custom BIOS, quietly overwriting anything
 *  was copied there before.
 *
 * DANG_END_FUNCTION
 */
void map_custom_bios(void)
{
  unsigned int ptr;

  /* make sure nothing overlaps */
  assert(bios_data_start >= DOSEMU_LMHEAP_OFF + DOSEMU_LMHEAP_SIZE);
  /* Copy the BIOS into DOS memory */
  ptr = SEGOFF2LINEAR(BIOSSEG, bios_data_start);
  e_invalidate(ptr, DOSEMU_BIOS_SIZE());
  MEMCPY_2DOS(ptr, _binary_bios_o_bin_start, DOSEMU_BIOS_SIZE());
  setup_fonts();
  /* Initialise the ROM-BIOS graphic font (lower half only) */
  MEMCPY_2DOS(GFX_CHARS, vga_rom_08, 128 * 8);
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
  /* Initialize the lowmem heap that resides in a custom bios */
  lowmem_init();
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
  video_config_init();	/* privileged part of video init */
  keyb_priv_init();
  mouse_priv_init();
}

/*
 * DANG_BEGIN_FUNCTION mem_reserve
 *
 * description:
 *  reserves address space seen by DOS and DPMI apps
 *   There are two possibilities:
 *  1) 0-based mapping: one map at 0 of 1088k, and the combined map
 *     below.
 *     This is only used for i386 + vm86() (not KVM/CPUEMU)
 *  2) non-zero-based mapping: one combined mmap.
 *     Everything needs to be below 4G for native DPMI
 *     (intrinsically) and JIT CPUEMU (for now)
 *
 * DANG_END_FUNCTION
 */
static void *mem_reserve(uint32_t memsize)
{
  void *result;
  int cap = MAPPING_INIT_LOWRAM | MAPPING_SCRATCH | MAPPING_DPMI;
  int prot = PROT_NONE;

#ifdef __i386__
  if (config.cpu_vm == CPUVM_VM86) {
    result = mmap_mapping(MAPPING_NULL | MAPPING_SCRATCH,
			  NULL, LOWMEM_SIZE + HMASIZE, PROT_NONE);
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
    }
  }
#endif

  result = mmap_mapping_huge_page_aligned(cap, memsize, prot);
  if (result == MAP_FAILED) {
    perror ("LOWRAM mmap");
    exit(EXIT_FAILURE);
  }
  return result;
}

static void low_mem_init_config_scrub(void)
{
  const uint32_t mem_1M = 1024 * 1024;
  /* 16Mb limit is for being in reach of DMAc */
  const uint32_t mem_16M = mem_1M * 16;
  uint32_t min_phys_rsv = LOWMEM_SIZE + EXTMEM_SIZE;

  if (EXTMEM_SIZE < HMASIZE)
    config.ext_mem = 64;
  if (config.xms_size) {
    /* reserve 1Mb for XMS mappings */
    min_phys_rsv += mem_1M;
    if (min_phys_rsv > mem_16M) {
      error("$_ext_mem too large, please set to (%d) or lower, or set $_xms=(0)\n",
	    (EXTMEM_SIZE - (min_phys_rsv - mem_16M)) / 1024);
      config.exitearly = 1;
      return;
    }
  }

  min_phys_rsv = roundUpToNextPowerOfTwo(LOWMEM_SIZE + EXTMEM_SIZE + XMS_SIZE);
  if (config.dpmi && min_phys_rsv > config.dpmi_base) {
    error("$_dpmi_base is too small, please set to at least (0x%x)\n", min_phys_rsv);
    config.exitearly = 1;
    return;
  }
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
  unsigned char *lowmem, *ptr, *ptr2;
  int result;
  uint32_t memsize;
  int32_t phys_rsv, phys_low;

  open_mapping(MAPPING_INIT_LOWRAM);
  g_printf ("DOS+HMA memory area being mapped in\n");
  lowmem = alloc_mapping_huge_page_aligned(MAPPING_INIT_LOWRAM, LOWMEM_SIZE +
	EXTMEM_SIZE);
  if (lowmem == MAP_FAILED) {
    perror("LOWRAM alloc");
    leavedos(98);
  }

  phys_low = roundUpToNextPowerOfTwo(LOWMEM_SIZE + EXTMEM_SIZE + XMS_SIZE);
  memsize = phys_low;
  if (config.dpmi)
    /* LOWMEM_SIZE accounted twice for alignment */
    memsize += config.dpmi_base + HUGE_PAGE_ALIGN(dpmi_mem_size());
  mem_base = mem_reserve(memsize);
  mem_base_mask = ~(uintptr_t)0;
#ifdef __x86_64__
  if (_MAP_32BIT) mem_base_mask = 0xffffffffu;
#endif
  register_hardware_ram_virtual('L', 0, LOWMEM_SIZE + HMASIZE, 0);
  result = alias_mapping(MAPPING_LOWMEM, 0, LOWMEM_SIZE + HMASIZE,
			 PROT_READ | PROT_WRITE | PROT_EXEC, lowmem);
  if (result == -1) {
    perror ("LOWRAM mmap");
    exit(EXIT_FAILURE);
  }
  c_printf("Conventional memory mapped from %p to %p\n", lowmem, mem_base);

  if (config.xms_size)
    memcheck_reserve('x', LOWMEM_SIZE + EXTMEM_SIZE, XMS_SIZE);

  sminit_comu(&main_pool, mem_base, memsize, mcommit, muncommit);
  ptr = smalloc(&main_pool, LOWMEM_SIZE + HMASIZE);
  assert(ptr == mem_base);
  /* smalloc uses PROT_READ | PROT_WRITE, needs to add PROT_EXEC here */
  mprotect_mapping(MAPPING_LOWMEM, 0, LOWMEM_SIZE + HMASIZE, PROT_READ | PROT_WRITE |
      PROT_EXEC);
  /* we have an uncommitted hole up to phys_low */
  ptr += phys_low;
  phys_rsv = phys_low - (LOWMEM_SIZE + HMASIZE);
  /* create non-identity mapping up to phys_low */
  ptr2 = smalloc_topdown(&main_pool, config.dpmi ? phys_low : phys_rsv);
  assert(ptr2);
  if (config.dpmi) {
    void *dptr = smalloc_fixed(&main_pool, MEM_BASE32(config.dpmi_base),
        dpmi_mem_size());
    assert(dptr);
    if (config.cpu_vm_dpmi == CPUVM_KVM) {
      /* map dpmi+uncommitted space to kvm */
      int prot = PROT_READ | PROT_WRITE | PROT_EXEC;
      mmap_kvm(MAPPING_INIT_LOWRAM, phys_low, ptr2 - ptr, ptr, phys_low, prot);
    }
    /* unused hole for alignment */
    ptr2 += LOWMEM_SIZE + HMASIZE;
  }

  /* LOWMEM_SIZE + HMASIZE == base */
  memcheck_addtype('X', "EXT MEM");
  memcheck_reserve('X', LOWMEM_SIZE + HMASIZE, EXTMEM_SIZE - HMASIZE);
  x_printf("Ext.Mem of size 0x%x at %#x\n", EXTMEM_SIZE - HMASIZE,
      LOWMEM_SIZE + HMASIZE);

  /* establish ext_mem alias access for int15 */
  register_hardware_ram_virtual('X', LOWMEM_SIZE + HMASIZE, phys_rsv,
	    DOSADDR_REL(ptr2));
  if (config.dpmi) {
    register_hardware_ram_virtual('U', DOSADDR_REL(ptr2), phys_rsv,
	    LOWMEM_SIZE + HMASIZE);
    /* create ext_mem alias for dpmi */
    result = alias_mapping(MAPPING_EXTMEM, DOSADDR_REL(ptr2),
			 EXTMEM_SIZE - HMASIZE,
			 PROT_READ | PROT_WRITE,
			 lowmem + LOWMEM_SIZE + HMASIZE);
    assert(result != -1);
  }

  /* R/O protect 0xf0000-0xf4000 */
  if (!config.umb_f0) {
    memcheck_addtype('R', "ROM at f000:0000 for $_umb_f0 = (off)");
    memcheck_reserve('R', 0xF0000, DOSEMU_LMHEAP_OFF);
  }
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
  warn("dosemu2-%s is coming up on %s version %s %s %s\n", VERSTR,
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

CONSTRUCTOR(static void init(void))
{
  register_config_scrub(low_mem_init_config_scrub);
}

/*
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/utsname.h>

#include "config.h"
#include "emu.h"
#include "timers.h"
#include "video.h"
#include "mouse.h"
#include "serial.h"
#include "keymaps.h"
#include "memory.h"
#include "bios.h"
#include "lpt.h"
#include "int.h"
#include "dosemu_config.h"
#include "init.h"
#include "disks.h"
#include "userhook.h"
#include "pktdrvr.h"

#include "dos2linux.h"
#include "utilities.h"
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif
#include "mhpdbg.h"

#include "mapping.h"

/*
 * XXX - the mem size of 734 is much more dangerous than 704. 704 is the
 * bottom of 0xb0000 memory.  use that instead?
 */
#define MAX_MEM_SIZE    640


#if 0  /* initialized in data.c (via emu.h) */
struct debug_flags d =
{  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
/* d  R  W  D  C  v  X  k  i  T  s  m  #  p  g  c  w  h  I  E  x  M  n  P  r  S  j */
#endif

int kernel_version_code = 0;
int config_check_only = 0;

int dosemu_argc;
char **dosemu_argv;
char *dosemu_proc_self_exe = NULL;

static void     check_for_env_autoexec_or_config(void);
static void     usage(char *basename);

/*
 * DANG_BEGIN_FUNCTION cpu_override
 * 
 * description: 
 * Process user CPU override from the config file ('cpu xxx') or
 * from the command line. Returns the selected CPU identifier or
 * -1 on error. 'config.realcpu' should have already been defined.
 * 
 * DANG_END_FUNCTION
 * 
 */
int cpu_override (int cpu)
{
    switch (cpu) {
	case 2:
	    return CPU_286;
	case 5: case 6:
	    if (config.realcpu > CPU_486) return CPU_586;
	/* fall thru */
	case 4:
	    if (config.realcpu > CPU_386) return CPU_486;
	/* fall thru */
	case 3:
	    return CPU_386;
    }
    return -1;
}

static void dump_printf(char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

static void log_dump_printf(char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vlog_printf(-1, fmt, args);
    va_end(args);
}

void dump_config_status(void *printfunc)
{
    char *s;
    void (*print)(char *, ...) = (printfunc ? printfunc : dump_printf);

    if (!printfunc) {
      (*print)("\n-------------------------------------------------------------\n");
      (*print)("------dumping the runtime configuration _after_ parsing -----\n");
    }
    else (*print)("dumping the current runtime configuration:\n");
    (*print)("Version: dosemu-" VERSTR " versioncode = 0x%08x\n\n", DOSEMU_VERSION_CODE);
    (*print)("Running Kernel Version: linux-%d.%d.%d\n", kernel_version_code >> 16, (kernel_version_code >> 8) & 255, kernel_version_code & 255);
    (*print)("cpu ");
    switch (vm86s.cpu_type) {
      case CPU_386: s = "386"; break;
      case CPU_486: s = "486"; break;
      case CPU_586: s = "586"; break;
      default:  s = "386"; break;
    }
    (*print)("%s\nrealcpu ", s);
    switch (config.realcpu) {
      case CPU_386: s = "386"; break;
      case CPU_486: s = "486"; break;
      case CPU_586: s = "586"; break;
      default:  s = "386"; break;
    }
    (*print)("%s\n", s);
    (*print)("CPUclock %gMHz\ncpu_spd 0x%lx\ncpu_tick_spd 0x%lx\n",
	(((double)LLF_US)/config.cpu_spd), config.cpu_spd, config.cpu_tick_spd);

    (*print)("pci %d\nrdtsc %d\nmathco %d\nsmp %d\n",
                 config.pci, config.rdtsc, config.mathco, config.smp);
    (*print)("cpuspeed %d\n", config.CPUSpeedInMhz);
#ifdef X86_EMULATOR
    (*print)("cpuemu %d\n", config.cpuemu);
#endif

    if (config_check_only) mapping_init();
    if (mappingdriver.name)
      (*print)("mappingdriver %s\n", mappingdriver.name);
    else
      (*print)("mappingdriver %s\n", config.mappingdriver ? config.mappingdriver : "auto");
    if (config_check_only) mapping_close();
    (*print)("hdiskboot %d\nmem_size %d\n",
        config.hdiskboot, config.mem_size);
    (*print)("ems_size 0x%x\nems_frame 0x%x\n",
        config.ems_size, config.ems_frame);
    (*print)("xms_size 0x%x\nmax_umb 0x%x\ndpmi 0x%x\n",
        config.xms_size, config.max_umb, config.dpmi);
    (*print)("mouse_flag %d\nmapped_bios %d\nvbios_file %s\n",
        config.mouse_flag, config.mapped_bios, (config.vbios_file ? config.vbios_file :""));
    (*print)("vbios_copy %d\nvbios_seg 0x%x\nvbios_size 0x%x\n",
        config.vbios_copy, config.vbios_seg, config.vbios_size);
    (*print)("console %d\nconsole_keyb %d\nconsole_video %d\n",
        config.console, config.console_keyb, config.console_video);
    (*print)("kbd_tty %d\nexitearly %d\n",
        config.kbd_tty, config.exitearly);
    (*print)("fdisks %d\nhdisks %d\nbootdisk %d\n",
        config.fdisks, config.hdisks, config.bootdisk);
    (*print)("term_esc_char 0x%x\nterm_color %d\nterm_updatefreq %d\n",
        config.term_esc_char, config.term_color, config.term_updatefreq);
    switch (config.term_charset) {
      case CHARSET_LATIN: s = "latin"; break;
      case CHARSET_LATIN1: s = "latin1"; break;
      case CHARSET_LATIN2: s = "latin2"; break;
      case CHARSET_IBM: s = "ibm"; break;
      case CHARSET_FULLIBM: s = "fullibm"; break;
      default: s = "(unknown)";
    }
    (*print)("term_charset \"%s\"\nX_updatelines %d\nX_updatefreq %d\n",
        s, config.X_updatelines, config.X_updatefreq);
    (*print)("xterm_title\n", config.xterm_title);
    (*print)("X_display \"%s\"\nX_title \"%s\"\nX_icon_name \"%s\"\n",
        (config.X_display ? config.X_display :""), config.X_title, config.X_icon_name);
    (*print)("X_blinkrate %d\nX_sharecmap %d\nX_mitshm %d\n",
        config.X_blinkrate, config.X_sharecmap, config.X_mitshm);
    (*print)("X_fixed_aspect %d\nX_aspect_43 %d\nX_lin_filt %d\n",
        config.X_fixed_aspect, config.X_aspect_43, config.X_lin_filt);
    (*print)("X_bilin_filt %d\nX_mode13fact %d\nX_winsize_x %d\n",
        config.X_bilin_filt, config.X_mode13fact, config.X_winsize_x);
    (*print)("X_winsize_y %d\nX_gamma %d\nvgaemu_memsize 0x%x\n",
        config.X_winsize_y, config.X_gamma, config.vgaemu_memsize);
    (*print)("vesamode_list %p\nX_lfb %d\nX_pm_interface %d\n",
        config.vesamode_list, config.X_lfb, config.X_pm_interface);
    (*print)("X_keycode %d\nX_font \"%s\"\n",
        config.X_keycode, config.X_font);
    (*print)("X_mgrab_key \"%s\"\n",  config.X_mgrab_key);
    (*print)("X_background_pause %d\n", config.X_background_pause);

    switch (config.chipset) {
      case PLAINVGA: s = "plainvga"; break;
      case TRIDENT: s = "trident"; break;
      case ET4000: s = "et4000"; break;
      case DIAMOND: s = "diamond"; break;
      case S3: s = "s3"; break;
      case AVANCE: s = "avance"; break;
      case ATI: s = "ati"; break;
      case CIRRUS: s = "cirrus"; break;
      case MATROX: s = "matrox"; break;
      case WDVGA: s = "wdvga"; break;
      case SIS: s = "sis"; break;
#ifdef USE_SVGALIB
      case SVGALIB: s = "svgalib"; break;
#endif
      default: s = "unknown"; break;
    }
    (*print)("config.X %d\nhogthreshold %d\nchipset \"%s\"\n",
        config.X, config.hogthreshold, s);
    switch (config.cardtype) {
      case CARD_VGA: s = "VGA"; break;
      case CARD_MDA: s = "MGA"; break;
      case CARD_CGA: s = "CGA"; break;
      case CARD_EGA: s = "EGA"; break;
      default: s = "unknown"; break;
    }
    (*print)("cardtype \"%s\"\npci_video %d\nfullrestore %d\n",
        s, config.pci_video, config.fullrestore);
    (*print)("gfxmemsize %d\nvga %d\n",
        config.gfxmemsize, config.vga);
    switch (config.speaker) {
      case SPKR_OFF: s = "off"; break;
      case SPKR_NATIVE: s = "native"; break;
      case SPKR_EMULATED: s = "emulated"; break;
      default: s = "wrong"; break;
    }
    (*print)("dualmon %d\nforce_vt_switch %d\nspeaker \"%s\"\n",
        config.dualmon, config.force_vt_switch, s);
    (*print)("update %d\nfreq %d\n",
        config.update, config.freq);
    (*print)("tty_lockdir \"%s\"\ntty_lockfile \"%s\"\nconfig.tty_lockbinary %d\n",
        config.tty_lockdir, config.tty_lockfile, config.tty_lockbinary);
    (*print)("num_ser %d\nnum_lpt %d\nfastfloppy %d\nfull_file_locks %d\n",
        config.num_ser, config.num_lpt, config.fastfloppy, config.full_file_locks);
    (*print)("emusys \"%s\"\nemuini \"%s\"\n",
        (config.emusys ? config.emusys : ""), (config.emuini ? config.emuini : ""));
    (*print)("dosbanner %d\nvbios_post %d\ndetach %d\n",
        config.dosbanner, config.vbios_post, config.detach);
    (*print)("debugout \"%s\"\n",
        (config.debugout ? config.debugout : (unsigned char *)""));
    {
	char buf[256];
	GetDebugFlagsHelper(buf, 0);
	(*print)("debug_flags \"%s\"\n", buf);
    }
    if (!printfunc) dump_keytable(stderr, config.keytable);
    (*print)("pre_stroke \"%s\"\n", (config.pre_stroke ? config.pre_stroke : (unsigned char *)""));
    (*print)("irqpassing= ");
    if (config.sillyint) {
      int i;
      for (i=0; i <16; i++) {
        if (config.sillyint & (1<<i)) {
          (*print)("IRQ%d", i);
          if (config.sillyint & (0x10000<<i))
            (*print)("(sigio) ");
          else
            (*print)(" ");
        }
      }
      (*print)("\n");
    }
    else (*print)("none\n");
    (*print)("must_spare_hardware_ram %d\n",
        config.must_spare_hardware_ram);
    {
      int need_header_line =1;
      int i;
      for (i=0; i<(sizeof(config.hardware_pages)); i++) {
        if (config.hardware_pages[i]) {
          if (need_header_line) {
            (*print)("hardware_pages:\n");
            need_header_line = 0;
          }
          (*print)("%05x ", (i << 12) + 0xc8000);
        }
      }
      if (!need_header_line) (*print)("\n");
    }
    (*print)("ipxsup %d\nvnet %d\npktflags 0x%x\n",
	config.ipxsup, config.vnet, config.pktflags);
    
    {
        int i;
	for (i = 0; i < config.num_lpt; i++)
          printer_print_config(i, print);
    }

    {
	int i;
	int size = sizeof(config.features) / sizeof(config.features[0]);
	for (i = 0; i < size; i++) {
	  (*print)("feature_%d %d\n", i, config.features[i]);
	}
    }

    (*print)("\nSOUND:\nsb_base 0x%x\nsb_dma %d\nsb_hdma %d\nsb_irq %d\n"
	"mpu401_base 0x%x\nsb_dsp \"%s\"\nsb_mixer \"%s\"\nsound_driver \"%s\"\n",
        config.sb_base, config.sb_dma, config.sb_hdma, config.sb_irq,
	config.mpu401_base, config.sb_dsp, config.sb_mixer, config.sound_driver);
    (*print)("\nSOUND_OSS:\noss_min_frags 0x%x\noss_max_frags 0x%x\n"
	     "oss_stalled_frags 0x%x\noss_do_post %d\noss_min_extra_frags 0x%x\n",
        config.oss_min_frags, config.oss_max_frags, config.oss_stalled_frags,
	config.oss_do_post, config.oss_min_extra_frags);
    (*print)("\ncli_timeout %d\n", config.cli_timeout);
    (*print)("\npic_watchdog %d\n", config.pic_watchdog);
    (*print)("\nJOYSTICK:\njoy_device0 \"%s\"\njoy_device1 \"%s\"\njoy_dos_min %i\njoy_dos_max %i\njoy_granularity %i\njoy_latency %i\n",
        config.joy_device[0], config.joy_device[1], config.joy_dos_min, config.joy_dos_max, config.joy_granularity, config.joy_latency);

    if (!printfunc) {
      (*print)("\n--------------end of runtime configuration dump -------------\n");
      (*print)(  "-------------------------------------------------------------\n\n");
    }
}

static void 
open_terminal_pipe(char *path)
{
    terminal_fd = DOS_SYSCALL(open(path, O_RDWR));
    if (terminal_fd == -1) {
	terminal_pipe = 0;
	error("open_terminal_pipe failed - cannot open %s!\n", path);
	return;
    } else
	terminal_pipe = 1;
}

static void our_envs_init(char *usedoptions)
{
    struct utsname unames;
    char *s;
    char buf[256];
    int i,j;

    if (usedoptions) {
        for (i=0,j=0; i<256; i++) {
            if (usedoptions[i]) buf[j++] = i;
        }
        buf[j] = 0;
        setenv("DOSEMU_OPTIONS", buf, 1);
        strcpy(buf, "0");
        if (!usedoptions['X'] && is_console(0)) strcpy(buf, "1");
        setenv("DOSEMU_STDIN_IS_CONSOLE", buf, 1);
        return;
    }
    uname(&unames);
    kernel_version_code = strtol(unames.release, &s,0) << 16;
    kernel_version_code += strtol(s+1, &s,0) << 8;
    kernel_version_code += strtol(s+1, &s,0);
    sprintf(buf, "%d", kernel_version_code);
    setenv("KERNEL_VERSION_CODE", buf, 1);
    sprintf(buf, "%d", DOSEMU_VERSION_CODE);
    setenv("DOSEMU_VERSION_CODE", buf, 1);
    sprintf(buf, "%d", geteuid());
    setenv("DOSEMU_EUID", buf, 1);
    sprintf(buf, "%d", getuid());
    setenv("DOSEMU_UID", buf, 1);
}


static void restore_usedoptions(char *usedoptions)
{
    char *p = getenv("DOSEMU_OPTIONS");
    if (p) {
        memset(usedoptions,0,256);
        while (*p) {
	    usedoptions[(unsigned char)*p] = *p;
	    p++;
	}
    }
}

static int find_option(char *option, int argc, char **argv)
{
  int i;
  if (argc <=1 ) return 0;
  for (i=1; i < argc; i++) {
    if (!strcmp(argv[i], option)) {
      return i;
    }
  }
  return 0;
}

static int option_delete(int option, int *argc, char **argv)
{
  int i;
  if (option >= *argc) return 0;
  for (i=option; i < *argc; i++) {
    argv[i] = argv[i+1];
  }
  *argc -= 1;
  return option;
}

void secure_option_preparse(int *argc, char **argv)
{
  char *opt;
  int runningsuid = can_do_root_stuff && !under_root_login;

  static char * get_option(char *key, int with_arg)
  {
    char *p;
    char *basename;
    int o = find_option(key, *argc, argv);
    if (!o) return 0;
    o = option_delete(o, argc, argv);
    if (!with_arg) return "";
    if (!with_arg || o >= *argc) return "";
    if (argv[o][0] == '-') {
      basename = strrchr(argv[0], '/');   /* parse the program name */
      basename = basename ? basename + 1 : argv[0];
      usage(basename);
      exit(0);
    }
    p = strdup(argv[o]);
    option_delete(o, argc, argv);
    return p;
  }

  if (runningsuid) unsetenv("DOSEMU_LAX_CHECKING");
  else setenv("DOSEMU_LAX_CHECKING", "on", 1);

  if (*argc <=1 ) return;
 
  opt = get_option("--Fusers", 1);
  if (opt && opt[0]) {
    if (runningsuid) {
      fprintf(stderr, "Bypassing /etc/dosemu.users not allowed for suid-root\n");
      exit(0);
    }
    DOSEMU_USERS_FILE = opt;
  }

  opt = get_option("--Flibdir", 1);
  if (opt && opt[0]) {
    if (runningsuid) {
      fprintf(stderr, "Bypassing systemwide configuration not allowed for suid-root\n");
      exit(0);
    }
    dosemu_lib_dir_path = opt;
  }

  opt = get_option("--Fimagedir", 1);
  if (opt && opt[0]) {
    if (runningsuid) {
      fprintf(stderr, "Bypassing systemwide boot path not allowed for suid-root\n");
      exit(0);
    }
    dosemu_hdimage_dir_path = opt;
  }

  opt = get_option("-n", 0);
  if (opt) {
    if (runningsuid) {
      fprintf(stderr, "The -n option to bypass the system configuration files "
	      "is not allowed with sudo/suid-root\n");
      exit(0);
    }
    DOSEMU_USERS_FILE = NULL;
  }
}

static void config_pre_process(void)
{
    char *cpuflags;
    int k = 386;

    parse_debugflags("+cw", 1);
    open_proc_scan("/proc/cpuinfo");
    switch (get_proc_intvalue_by_key(
          kernel_version_code > 0x20100+74 ? "cpu family" : "cpu" )) {
      case 5: case 586:
      case 6: case 686:
      case 15:
        config.realcpu = CPU_586;
        cpuflags = get_proc_string_by_key("features");
        if (!cpuflags) {
          cpuflags = get_proc_string_by_key("flags");
        }
#ifdef X86_EMULATOR
        if (cpuflags && strstr(cpuflags, "mmx")) {
	  config.cpummx = 1;
        }
#endif
        if (cpuflags && strstr(cpuflags, "tsc")) {
          /* bogospeed currently returns 0; should it deny
           * pentium features, fall back into 486 case */
	  if ((kernel_version_code > 0x20100+126)
	       && (cpuflags = get_proc_string_by_key("cpu MHz"))) {
	    int di,df;
	    /* last known proc/cpuinfo format is xxx.xxxxxx, with 3
	     * int and 6 decimal digits - but what if there are less
	     * or more digits??? */
	    /* some broken kernel/cpu combinations apparently imply
	     * the existence of 0 Mhz CPUs */
	    if (sscanf(cpuflags,"%d.%d",&di,&df)==2 && (di || df)) {
		char cdd[8]; int i;
		long long chz = 0;
		char *p = cpuflags;
		while (*p!='.') p++; p++;
		for (i=0; i<6; i++) cdd[i]=(*p && isdigit(*p)? *p++:'0');
		cdd[6]=0; sscanf(cdd,"%d",&df);
		/* speed division factor to get 1us from CPU clocks - for
		 * details on fast division see timers.h */
		chz = (di * 1000000LL) + df;

		/* speed division factor to get 1us from CPU clock */
		config.cpu_spd = (LLF_US*1000000)/chz;

		/* speed division factor to get 838ns from CPU clock */
		config.cpu_tick_spd = (LLF_TICKS*1000000)/chz;

		warn ("Linux kernel %d.%d.%d; CPU speed is %lld Hz\n",
		   kernel_version_code >> 16, (kernel_version_code >> 8) & 255,
		   kernel_version_code & 255,chz);
/*		fprintf (stderr,"CPU speed factors %ld,%ld\n",
			config.cpu_spd, config.cpu_tick_spd); */
		config.CPUSpeedInMhz = di + (df>500000);
#ifdef X86_EMULATOR
		warn ("CPU-EMU speed is %d MHz\n",config.CPUSpeedInMhz);
#endif
		break;
	    }
	    else
	      cpuflags=NULL;
	  }
	  else
	    cpuflags=0;
	  if (!cpuflags) {
            if (!bogospeed(&config.cpu_spd, &config.cpu_tick_spd)) {
              break;
            }
          }
        }
        /* fall thru */
      case 4: case 486: config.realcpu = CPU_486;
      	break;
      default:
        error("Unknown CPU type!\n");
	/* config.realcpu is set to CPU_386 at this point */
    }
    config.mathco = strcmp(get_proc_string_by_key("fpu"), "yes") == 0;
    reset_proc_bufferptr();
    k = 0;
    while (get_proc_string_by_key("processor")) {
      k++;
      advance_proc_bufferptr();
    }
    if (k > 1) {
      config.smp = 1;		/* for checking overrides, later */
    }
    close_proc_scan();
    warn("Dosemu-" VERSTR " Running on CPU=%d86, FPU=%d\n",
         config.realcpu, config.mathco);
}

static void config_post_process(void)
{
    /* console scrub */
    if (config.X) {
#ifdef HAVE_KEYBOARD_V1
	if (!config.X_keycode) {
	    keyb_layout(-1);
	    c_printf("CONF: Forcing neutral Keyboard-layout, X-server will translate\n");
	}
#endif
	config.console_video = config.vga = 0;
	config.emuretrace = 0;	/* already emulated */
    }
    else {
	if (!can_do_root_stuff && config.console_video) {
	    /* force use of Slang-terminal on console too */
	    config.console = config.console_video = config.vga = 0;
	    config.cardtype = 0;
	    config.vbios_seg = 0;
	    config.mapped_bios = 0;
	    fprintf(stderr, "no console on low feature (non-suid root) DOSEMU\n");
	}
    }
    if ((config.vga || config.X) && config.mem_size > 640) {
	error("$_dosmem = (%d) not allowed for X and VGA console graphics, "
              "restricting to 640K\n", config.mem_size);
        config.mem_size = 640;
    }

    /* UID scrub */
    if (under_root_login)  c_printf("CONF: running exclusively as ROOT:");
    else {
      c_printf("CONF: mostly running as USER:");
    }
    c_printf(" uid=%d (cached %d) gid=%d (cached %d)\n",
        geteuid(), get_cur_euid(), getegid(), get_cur_egid());

    /* Speaker scrub */
#ifdef X86_EMULATOR
    if (config.cpuemu && config.speaker==SPKR_NATIVE) {
	c_printf("SPEAKER: can`t use native mode with cpu-emu\n");
	config.speaker=SPKR_EMULATED;
    }
#endif

    check_for_env_autoexec_or_config();

    if (config.pci && !can_do_root_stuff) {
        c_printf("CONF: Warning: PCI requires root, disabled\n");
        config.pci = 0;
    }

    if (config.vnet == VNET_TYPE_DSN) {
	error("DOSNET is deprecated and will soon be removed.\n"
	"Please consider using TAP instead.\n"
	"To do this, you have to set $_vnet=\"TAP\"\n");
    }
    if (dosemu_lib_dir_path != dosemulib_default)
        free(dosemu_lib_dir_path);
    dosemu_lib_dir_path = NULL;
    if (dosemu_hdimage_dir_path != dosemuhdimage_default)
        free(dosemu_hdimage_dir_path);
    dosemu_hdimage_dir_path = NULL;
    if (keymap_load_base_path != keymaploadbase_default)
        free(keymap_load_base_path);
    keymap_load_base_path = NULL;
    dexe_load_path = NULL;
}

static config_scrub_t config_scrub_func[100];

/*
 * DANG_BEGIN_FUNCTION register_config_scrub
 * 
 * description: 
 * register a function Enforce consistency upon the `config` structure after
 * all values have been set to remove silly option combinations
 * 
 * DANG_END_FUNCTION
 * 
 */
int register_config_scrub(config_scrub_t new_config_scrub)
{
	int i;
	int result = -1;
	for(i = 0; i < sizeof(config_scrub_func)/sizeof(config_scrub_func[0]); i++) {
		if (!config_scrub_func[i]) {
			config_scrub_func[i] = new_config_scrub;
			result = 0;
			break;
		}
	}
	if (result < 0) {
		c_printf("register_config_scrub failed > %d config_scrub functions\n",
			sizeof(config_scrub_func)/sizeof(config_scrub_func[0]));
	}
	return result;
}

/*
 * DANG_BEGIN_FUNCTION unregister_config_scrub
 * 
 * description: 
 * Complement of register_config_scrub 
 * This removes a scrub function.
 * 
 * DANG_END_FUNCTION
 * 
 */
void unregister_config_scrub( config_scrub_t old_config_scrub)
{
	int i;
	for(i = 0; i < sizeof(config_scrub_func)/sizeof(config_scrub_func[0]); i++) {
		if (config_scrub_func[i] == old_config_scrub) {
			config_scrub_func[i] = 0;
		}
	}
}

/*
 * DANG_BEGIN_FUNCTION config_scrub
 * 
 * description: 
 * Enforce consistency upon the `config` structure after
 * all values have been set to remove silly option combinations
 * 
 * DANG_END_FUNCTION
 * 
 */
static void config_scrub(void)
{
	int i;
	for(i = 0; i < sizeof(config_scrub_func)/sizeof(config_scrub_func[0]); i++) {
		config_scrub_t func = config_scrub_func[i];
		if (func) {
			func();
		}
	}
}

/*
 * DANG_BEGIN_FUNCTION config_init
 * 
 * description: 
 * This is called to parse the command-line arguments and config
 * files. 
 *
 * DANG_END_FUNCTION
 * 
 */
void 
config_init(int argc, char **argv)
{
    int             c=0;
    char           *confname = NULL;
    char           *dosrcname = NULL;
    char           *basename;
    char           *dexe_name = 0;
    char usedoptions[256];

    basename = strrchr(argv[0], '/');   /* parse the program name */
    basename = basename ? basename + 1 : argv[0];

    if (argv[1] && !strcmp("--version",argv[1])) {
      usage(basename);
      exit(0);
    }

    dosemu_argc = argc;
    dosemu_argv = argv;

    memset(usedoptions,0,sizeof(usedoptions));
    memcheck_type_init();
    our_envs_init(0);
    config_pre_process();

#ifdef X_SUPPORT
    /*
     * DANG_BEGIN_REMARK For simpler support of X, DOSEMU can be started
     * by a symbolic link called `xdos` which DOSEMU will use to switch
     * into X-mode. DANG_END_REMARK
     */
    if (strcmp(basename, "xdos") == 0) {
	    config.X = 1;	/* activate X mode if dosemu was */
	    usedoptions['X'] = 'X';
	/* called as 'xdos'              */
    }
#endif

    opterr = 0;
    if (strcmp(config_script_name, DEFAULT_CONFIG_SCRIPT))
      confname = config_script_path;
    while ((c = getopt(argc, argv, "ABCcF:f:I:kM:D:P:VNtsgh:H:x:KL:mn23456e:E:dXY:Z:o:Ou:U:")) != EOF) {
	usedoptions[(unsigned char)c] = c;
	switch (c) {
	case 'h':
	    config_check_only = atoi(optarg) + 1;
	    break;
	case 'H': {
	    dosdebug_flags = strtoul(optarg,0,0) & 255;
#if 0
	    if (!dosdebug_flags) dosdebug_flags = DBGF_WAIT_ON_STARTUP;
#endif
	    break;
            }
	case 'F':
	    if (get_orig_uid()) {
		FILE *f;
		if (!get_orig_euid()) {
		    /* we are running suid root as user */
		    fprintf(stderr, "Sorry, -F option not allowed here\n");
		    exit(1);
		}
		f=fopen(optarg, "r");
		if (!f) {
		  fprintf(stderr, "Sorry, no access to configuration script %s\n", optarg);
		  exit(1);
		}
		fclose(f);
	    }
	    confname = optarg;
	    break;
	case 'f':
	    {
		FILE *f;
		f=fopen(optarg, "r");
		if (!f) {
		  fprintf(stderr, "Sorry, no access to user configuration file %s\n", optarg);
		  exit(1);
		}
		fclose(f);
	        dosrcname = optarg;
	    }
	    break;
	case 'L':
	    dexe_name = optarg;
	    break;
	case 'I':
	    commandline_statements = optarg;
	    break;
	case 'd':
	    if (config.detach)
		break;
	    config.detach = (unsigned short) detach();
	    break;
	case 'D':
	    parse_debugflags(optarg, 1);
	    break;
	case 'O':
	    fprintf(stderr, "using stderr for debug-output\n");
	    dbg_fd = stderr;
	    break;
	case 'o':
	    config.debugout = strdup(optarg);
	    break;
	case '2': case '3': case '4': case '5': case '6':
#if 1
	    {
		int cpu = cpu_override (c-'0');
		if (cpu > 0) {
			fprintf(stderr,"CPU set to %d86\n",cpu);
			vm86s.cpu_type = cpu;
		}
		else
			fprintf(stderr,"error in CPU user override\n");
	    }
#endif
	    break;

	case 'u': {
		char *s=malloc(strlen(optarg)+3);
		s[0]='u'; s[1]='_';
		strcpy(s+2,optarg);
		define_config_variable(s);
	    }
	    break;
	case 'U':
	    init_uhook(optarg);
	    break;
	}
    }

    if (dbg_fd == 0) {
        if (config.debugout == NULL) {
            char *home = getenv("HOME");
            if (home) {
                const static char *debugout = "/.dosemu/boot.log";
                config.debugout = malloc(strlen(home) + strlen(debugout) + 1);
                strcpy(config.debugout, home);
                strcat(config.debugout, debugout);
            }
        }
        if (config.debugout != NULL) {
            dbg_fd = fopen(config.debugout, "w");
            if (!dbg_fd) {
                fprintf(stderr, "can't open \"%s\" for writing\n", config.debugout);
                exit(1);
            }
            free(config.debugout);
            config.debugout = NULL;
        }
    }

    if (config_check_only) set_debug_level('c',1);

    if (dexe_name || !strcmp(basename,"dosexec")) {
	if (!dexe_name) dexe_name = argv[optind];
	if (!dexe_name) {
	  usage(basename);
	  exit(1);
	}
	prepare_dexe_load(dexe_name);
	usedoptions['L'] = 'L';
    }

    our_envs_init(usedoptions);
    parse_config(confname,dosrcname);
    restore_usedoptions(usedoptions);

    if (config.exitearly && !config_check_only)
	exit(0);

    if (vm86s.cpu_type > config.realcpu) {
    	vm86s.cpu_type = config.realcpu;
    	fprintf(stderr, "CONF: emulated CPU forced down to real CPU: %ld86\n",vm86s.cpu_type);
    }

#ifdef __linux__
    optind = 0;
#endif
    opterr = 0;
    while ((c = getopt(argc, argv, "ABCcF:f:I:kM:D:P:v:VNtsgh:H:x:KLm23456e:dXY:Z:E:o:Ou:U:")) != EOF) {
	/* currently _NOT_ used option characters: abGjJlpqQrRSTwWyz */

	/* Note: /etc/dosemu.conf may have disallowed some options
	 *	 ( by removing them from $DOSEMU_OPTIONS ).
	 *	 We skip them by re-checking 'usedoptions'.
	 */
	if (!usedoptions[(unsigned char)c]) {
	    warn("command line option -%c disabled by dosemu.conf\n", c);
	}
	else switch (c) {
	case 'F':		/* previously parsed config file argument */
	case 'f':
	case 'h':
	case 'H':
	case 'I':
	case 'd':
	case 'o':
	case 'O':
	case 'L':
	case 'u':
	case 'U':
	case '2': case '3': case '4': case '5': case '6':
	    break;
	case 'A':
	    if (!dexe_running) config.hdiskboot = 0;
	    break;
	case 'B':
	    if (!dexe_running) config.hdiskboot = 2;
	    break;
	case 'C':
	    config.hdiskboot = 1;
	    break;
	case 'c':
	    config.console_video = 1;
	    break;
	case 'k':
	    config.console_keyb = 1;
	    break;
	case 'X':
#ifdef X_SUPPORT
	    config.X = 1;
#else
	    error("X support not compiled in\n");
#endif
	    break;
	case 'K':
#if 0 /* now dummy, leave it for compatibility */
	    warn("Keyboard interrupt enabled...this is still buggy!\n");
#endif
	    break;
	case 'M':{
		int             max_mem = config.vga ? 640 : MAX_MEM_SIZE;
		config.mem_size = atoi(optarg);
		if (config.mem_size > max_mem)
		    config.mem_size = max_mem;
		break;
	    }
	case 'D':
	    parse_debugflags(optarg, 1);
	    break;
	case 'P':
	    if (terminal_fd == -1) {
		open_terminal_pipe(optarg);
	    } else
		error("terminal pipe already open\n");
	    break;
	case 'V':
	    g_printf("Configuring as VGA video card & mapped ROM\n");
	    config.vga = 1;
	    config.mapped_bios = 1;
	    config.console_video = 1;
	    if (config.mem_size > 640)
		config.mem_size = 640;
	    break;
	case 'v':
	    config.cardtype = atoi(optarg);
	    if (config.cardtype > MAX_CARDTYPE)	/* keep it updated when adding a new card! */
		config.cardtype = 1;
	    g_printf("Configuring cardtype as %d\n", config.cardtype);
	    break;
	case 'N':
	    warn("DOS will not be started\n");
	    config.exitearly = 1;
	    break;
	case 't': /* obsolete "timer" option */
	    break;
	case 's':
	    g_printf("using new scrn size code\n");
	    sizes = 1;
	    break;
	case 'g': /* obsolete "graphics" option */
	    break;

	case 'x':
	    config.xms_size = atoi(optarg);
	    x_printf("enabling %dK XMS memory\n", config.xms_size);
	    break;

	case 'e':
	    config.ems_size = atoi(optarg);
	    g_printf("enabling %dK EMS memory\n", config.ems_size);
	    break;

	case 'm':
	    g_printf("turning MOUSE support on\n");
	    config.mouse_flag = 1;
	    break;

	case 'E':
	    g_printf("DOS command given on command line\n");
	    misc_e6_store_command(optarg,0);
	    break;

	case '?':
	default:
	    fprintf(stderr, "unrecognized option: -%c\n\r", c);
	    usage(basename);
	    fflush(stdout);
	    fflush(stderr);
	    exit(1);
	}
    }
    if (optind < argc) {
	g_printf("DOS command given on command line\n");
	misc_e6_store_command(argv[optind],1);
    }
    config_post_process();
    config_scrub();
    if (config_check_only) {
	dump_config_status(0);
	usage(basename);
	exit(1);
    } else {
	if (debug_level('c') >= 8)
	    dump_config_status(log_dump_printf);
    }
}


static void 
check_for_env_autoexec_or_config(void)
{
    char           *cp;
    cp = getenv("CONFIG");
    if (cp)
	config.emusys = cp;


     /*
      * The below is already reported in the conf. It is in no way an error
      * so why the messages?
      */

#if 0
    if (config.emusys)
	fprintf(stderr, "config extension = %s\n", config.emusys);
#endif
}

static void
usage(char *basename)
{
    fprintf(stderr,
	"dosemu-" VERSTR "\n\n"
	"USAGE:\n"
	"  %s  [-ABCckbVNtsgxKm23456ez] [-h{0|1|2}] [-H dflags \\\n"
	"       [-D flags] [-M SIZE] [-P FILE] [ {-F|-L} File ] \\\n"
	"       [-u confvar] [-f dosrcFile] [-o dbgfile] 2> vital_logs\n"
	"  %s --version\n\n"
	"    -2,3,4,5,6 choose 286, 386, 486 or 586 or 686 CPU\n"
	"    -A boot from first defined floppy disk (A)\n"
	"    -B boot from second defined floppy disk (B) (#)\n"
	"    -b map BIOS into emulator RAM (%%)\n"
	"    -C boot from first defined hard disk (C)\n"
	"    -c use PC console video (!%%)\n"
	"    -d detach (?)\n"
#ifdef X_SUPPORT
	"    -X run in X Window (#)\n"
#endif
    ,basename, basename);
    print_debug_usage(stderr);
    fprintf(stderr,
	"    -E STRING pass DOS command on command line\n"
	"    -e SIZE enable SIZE K EMS RAM\n"
	"    -F use File as global config-file\n"
	"    -f use dosrcFile as user config-file\n"
	"    -n causes DOSEMU to bypass the system configuration file\n"
	"    -L load and execute DEXE File\n"
	"    -I insert config statements (on commandline)\n"
	"    -h dump configuration to stderr and exit (sets -D+c)\n"
	"       0=no parser debug, 1=loop debug, 2=+if_else debug\n"
	"    -H wait for dosdebug terminal at startup and pass dflags\n"
	"    -K no effect, left for compatibility\n"
	"    -k use PC console keyboard (!)\n"
	"    -M set memory size to SIZE kilobytes (!)\n"
	"    -m enable mouse support (!#)\n"
	"    -N No boot of DOS\n"
	"    -O write debug messages to stderr\n"
	"    -o FILE put debug messages in file\n"
	"    -P copy debugging output to FILE\n"
	"    -u set user configuration variable 'confvar' prefixed by 'u_'.\n"
	"    -V use BIOS-VGA video modes (!#%%)\n"
	"    -v NUM force video card type\n"
	"    -x SIZE enable SIZE K XMS RAM\n"
	"    --version, print version of dosemu\n"
	"    (!) BE CAREFUL! READ THE DOCS FIRST!\n"
	"    (%%) require DOSEMU be run as root (i.e. suid)\n"
	"    (#) options do not fully work yet\n\n"
	"xdosemu [options]        == %s [options] -X\n"
    ,basename);
}

/*
 * $Id: config.c,v 1.2 1995/02/05 16:53:16 root Exp root $ 
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <linux/config.h>
#include <linux/utsname.h>

#include "config.h"
#include "emu.h"
#include "cpu.h"
#include "video.h"
#include "mouse.h"
#include "keymaps.h"
#include "memory.h"
#include "bios.h"
#include "kversion.h"

/* XXX - the mem size of 734 is much more dangerous than 704.
 * 704 is the bottom of 0xb0000 memory.  use that instead?
 */
#ifdef EXPERIMENTAL_GFX
#define MAX_MEM_SIZE    640
#else
#define MAX_MEM_SIZE    734	/* up close to the 0xB8000 mark */
#endif

struct debug_flags d =
{0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0};


static void parse_debugflags(const char *s);
static void usage(void);

void
configuration_init(void)
{
  int b;

  /* show 0 serial ports and 3 parallel ports, maybe a mouse, and the
   * configured number of floppy disks
   */
  CONF_NFLOP(configuration, config.fdisks);
  CONF_NSER(configuration, config.num_ser);
  CONF_NLPT(configuration, config.num_lpt);
  if ((mice->type == MOUSE_PS2) || (mice->intdrv))
    configuration |= CONF_MOUSE;

  if (config.mathco)
    configuration |= CONF_MATHCO;

  g_printf("CONFIG: 0x%04x    binary: ", configuration);
  for (b = 15; b >= 0; b--)
    g_printf("%s%s", (configuration & (1 << b)) ? "1" : "0",
		(b % 4) ? "" : " ");

  g_printf("\n");
}

/*
 * DANG_BEGIN_FUNCTION config_defaults
 *
 * description:
 *  Set all values in the `config` structure to their default value.
 *  These will be modified by the config parser.
 *
 * DANG_END_FUNCTION
 *
 */
static void
config_defaults(void)
{
  config.hdiskboot = 1;		/* default hard disk boot */

  config.mem_size = 640;
  config.ems_size = 0;
  config.ems_frame = 0xd000;
  config.xms_size = 0;
  config.max_umb = 0;
  config.dpmi_size = 0;
  config.mathco = 1;
  config.mouse_flag = 0;
  config.mapped_bios = 0;
  config.mapped_sbios = 0;
  config.vbios_file = NULL;
  config.vbios_copy = 0;
  config.vbios_seg = 0xc000;
  config.vbios_size = 0x10000;
  config.console_keyb = 0;
  config.kbd_tty = 0;
  config.console_video = 0;
  config.fdisks = 0;
  config.hdisks = 0;
  config.bootdisk = 0;
  config.exitearly = 0;
  config.term_method = METHOD_FAST;
  config.term_color = COLOR_NORMAL;
  config.term_updatelines = 25;
  config.term_updatefreq = 2;
  config.term_charset = CHARSET_LATIN;
  config.term_corner = 1;
  config.X_updatelines = 25;
  config.X_updatefreq  = 8;
  config.X_display     = NULL;     /* NULL means use DISPLAY variable */
  config.X_title       = "dosemu";
  config.X_icon_name   = "dosemu";
  config.X_blinkrate   = 8;
  config.X_keycode     = 0;
  config.X_font = "vga";
  config.usesX   = 0;
  config.X   = 0;
  config.hogthreshold = 10;     /* bad estimate of a good garrot value */
  config.chipset = PLAINVGA;
  config.cardtype = CARD_VGA;
  config.fullrestore = 0;
  config.graphics = 0;
  config.gfxmemsize = 256;
  config.vga = 0;		/* this flags BIOS graphics */
  config.dualmon =0;

  config.speaker = SPKR_EMULATED;

#if 0 /* This is too slow, but why? */
  config.update = 54945;
#else
  config.update = 27472;
#endif
  config.freq = 18;		/* rough frequency */

  config.timers = 1;		/* deliver timer ints */
  config.keybint = 0;		/* keyboard interrupts */

  config.num_ser = 0;
  config.num_lpt = 0;
  vm86s.cpu_type = CPU_386;
  config.fastfloppy = 1;

  config.emusys = (char *) NULL;
  config.emubat = (char *) NULL;
  tmpdir = strdup(tempnam("/tmp", "dosemu"));
  config.dosbanner = 1;
  config.allowvideoportaccess = 0;

  config.keyboard = KEYB_US;	/* What's the current keyboard  */
  config.key_map = key_map_us;	/* pointer to the keyboard-maps */
  config.shift_map = shift_map_us;	/* Here the Shilt-map           */
  config.alt_map = alt_map_us;	/* And the Alt-map              */
  config.num_table = num_table_dot;	/* Numeric keypad has a dot     */
  config.detach = 0; /* Don't detach from current tty and open new VT. */

  config.sillyint = 0;
  config.must_spare_hardware_ram =0;
  memset(config.hardware_pages,0,sizeof(config.hardware_pages));
  
  mice->fd         = -1;
  mice->type       = 0;
  mice->flags      = 0;
  mice->intdrv     = 0;
  mice->cleardtr   = 0;
  mice->baudRate   = 0;
  mice->sampleRate = 0;
  mice->lastButtons= 0;
  mice->chordMiddle= 0;
}

static void open_terminal_pipe(char *path)
{
  terminal_fd = DOS_SYSCALL(open(path, O_RDWR));
  if (terminal_fd == -1) {
    terminal_pipe = 0;
    error("ERROR: open_terminal_pipe failed - cannot open %s!\n", path);
    return;
  }
  else
    terminal_pipe = 1;
}

static void open_Xkeyboard_pipe(char *path)
{
  keypipe = DOS_SYSCALL(open(path, O_RDWR));
  if (keypipe == -1) {
    keypipe = 0;
    error("ERROR: open_Xkeyboard_pipe failed - cannot open %s!\n", path);
    return;
  }
  return;
}

static void open_Xmouse_pipe(char *path)
{
  mousepipe = DOS_SYSCALL(open(path, O_RDWR));
  if (mousepipe == -1) {
    mousepipe = 0;
    error("ERROR: open_Xmouse_pipe failed - cannot open %s!\n", path);
    return;
  }
  return;
}

/*
 * DANG_BEGIN_FUNCTION config_init
 *
 * description:
 *  This is called to parse the command-line arguments and config files.
 * DANG_END_FUNCTION
 *
 */
void config_init(int argc, char **argv)
{
  int c;
  char *confname = NULL;

  config_defaults();

/*
 * DANG_BEGIN_REMARK
 *  For simpler support of X, DOSEMU can be started by a symbolic
 *  link called `xdos` which DOSEMU will use to switch into X-mode.
 * DANG_END_REMARK
 */
#ifdef X_SUPPORT
  {  char *p;
     p = strrchr(argv[0],'/');    /* parse the program name */
     p = p ? p+1 : argv[0];
    
     if (strcmp(p,"xdos")==0)
        config.X=1;               /* activate X mode if dosemu was */ 
                                  /* called as 'xdos'              */
  }
#endif
     
  opterr = 0;
  confname = CONFIG_FILE;
  while ((c = getopt(argc, argv, "ABCcF:kM:D:P:VNtsgx:Km234e:dXY:Z:")) != EOF) {
	  switch (c) {
	  case 'F':
		  confname = optarg;
		  break;
	  case 'd':
		  if (config.detach)
			  break;
		  config.detach = (unsigned short)detach();
		  break;
	  case 'D':
	     parse_debugflags(optarg);
	     break;
	  }
  }

  parse_config(confname);

  if (config.exitearly)
    leavedos(0);

  optind = 0;
  opterr = 0;
  while ((c = getopt(argc, argv, "ABCcF:kM:D:P:VNtT:sgx:Km234e:dXY:Z:")) != EOF) {
    switch (c) {
    case 'F':			/* previously parsed config file argument */
    case 'd':
      break;
    case 'A':
      config.hdiskboot = 0;
      break;
    case 'B':
      config.hdiskboot = 2;
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
    case 'Y':
      open_Xkeyboard_pipe(optarg);
      config.cardtype = CARD_MDA;
      config.mapped_bios = 0;
      config.vbios_file = NULL;
      config.vbios_copy = 0;
      config.vbios_seg = 0xc000;
      config.console_video = 0;
      config.chipset = 0;
      config.fullrestore = 0;
      config.graphics = 0;
      config.vga = 0;  /* this flags BIOS graphics */
      config.usesX = 1;
      config.console_keyb = 1;
      break;
    case 'Z':
      open_Xmouse_pipe(optarg);
      config.usesX = 1;
      break;
    case 'K':
      warn("Keyboard interrupt enabled...this is still buggy!\n");
      config.keybint = 1;
      break;
    case 'M':{
	int max_mem = config.vga ? 640 : MAX_MEM_SIZE;

	config.mem_size = atoi(optarg);
	if (config.mem_size > max_mem)
	  config.mem_size = max_mem;
	break;
      }
    case 'D':
      parse_debugflags(optarg);
      break;
    case 'P':
      if (terminal_fd == -1)
	open_terminal_pipe(optarg);
      else
	error("ERROR: terminal pipe already open\n");
      break;
    case 'V':
      g_printf("Assuming VGA video card & mapped ROM\n");
      config.vga = 1;
      config.mapped_bios = 1;
      if (config.mem_size > 640)
	config.mem_size = 640;
      break;
    case 'N':
      warn("DOS will not be started\n");
      config.exitearly = 1;
      break;
    case 'T':
      g_printf("Using tmpdir=%s\n", optarg);
      free(tmpdir);
      tmpdir = strdup(optarg);
      break;
    case 't':
      g_printf("doing timer emulation\n");
      config.timers = 1;
      break;
    case 's':
      g_printf("using new scrn size code\n");
      sizes = 1;
      break;
    case 'g':
#ifdef EXPERIMENTAL_GFX
      g_printf("turning graphics option on\n");
      config.graphics = 1;
#else
      error("Graphics support not compiled in!\n");
#endif
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

    case '2':
      g_printf("CPU set to 286\n");
      vm86s.cpu_type = CPU_286;
      break;

    case '3':
      g_printf("CPU set to 386\n");
      vm86s.cpu_type = CPU_386;
      break;

    case '4':
      g_printf("CPU set to 486\n");
      vm86s.cpu_type = CPU_486;
      break;

    case '?':
    default:
      fprintf(stderr, "unrecognized option: -%c\n\r", c);
      usage();
      fflush(stdout);
      fflush(stderr);
      _exit(1);
    }
  }
  if (config.X) {
    config.console_video = config.vga = config.graphics = 0;
  }
}

/* 
 * DANG_BEGIN_FUNCTION parse_debugflags
 *
 * arguments:
 *  s - string of options.
 * 
 * description:
 *  This part is fairly flexible...you specify the debugging flags you wish
 * with -D string.  The string consists of the following characters:
 *   +   turns the following options on (initial state)
 *   -   turns the following options off
 *   a   turns all the options on/off, depending on whether +/- is set
 *   0-9 sets debug levels (0 is off, 9 is most verbose)
 *   #   where # is a letter from the valid option list (see docs), turns
 *       that option off/on depending on the +/- state.
 *
 * Any option letter can occur in any place.  Even meaningless combinations,
 * such as "01-a-1+0vk" will be parsed without error, so be careful.
 * Some options are set by default, some are clear. This is subject to my
 * whim.  You can ensure which are set by explicitly specifying.
 *
 * DANG_END_FUNCTION
 */
static void
parse_debugflags(const char *s)
{
  char c;
  unsigned char flag = 1;
  const char allopts[] = "vsdDRWkpiwghxmIEcXP";

  /* if you add new classes of debug messages, make sure to add the
	 * letter to the allopts string above so that "1" and "a" can work
	 * correctly.
	 */

  dbug_printf("debug flags: %s\n", s);
  while ((c = *(s++)))
    switch (c) {
    case '+':			/* begin options to turn on */
      if (!flag)
	flag = 1;
      break;
    case '-':			/* begin options to turn off */
      flag = 0;
      break;
    case 'v':			/* video */
      d.video = flag;
      break;
#ifdef X_SUPPORT
    case 'X':
      d.X = flag;
      break;
#endif
    case 's':			/* serial */
      d.serial = flag;
      break;
    case 'c':			/* disk */
      d.config = flag;
      break;
    case 'd':			/* disk */
      d.disk = flag;
      break;
    case 'R':			/* disk READ */
      d.read = flag;
      break;
    case 'W':			/* disk WRITE */
      d.write = flag;
      break;
    case 'k':			/* keyboard */
      d.keyb = flag;
      break;
    case 'p':			/* printer */
      d.printer = flag;
      break;
    case 'i':			/* i/o instructions (in/out) */
      d.io = flag;
      break;
    case 'w':			/* warnings */
      d.warning = flag;
      break;
    case 'g':			/* general messages */
      d.general = flag;
      break;
    case 'x':			/* XMS */
      d.xms = flag;
      break;
    case 'D':			/* DPMI */
      d.dpmi = flag;
      break;
    case 'm':			/* mouse */
      d.mouse = flag;
    case 'P':
      d.pd = flag;
      d.network=flag;
      break;
    case 'a':{			/* turn all on/off depending on flag */
	char *newopts = (char *) malloc(strlen(allopts) + 2);

	d.all = flag;
	newopts[0] = flag ? '+' : '-';
	newopts[1] = 0;
	strcat(newopts, allopts);
	parse_debugflags(newopts);
	free(newopts);
	break;
      }
    case 'h':			/* hardware */
      d.hardware = flag;
      break;
    case 'I':			/* IPC */
      d.IPC = flag;
      break;
    case 'E':			/* EMS */
      d.EMS = flag;
      break;
    case '0'...'9':		/* set debug level, 0 is off, 9 is most verbose */
      flag = c - '0';
      break;
    default:
      fprintf(stderr, "Unknown debug-msg mask: %c\n\r", c);
      dbug_printf("Unknown debug-msg mask: %c\n", c);
    }
}

static void
 usage(void) {
  fprintf(stdout, "$Header: /home/src/dosemu0.60/init/RCS/config.c,v 1.2 1995/02/05 16:53:16 root Exp root $\n");
  fprintf(stdout, "usage: dos [-ABCckbVNtsgxKm234e] [-D flags] [-M SIZE] [-P FILE] [ -F File ] 2> dosdbg\n");
  fprintf(stdout, "    -A boot from first defined floppy disk (A)\n");
  fprintf(stdout, "    -B boot from second defined floppy disk (B) (#)\n");
  fprintf(stdout, "    -C boot from first defined hard disk (C)\n");
  fprintf(stdout, "    -c use PC console video (kernel 0.99pl3+) (!%%)\n");
  fprintf(stdout, "    -k use PC console keyboard (kernel 0.99pl3+) (!)\n");
#ifdef X_SUPPORT
  fprintf(stdout, "    -X run in X Window (#)\n");
#endif
  fprintf(stdout, "    -X NAME use MDA direct and FIFO NAME for keyboard (only with x2dos!\n");
  fprintf(stdout, "    -Y NAME use FIFO NAME for mouse (only with x2dos!\n");
  fprintf(stdout, "    -D set debug-msg mask to flags (+-)(vsdDRWkpiwghxmIEc01)\n");
  fprintf(stdout, "    -M set memory size to SIZE kilobytes (!)\n");
  fprintf(stdout, "    -P copy debugging output to FILE\n");
  fprintf(stdout, "    -b map BIOS into emulator RAM (%%)\n");
  fprintf(stdout, "    -F use config-file File\n");
  fprintf(stdout, "    -V use BIOS-VGA video modes (!#%%)\n");
  fprintf(stdout, "    -N No boot of DOS\n");
  fprintf(stdout, "    -t try new timer code (#)\n");
  fprintf(stdout, "    -s try new screen size code (COMPLETELY BROKEN)(#)\n");
  fprintf(stdout, "    -g enable graphics modes (COMPLETELY BROKEN) (!%%#)\n");
  fprintf(stdout, "    -x SIZE enable SIZE K XMS RAM\n");
  fprintf(stdout, "    -e SIZE enable SIZE K EMS RAM\n");
  fprintf(stdout, "    -m enable mouse support (!#)\n");
  fprintf(stdout, "    -2,3,4 choose 286, 386 or 486 CPU\n");
  fprintf(stdout, "    -K Do int9 (!#)\n");
  fprintf(stdout, "\n     (!) means BE CAREFUL! READ THE DOCS FIRST!\n");
  fprintf(stdout, "     (%%) marks those options which require dos be run as root (i.e. suid)\n");
  fprintf(stdout, "     (#) marks options which do not fully work yet\n");
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
  struct new_utsname unames;
  uname(&unames);
  fprintf(stderr, "DOSEMU%spl%s is coming up on %s version %s\n", VERSTR, PATCHSTR, unames.sysname, unames.release);  
  fprintf(stderr, "Built for %d\n", KERNEL_VERSION);
  if (unames.release[0] > 0 ) {
    if ((unames.release[2] == 1  && unames.release[3] > 1 ) ||
         unames.release[2] > 1 ) {
      use_sigio=FASYNC;
    }
  }
  /* Next Check input */
  if (isatty(STDIN_FILENO)) {
    k_printf("STDIN is tty\n");
    config.kbd_tty = 0;
  } else {
    k_printf("STDIN not a tty\n");
    config.kbd_tty = 1;
  }
}


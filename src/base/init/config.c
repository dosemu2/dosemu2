#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "config.h"
#include "emu.h"
#include "timers.h"
#include "video.h"
#include "mouse.h"
#include "serial.h"
#include "keymaps.h"
#include "memory.h"
#include "bios.h"
#include "kversion.h"

#include "dos2linux.h"
#include "priv.h"
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif

/*
 * XXX - the mem size of 734 is much more dangerous than 704. 704 is the
 * bottom of 0xb0000 memory.  use that instead?
 */
#define MAX_MEM_SIZE    640


#if 0  /* initialized in data.c (via emu.h) */
struct debug_flags d =
{  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
/* d  R  W  D  C  v  X  k  i  T  s  m  #  p  g  c  w  h  I  E  x  M  n  P  r  S */
#endif

static void     check_for_env_autoexec_or_config(void);
int     parse_debugflags(const char *s, unsigned char flag);
static void     usage(void);

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

/*
 * DANG_BEGIN_FUNCTION config_defaults
 * 
 * description: 
 * Set all values in the `config` structure to their default
 * value. These will be modified by the config parser.
 * 
 * DANG_END_FUNCTION
 * 
 */
static void
config_defaults(void)
{
    FILE *fs;
    char Line[256];
    int k = 386;

    vm86s.cpu_type = CPU_386;
    /* defaults - used when /proc is missing, cpu!=x86 etc. */
    config.realcpu = CPU_386;
    config.pci = 0;
    config.rdtsc = 0;
    config.mathco = 0;
    config.smp = 0;
#ifdef X86_EMULATOR
    config.emuspeed = 50;	/* instruction cycles per us */
#endif

    fs=fopen("/proc/cpuinfo","r");
    if (fs) {
	while (fgets(Line,250,fs)) {
	  /* get the CPU (processor#0) info */
	  if (!strncmp(Line,"cpu",3) && isspace(Line[3])) {
	    char *p = Line+4;
	    while (*p && (*p!=':')) p++;

	    /* if it's an x86, check which one */
	    if ((sscanf(p+1,"%d",&k)==1) && ((k%100)==86)) {
	      switch ((k/100)%10) {
	        case 3: config.realcpu = CPU_386;	/* redundant */
	        	break;
	        case 5:
	        case 6: config.realcpu = CPU_586;
			/* bogospeed currently returns 0; should it deny
			 * pentium features, fall back into 486 case */
			if (!bogospeed(&config.cpu_spd, &config.cpu_tick_spd)) {
			    config.pci = 1;	/* fair guess */
			    config.rdtsc = 1;
			    break;
			}
			k = 486;
			/* fall thru */
	        case 4: config.realcpu = CPU_486;
	        	break;
	        default:
	        	exit(1);	/* no 186,286,786.. */
	      }
	    }
	  }
	  else if (!strncmp(Line,"fpu",3) && isspace(Line[3])) {
	    /* get the fpu info. this is currently unused. */
	    char *p = Line+4;
	    while (*p && (*p!=':')) p++;
	    if (*p) {
	      p++; while (*p && isspace(*p)) p++;
	      config.mathco = (*p=='y');
	    }
	  }
	  else if (!strncmp(Line,"processor",9)) {
	    /* if a 'processor:n' statement is found with n>0, meaning we
	     * are on a SMP machine, deny use of pentium timer, force
	     * use of kernel timing and stop parsing the file.
	     */
	    char *p = Line+9;
	    while (*p && (*p!=':')) p++;
	    if (*p) {
	      p++; while (*p && isspace(*p)) p++;
	      if (isdigit(*p) && (*p>'0') && config.rdtsc) {
	  	fprintf(stderr,"Denying use of pentium timer on SMP machine\n");
		config.smp = 1;		/* for checking overrides, later */
	      	config.rdtsc = 0;
	      	break;
	      }
	    }
	  }
	}
	fclose(fs);
    }
    fprintf(stderr,"Running on CPU=%d86, FPU=%d\n",config.realcpu,config.mathco);

    config.hdiskboot = 1;	/* default hard disk boot */
#ifdef X86_EMULATOR
    config.cpuemu = 0;
#endif
    config.mem_size = 640;
    config.ems_size = 0;
    config.ems_frame = 0xd000;
    config.xms_size = 0;
    config.max_umb = 0;
    config.dpmi = 0;
    config.secure = 1;  /* need to have it 'on', else user may trick it out
                           via -F option */
    config.mouse_flag = 0;
    config.mapped_bios = 0;
    config.mapped_sbios = 0;
    config.vbios_file = NULL;
    config.vbios_copy = 0;
    config.vbios_seg = 0xc000;
    config.vbios_size = 0x10000;
    config.console = 0;
    config.console_keyb = 0;
    config.console_video = 0;
    config.kbd_tty = 0;
    config.fdisks = 0;
    config.hdisks = 0;
    config.bootdisk = 0;
    config.exitearly = 0;
    config.term_esc_char = 30;	       /* Ctrl-^ */
    /* config.term_method = METHOD_FAST; */
    config.term_color = 1;
    /* config.term_updatelines = 25; */
    config.term_updatefreq = 4;
    config.term_charset = CHARSET_LATIN;
    /* config.term_corner = 1; */
    config.X_updatelines = 25;
    config.X_updatefreq = 8;
    config.X_display = NULL;	/* NULL means use DISPLAY variable */
    config.X_title = "dosemu";
    config.X_icon_name = "dosemu";
    config.X_blinkrate = 8;
    config.X_sharecmap = 0;     /* Don't share colourmap in graphics modes */
    config.X_mitshm = 0;
    config.X_fixed_aspect = 1;
    config.X_aspect_43 = 0;
    config.X_lin_filt = 0;
    config.X_bilin_filt = 0;
    config.X_mode13fact = 2;
    config.X_winsize_x = 0;
    config.X_winsize_y = 0;
    config.X_gamma = 100;
    config.vgaemu_memsize = 0;
    config.vesamode_list = NULL;
    config.X_lfb = 1;
    config.X_pm_interface = 1;
    config.X_keycode = 0;
    config.X_font = "vga";
    config.usesX = 0;
    config.X = 0;
    config.hogthreshold = 10;	/* bad estimate of a good garrot value */
    config.chipset = PLAINVGA;
    config.cardtype = CARD_VGA;
    config.pci_video = 0;
    config.fullrestore = 0;
    config.graphics = 0;
    config.gfxmemsize = 256;
    config.vga = 0;		/* this flags BIOS graphics */
    config.dualmon = 0;
    config.force_vt_switch = 0;
    config.speaker = SPKR_EMULATED;

    /* The frequency in usec of the SIGALRM call (in signal.c) is
     * equal to this value / 6, and thus is currently 9158us = 100 Hz
     * The 6 (TIMER DIVISOR) is a constant of unknown origin
     * NOTE: if you set 'timer 18' in config.dist you can't get anything
     * better that 55555 (108Hz) because of integer math.
     * see timer_interrupt_init() in init.c
     */
    config.update = 54925;	/* should be = 1E6/config.freq */
    config.freq = 18;		/* rough frequency (real PC = 18.2065) */
    config.wantdelta = 9154;	/* requested value for setitimer */
    config.realdelta = 9154;

    config.timers = 1;		/* deliver timer ints */
    config.keybint = 1;		/* keyboard interrupts */
 
    /* Lock file stuff */
    config.tty_lockdir = PATH_LOCKD;    /* The Lock directory  */
    config.tty_lockfile = NAME_LOCKF;   /* Lock file pretext ie LCK.. */
    config.tty_lockbinary = FALSE;      /* Binary lock files ? */

    config.num_ser = 0;
    config.num_lpt = 0;
    config.fastfloppy = 1;

    config.emusys = (char *) NULL;
    config.emubat = (char *) NULL;
    config.emuini = (char *) NULL;
    tmpdir = strdup(tempnam("/tmp", "dosemu"));
    config.dosbanner = 1;
    config.allowvideoportaccess = 0;

    config.keyboard = KEYB_US;	/* What's the current keyboard  */
    config.key_map = key_map_us;/* pointer to the keyboard-maps */
    config.shift_map = shift_map_us;	/* Here the Shilt-map           */
    config.alt_map = alt_map_us;/* And the Alt-map              */
    config.num_table = num_table_dot;	/* Numeric keypad has a dot     */
    config.detach = 0;		/* Don't detach from current tty and open
				 * new VT. */
    config.debugout = NULL;	/* default to no debug output file */

    config.pre_stroke =NULL;	/* defualt no keyboard pre-strokes */

    config.sillyint = 0;
    config.must_spare_hardware_ram = 0;
    memset(config.hardware_pages, 0, sizeof(config.hardware_pages));

    mice->fd = -1;
    mice->add_to_io_select = 0;
    mice->type = 0;
    mice->flags = 0;
    mice->intdrv = 0;
    mice->cleardtr = 0;
    mice->baudRate = 0;
    mice->sampleRate = 0;
    mice->lastButtons = 0;
    mice->chordMiddle = 0;

    config.sb_base = 0x220;
    config.sb_dma = 1;
    config.sb_irq = 5;
#ifdef __NetBSD__
    config.sb_dsp = "/dev/sound";
    config.sb_mixer = "/dev/mixer";
#else
    config.sb_dsp = "/dev/dsp";
    config.sb_mixer = "/dev/mixer";
#endif /* !__NetBSD__ */
    config.mpu401_base = 0x330;
}

static void 
open_terminal_pipe(char *path)
{
    enter_priv_off();
    terminal_fd = DOS_SYSCALL(open(path, O_RDWR));
    leave_priv_setting();
    if (terminal_fd == -1) {
	terminal_pipe = 0;
	error("open_terminal_pipe failed - cannot open %s!\n", path);
	return;
    } else
	terminal_pipe = 1;
}

static void 
open_Xkeyboard_pipe(char *path)
{
    keypipe = DOS_SYSCALL(open(path, O_RDWR));
    if (keypipe == -1) {
	keypipe = 0;
	error("open_Xkeyboard_pipe failed - cannot open %s!\n", path);
	return;
    }
    return;
}

static void 
open_Xmouse_pipe(char *path)
{
    mousepipe = DOS_SYSCALL(open(path, O_RDWR));
    if (mousepipe == -1) {
	mousepipe = 0;
	error("open_Xmouse_pipe failed - cannot open %s!\n", path);
	return;
    }
    return;
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
    extern char *commandline_statements;
    extern int dexe_running;
    int             c;
    char           *confname = NULL;
    char           *basename;
    char           *dexe_name = 0;

    if (argv[1] && !strcmp("--version",argv[1])) {
      usage();
      exit(0);
    }

    config_defaults();
    basename = strrchr(argv[0], '/');	/* parse the program name */
    basename = basename ? basename + 1 : argv[0];

#ifdef X_SUPPORT
    /*
     * DANG_BEGIN_REMARK For simpler support of X, DOSEMU can be started
     * by a symbolic link called `xdos` which DOSEMU will use to switch
     * into X-mode. DANG_END_REMARK
     */
    {
	if (strcmp(basename, "xdos") == 0)
	    config.X = 1;	/* activate X mode if dosemu was */
	/* called as 'xdos'              */
    }
#endif

    opterr = 0;
    confname = CONFIG_FILE;
    while ((c = getopt(argc, argv, "ABCcF:I:kM:D:P:VNtsgx:KL:m23456e:E:dXY:Z:o:Ou:")) != EOF) {
	switch (c) {
	case 'F':
	    if (get_orig_uid()) {
		FILE *f;
		enter_priv_off();
		f=fopen(optarg, "r");
		leave_priv_setting();
		if (!f) {
		  fprintf(stderr, "Sorry, no access to configuration file %s\n", optarg);
		  exit(1);
		}
		fclose(f);
	    }
	    confname = optarg;	/* someone reassure me that this is *safe*? */
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
	    enter_priv_off();
	    dbg_fd = fopen(config.debugout, "w");
	    leave_priv_setting();
	    if (!dbg_fd) {
		fprintf(stderr, "can't open \"%s\" for writing\n", config.debugout);
		exit(1);
	    }
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
		extern int define_config_variable(char *name);
		char *s=malloc(strlen(optarg)+3);
		s[0]='u'; s[1]='_';
		strcpy(s+2,optarg);
		define_config_variable(s);
	    }
	    break;
	}
    }

    if (dexe_name || !strcmp(basename,"dosexec")) {
	extern void prepare_dexe_load(char *name);
	if (!dexe_name) dexe_name = argv[optind];
	if (!dexe_name) {
	  usage();
	  exit(1);
	}
	prepare_dexe_load(dexe_name);
    }

#if defined(__NetBSD__) && defined(X_SUPPORT) && defined(X_GRAPHICS)
    { extern int selfmem_fd;
    /* do this before any set*id functions are called */
    selfmem_fd = open("/proc/curproc/mem", O_RDWR);
    }
#endif

    parse_config(confname);

    if (config.exitearly)
	leavedos(0);

    if (vm86s.cpu_type > config.realcpu) {
    	vm86s.cpu_type = config.realcpu;
    	fprintf(stderr, "CONF: emulated CPU forced down to real CPU: %ld86\n",vm86s.cpu_type);
    }

#ifdef __NetBSD__
    optreset = 1;
    optind = 1;
#endif
#ifdef __linux__
    optind = 0;
#endif
    opterr = 0;
    while ((c = getopt(argc, argv, "ABCcF:I:kM:D:P:v:VNtT:sgx:KLm23456e:dXY:Z:E:o:Ou:")) != EOF) {
	switch (c) {
	case 'F':		/* previously parsed config file argument */
	case 'I':
	case 'd':
	case 'o':
	case 'O':
	case 'L':
	case 'u':
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
	case 'Y':
#ifdef X_SUPPORT
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
	    config.vga = 0;	/* this flags BIOS graphics */
	    config.usesX = 1;
	    config.console_keyb = 1;
#endif
	    break;
	case 'Z':
#ifdef X_SUPPORT
	    open_Xmouse_pipe(optarg);
	    config.usesX = 1;
#endif
	    break;
	case 'K':
	    warn("Keyboard interrupt enabled...this is still buggy!\n");
	    config.keybint = 1;
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
	    g_printf("turning graphics option on\n");
	    config.graphics = 1;
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
	    misc_e6_store_command(optarg);
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
#ifndef NEW_KBD_CODE
	if (!config.X_keycode) {
	    extern void keyb_layout(int layout);
	    keyb_layout(-1);
	    c_printf("CONF: Forceing neutral Keyboard-layout, X-server will translate\n");
	}
#endif /* NEW_KBD_CODE */
	config.console_video = config.vga = config.graphics = 0;
    }
    check_for_env_autoexec_or_config();
    if (under_root_login)  c_printf("CONF: running exclusively as ROOT:");
    else {
#ifdef RUN_AS_ROOT
      c_printf("CONF: mostly running as ROOT:");
#else
      c_printf("CONF: mostly running as USER:");
#endif
    }
    c_printf(" uid=%d (cached %d) gid=%d (cached %d)\n",
        geteuid(), get_cur_euid(), getegid(), get_cur_egid());
}


static void 
check_for_env_autoexec_or_config(void)
{
    char           *cp;
    cp = getenv("AUTOEXEC");
    if (cp)
	config.emubat = cp;
    cp = getenv("CONFIG");
    if (cp)
	config.emusys = cp;


     /*
      * The below is already reported in the conf. It is in no way an error
      * so why the messages?
      */

#if 0
    if (config.emubat)
	fprintf(stderr, "autoexec extension = %s\n", config.emubat);
    if (config.emusys)
	fprintf(stderr, "config extension = %s\n", config.emusys);
#endif
}

/*
 * DANG_BEGIN_FUNCTION parse_debugflags
 * 
 * arguments: 
 * s - string of options.
 * 
 * description: 
 * This part is fairly flexible...you specify the debugging
 * flags you wish with -D string.  The string consists of the following
 * characters: +   turns the following options on (initial state) -
 * turns the following options off a   turns all the options on/off,
 * depending on whether +/- is set 0-9 sets debug levels (0 is off, 9 is
 * most verbose) #   where # is a letter from the valid option list (see
 * docs), turns that option off/on depending on the +/- state.
 * 
 * Any option letter can occur in any place.  Even meaningless combinations,
 * such as "01-a-1+0vk" will be parsed without error, so be careful. Some
 * options are set by default, some are clear. This is subject to my whim.
 * You can ensure which are set by explicitly specifying.
 * 
 * DANG_END_FUNCTION
 */
int parse_debugflags(const char *s, unsigned char flag)
{
    char            c;

#ifdef X_SUPPORT
    const char      allopts[] = "dRWDCvXkiTsm#pgcwhIExMnPrSe";
#else
    const char      allopts[] = "dRWDCvkiTsm#pgcwhIExMnPrSe";
#endif

    /*
     * if you add new classes of debug messages, make sure to add the
     * letter to the allopts string above so that "1" and "a" can work
     * correctly.
     */

    dbug_printf("debug flags: %s\n", s);
    while ((c = *(s++)))
	switch (c) {
	case '+':		/* begin options to turn on */
	    if (!flag)
		flag = 1;
	    break;
	case '-':		/* begin options to turn off */
	    flag = 0;
	    break;

	case 'd':		/* disk */
	    d.disk = flag;
	    break;
	case 'R':		/* disk READ */
	    d.read = flag;
	    break;
	case 'W':		/* disk WRITE */
	    d.write = flag;
	    break;
	case 'D':		/* DOS int 21h */
	    d.dos = flag;
	    break;
        case 'C':               /* CDROM */
	    d.cdrom = flag;
            break;
	case 'v':		/* video */
	    d.video = flag;
	    break;
#ifdef X_SUPPORT
	case 'X':
	    d.X = flag;
	    break;
#endif
	case 'k':		/* keyboard */
	    d.keyb = flag;
	    break;
	case 'i':		/* i/o instructions (in/out) */
	    d.io = flag;
	    break;
	case 'T':		/* i/o port tracing */
	    d.io_trace = flag;
	    break;
	case 's':		/* serial */
	    d.serial = flag;
	    break;
	case 'm':		/* mouse */
	    d.mouse = flag;
	    break;
	case '#':		/* default int */
	    d.defint =flag;
	    break;
	case 'p':		/* printer */
	    d.printer = flag;
	    break;
	case 'g':		/* general messages */
	    d.general = flag;
	    break;
	case 'c':		/* configuration */
	    d.config = flag;
	    break;
	case 'w':		/* warnings */
	    d.warning = flag;
	    break;
	case 'h':		/* hardware */
	    d.hardware = flag;
	    break;
	case 'I':		/* IPC */
	    d.IPC = flag;
	    break;
	case 'E':		/* EMS */
	    d.EMS = flag;
	    break;
	case 'x':		/* XMS */
	    d.xms = flag;
	    break;
	case 'M':		/* DPMI */
	    d.dpmi = flag;
	    break;
	case 'n':		/* IPX network */
	    d.network = flag;
	    break;
	case 'P':		/* Packet driver */
	    d.pd = flag;
	    break;
	case 'r':		/* PIC */
	    d.request = flag;
	    break;
	case 'S':		/* SOUND */
	    d.sound = flag;
	    break;
#ifdef X86_EMULATOR
	case 'e':		/* cpu-emu */
	    d.emu = flag;
	    break;
#endif
	case 'a':{		/* turn all on/off depending on flag */
		char           *newopts = (char *) malloc(strlen(allopts) + 2);

		newopts[0] = flag ? '+' : '-';
		newopts[1] = 0;
		strcat(newopts, allopts);
#ifdef X86_EMULATOR
		/* hack-do not set 'e' flag if not explicitly specified */
		{char *p=newopts; while (*p) {if (*p=='e') *p='g'; p++;}}
#endif
		parse_debugflags(newopts, flag);
		free(newopts);
		break;
	    }
	case '0'...'9':	/* set debug level, 0 is off, 9 is most
				 * verbose */
	    flag = c - '0';
	    break;
	default:
	    fprintf(stderr, "Unknown debug-msg mask: %c\n\r", c);
	    dbug_printf("Unknown debug-msg mask: %c\n", c);
	    return 1;
	}
  return 0;
}

static void
usage(void)
{
    fprintf(stdout,
	"dosemu-" VERSTR "\n\n"
	"USAGE:\n"
	"  dos  [-ABCckbVNtsgxKm23456ez] \\\n"
	"       [-D flags] [-M SIZE] [-P FILE] [ {-F|-L} File ] \\\n"
	"       [-o dbgfile] 2> vital_logs\n"
	"  dos --version\n\n"
	"    -2,3,4,5,6 choose 286, 386, 486 or 586 or 686 CPU\n"
	"    -A boot from first defined floppy disk (A)\n"
	"    -B boot from second defined floppy disk (B) (#)\n"
	"    -b map BIOS into emulator RAM (%%)\n"
	"    -C boot from first defined hard disk (C)\n"
	"    -c use PC console video (!%%)\n"
	"    -d detach (?)\n"
#ifdef X_SUPPORT
	"    -X run in X Window (#)\n"
/* seems no longer valid bo 18.7.95
	"    -Y NAME use MDA direct and FIFO NAME for keyboard (only with x2dos!)\n"
	"    -Z NAME use FIFO NAME for mouse (only with x2dos!)\n"
*/
	"    -D set debug-msg mask to flags {+-}{0-9}{#CDEIMPRSWXcdghikmnprsvwx}\n"
#else				/* X_SUPPORT */
	"    -D set debug-msg mask to flags {+-}{0-9}{#CDEIMPRSWcdghikmnprsvwx}\n"
#endif				/* X_SUPPORT */
	"       #=defint  C=cdrom    D=dos    E=ems       I=ipc     M=dpmi\n"
	"       P=packet  R=diskread S=sound  W=diskwrite c=config  d=disk\n"
	"       g=general h=hardware i=i/o    k=keyb      m=mouse   n=ipxnet\n"
	"       p=printer r=pic      s=serial v=video     w=warning x=xms\n"
	"    -E STRING pass DOS command on command line\n"
	"    -e SIZE enable SIZE K EMS RAM\n"
	"    -F use config-file File\n"
	"    -L load and execute DEXE File\n"
	"    -I insert config statements (on commandline)\n"
	"    -g enable graphics modes (!%%#)\n"
	"    -K Do int9 (!#)\n"
	"    -k use PC console keyboard (!)\n"
	"    -M set memory size to SIZE kilobytes (!)\n"
	"    -m enable mouse support (!#)\n"
	"    -N No boot of DOS\n"
	"    -O write debugmessages to stderr\n"
	"    -o FILE put debugmessages in file\n"
	"    -P copy debugging output to FILE\n"
	"    -T DIR set tmpdir\n"
	"    -t try new timer code (#)\n"
	"    -V use BIOS-VGA video modes (!#%%)\n"
	"    -v NUM force video card type\n"
	"    -x SIZE enable SIZE K XMS RAM\n"
	"    --version, print version of dosemu\n"
	"    (!) BE CAREFUL! READ THE DOCS FIRST!\n"
	"    (%%) require dos be run as root (i.e. suid)\n"
	"    (#) options do not fully work yet\n\n"
	"xdos [options]           == dos [options] -X\n"
	"dosexec [options] <file> == dos [options] -L <file>\n"
    );
}

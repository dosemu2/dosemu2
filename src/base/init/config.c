#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#ifdef HAVE_LIBBSD
#include <bsd/string.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <sys/utsname.h>
#include <sys/stat.h>

#include "version.h"
#include "emu.h"
#include "timers.h"
#include "video.h"
#include "vc.h"
#include "mouse.h"
#include "serial.h"
#include "keyboard/keymaps.h"
#include "memory.h"
#include "bios.h"
#include "lpt.h"
#include "int.h"
#include "dosemu_config.h"
#include "init.h"
#include "disks.h"
#include "pktdrvr.h"
#include "speaker.h"
#include "sound/sound.h"
#include "keyboard/keyb_clients.h"
#include "translate/dosemu_charset.h"
#include "dos2linux.h"
#include "utilities.h"
#include "redirect.h"
#include "shlock.h"
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif
#include "mhpdbg.h"
#include "mapping.h"

/*
 * Options used in config_init().
 *
 * Please keep "getopt_string", secure_option_preparse(), config_init(),
 * usage() and the manpage in sync!
 *
 * No more undocumented switches *please*.
 *
 * "--Fusers", "--Flibdir", "--Fimagedir" and "-n" not in "getopt_string"
 * they are eaten by secure_option_preparse().
 */


int config_check_only = 0;

int dosemu_argc;
char **dosemu_argv;
char *dosemu_proc_self_exe = NULL;
int dosemu_proc_self_maps_fd = -1;

static void usage(char *basename);
static void assign_floppy(int fnum, const char *name);

const char *config_script_name = DEFAULT_CONFIG_SCRIPT;
const char *dosemu_loglevel_file_path = "/etc/" DOSEMU_LOGLEVEL;
char *dosemu_rundir_path;
char *dosemu_localdir_path;
char *dosemu_tmpdir;

char *dosemu_lib_dir_path;
const char *dosemu_exec_dir_path = DOSEMUEXEC_DEFAULT;
char *dosemu_plugin_dir_path;
char *commands_path;
char *dosemu_image_dir_path;
char *dosemu_drive_c_path;
char keymaploadbase_default[] = PREFIX "/share/";
char *keymap_load_base_path = keymaploadbase_default;
const char *keymap_dir_path = "keymap/";
const char *owner_tty_locks = "uucp";
const char *tty_locks_dir_path = "/var/lock";
const char *tty_locks_name_path = "LCK..";
const char *dosemu_midi_path;
const char *dosemu_midi_in_path;
char *dosemu_map_file_name;
char *fddir_default;
char *comcom_dir;
char *fddir_boot;
char *xbat_dir;
struct config_info config;

#define STRING_STORE_SIZE 10
struct cfg_string_store {
    struct string_store base;
    char *strings[STRING_STORE_SIZE];
} cfg_store = { { .num = STRING_STORE_SIZE } };

/*
 * DANG_BEGIN_FUNCTION cpu_override
 *
 * description:
 * Process user CPU override from the config file ('cpu xxx') or
 * from the command line. Returns the selected CPU identifier or
 * -1 on error.
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
	    return CPU_586;
	/* fall through */
	case 4:
	    return CPU_486;
	/* fall through */
	case 3:
	    return CPU_386;
    }
    return -1;
}

static void dump_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

static void log_dump_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vlog_printf(fmt, args);
    va_end(args);
}

void dump_config_status(void (*printfunc)(const char *, ...))
{
    const char *s;
    void (*print)(const char *, ...) = (printfunc ? printfunc : dump_printf);

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
    (*print)("CPUclock %g MHz\ncpu_spd 0x%lx\ncpu_tick_spd 0x%lx\n",
	(((double)LLF_US)/config.cpu_spd), config.cpu_spd, config.cpu_tick_spd);

    (*print)("pci %d\nmathco %d\nsmp %d\n",
                 config.pci, config.mathco, config.smp);
    (*print)("cpuspeed %d\n", config.CPUSpeedInMhz);

    if (config_check_only) mapping_init();
    (*print)("mappingdriver %s\n", config.mappingdriver ? config.mappingdriver : "auto");
    if (config_check_only) mapping_close();
    (*print)("hdiskboot %d\n",
        config.hdiskboot);
    (*print)("mem_size %d\next_mem %d\n",
	config.mem_size, config.ext_mem);
    (*print)("ems_size 0x%x\nems_frame 0x%x\n",
        config.ems_size, config.ems_frame);
    (*print)("umb_a0 %i\numb_b0 %i\numb_f0 %i\ndos_up %i\n",
        config.umb_a0, config.umb_b0, config.umb_f0, config.dos_up);
    (*print)("dpmi 0x%x\ndpmi_base 0x%x\npm_dos_api %i\nignore_djgpp_null_derefs %i\n",
        config.dpmi, config.dpmi_base, config.pm_dos_api, config.no_null_checks);
    (*print)("mapped_bios %d\nvbios_file %s\n",
        config.mapped_bios, (config.vbios_file ? config.vbios_file :""));
    (*print)("vbios_copy %d\nvbios_seg 0x%x\nvbios_size 0x%x\n",
        config.vbios_copy, config.vbios_seg, config.vbios_size);
    (*print)("console_keyb %d\nconsole_video %d\n",
        config.console_keyb, config.console_video);
    (*print)("kbd_tty %d\nexitearly %d\n",
        config.kbd_tty, config.exitearly);
    (*print)("fdisks %d\nhdisks %d\n",
        config.fdisks, config.hdisks);
    (*print)("term_esc_char 0x%x\nterm_color %d\n",
        config.term_esc_char, config.term_color);
    (*print)("xterm_title\n", config.xterm_title);
    (*print)("X_display \"%s\"\nX_title \"%s\"\nX_icon_name \"%s\"\n",
        (config.X_display ? config.X_display :""), config.X_title, config.X_icon_name);
    (*print)("X_title_show_appname %d\n",
	config.X_title_show_appname);
    (*print)("X_blinkrate %d\nX_sharecmap %d\nX_mitshm %d\n",
        config.X_blinkrate, config.X_sharecmap, config.X_mitshm);
    (*print)("X_fixed_aspect %d\nX_aspect_43 %d\nX_lin_filt %d\n",
        config.X_fixed_aspect, config.X_aspect_43, config.X_lin_filt);
    (*print)("X_bilin_filt %d\nX_mode13fact %d\nX_winsize_x %d\n",
        config.X_bilin_filt, config.X_mode13fact, config.X_winsize_x);
    (*print)("X_winsize_y %d\nX_gamma %d\nX_fullscreen %d\nvgaemu_memsize 0x%x\n",
        config.X_winsize_y, config.X_gamma, config.X_fullscreen,
	     config.vgaemu_memsize);
    (*print)("SDL_hwrend %d\nSDL_fonts \"%s\"\n",
        config.sdl_hwrend, config.sdl_fonts);
    (*print)("SDL_clip_native %d\n",
        config.sdl_clip_native);
    (*print)("vesamode_list %p\nX_lfb %d\nX_pm_interface %d\n",
        config.vesamode_list, config.X_lfb, config.X_pm_interface);
    (*print)("X_font \"%s\"\n", config.X_font);
    (*print)("vga_fonts %i\n", config.vga_fonts);
    (*print)("X_mgrab_key \"%s\"\n",  config.X_mgrab_key);
    (*print)("X_background_pause %d\n", config.X_background_pause);
    (*print)("X_noclose %d\n", config.X_noclose);

    switch (config.chipset) {
      case PLAINVGA: s = "plainvga"; break;
#ifdef USE_SVGALIB
      case SVGALIB: s = "svgalib"; break;
#endif
      case VESA: s = "vesa"; break;
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
    (*print)("num_ser %d\nnum_lpt %d\nfastfloppy %d\nfile_lock_limit %d\n",
        config.num_ser, config.num_lpt, config.fastfloppy, config.file_lock_limit);
    (*print)("emusys \"%s\"\n",
        (config.emusys ? config.emusys : ""));
    (*print)("vbios_post %d\ndetach %d\n",
        config.vbios_post, config.detach);
    (*print)("debugout \"%s\"\n",
        (config.debugout ? config.debugout : ""));
    {
	char buf[256];
	GetDebugFlagsHelper(buf, 0);
	(*print)("debug_flags \"%s\"\n", buf);
    }
    if (!printfunc && config.keytable) {
        dump_keytable(stderr, config.keytable);
    } else {
        (*print) ("keytable not setup yet\n");
    }
    (*print)("pre_stroke \"%s\"\n", (config.pre_stroke ? config.pre_stroke : ""));
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
    list_hardware_ram(print);
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

    (*print)("\nSOUND:\nengine %d\nsb_base 0x%x\nsb_dma %d\nsb_hdma %d\nsb_irq %d\n"
	"mpu401_base 0x%x\nmpu401_irq %i\nsound_driver \"%s\"\n",
        config.sound, config.sb_base, config.sb_dma, config.sb_hdma, config.sb_irq,
	config.mpu401_base, config.mpu401_irq, config.sound_driver);
    (*print)("pcm_hpf %i\nmidi_file %s\nwav_file %s\n",
	config.pcm_hpf, config.midi_file, config.wav_file);
    (*print)("\ncli_timeout %d\n", config.cli_timeout);
    (*print)("\ntimer_tweaks %d\n", config.timer_tweaks);
    (*print)("\nJOYSTICK:\njoy_device0 \"%s\"\njoy_device1 \"%s\"\njoy_dos_min %i\njoy_dos_max %i\njoy_granularity %i\njoy_latency %i\n",
        config.joy_device[0], config.joy_device[1], config.joy_dos_min, config.joy_dos_max, config.joy_granularity, config.joy_latency);
    (*print)("\nFS:\nset_int_hooks %i\nforce_int_revect %i\nforce_fs_redirect %i\n\n",
        config.int_hooks, config.force_revect, config.force_redir);

    (*print)("\nMMIO:\nmmio_tracing %i\n\n", config.mmio_tracing);

    if (!printfunc) {
      (*print)("\n--------------end of runtime configuration dump -------------\n");
      (*print)(  "-------------------------------------------------------------\n\n");
    }
}

static void
open_terminal_pipe(char *path)
{
    terminal_fd = DOS_SYSCALL(open(path, O_RDWR | O_CLOEXEC));
    if (terminal_fd == -1) {
	terminal_pipe = 0;
	error("open_terminal_pipe failed - cannot open %s!\n", path);
	return;
    } else
	terminal_pipe = 1;
}

static void our_envs_init(void)
{
    struct utsname unames;
    char *s;
    char buf[256];

    strcpy(buf, on_console() ? "1" : "0");
    setenv("DOSEMU_STDIN_IS_CONSOLE", buf, 1);
    uname(&unames);
    kernel_version_code = strtol(unames.release, &s,0) << 16;
    kernel_version_code += strtol(s+1, &s,0) << 8;
    kernel_version_code += strtol(s+1, &s,0);
    sprintf(buf, "%d", kernel_version_code);
    setenv("KERNEL_VERSION_CODE", buf, 1);
    sprintf(buf, "%d", DOSEMU_VERSION_CODE);
    setenv("DOSEMU_VERSION_CODE", buf, 1);
    setenv("DOSEMU_VERSION", VERSTR, 1);
}

static int check_comcom(const char *dir)
{
  char buf[1024];
  char *path;
  int err;
  ssize_t res;

  path = assemble_path(dir, "command.com");
  err = access(path, R_OK);
  res = readlink(path, buf, sizeof(buf) - 1);
  free(path);
  if (err == 0) {
    if (res == -1)
      return 1;
    /* have symlink */
    buf[res] = '\0';
    if (strncmp(buf, "comcom64.exe", res) == 0) {
#ifndef USE_DJDEV64
      error("comcom64 support not compiled in\n");
      return 0;
#endif
      dbug_printf("booting with comcom64\n");
    }
    return 1;
  }
  path = assemble_path(dir, "comcom64.exe");
  err = access(path, R_OK);
  free(path);
  if (err == 0) {
    error("comcom64 found in %s but command.com symlink is missing\n", dir);
    return 0;
  }
  path = assemble_path(dir, "comcom32.exe");
  err = access(path, R_OK);
  free(path);
  if (err == 0) {
    error("comcom32 found in %s but command.com symlink is missing\n", dir);
    return 0;
  }
  return 0;
}

static void comcom_hook(struct sys_dsc *sfiles, fatfs_t *fat)
{
  const char *dir = fatfs_get_host_dir(fat);

  if (strcmp(dir, comcom_dir) == 0) {
    if (strstr(dir, "32") != NULL)
      sfiles[CMD_IDX].flags |= FLG_COMCOM32;
    else
      sfiles[CMD_IDX].flags |= FLG_COMCOM64;
  }
}

static int check_freedos(const char *xdir)
{
  char *fboot, *fdir;
  fboot = assemble_path(xdir, FDBOOT_DIR);
  if (access(fboot, R_OK | X_OK) != 0) {
    free(fboot);
    return 0;
  }
  fdir = assemble_path(xdir, FREEDOS_DIR);
  if (access(fdir, R_OK | X_OK) != 0) {
    free(fboot);
    free(fdir);
    return 0;
  }

  fddir_boot = fboot;
  fddir_default = fdir;
  return 1;
}

static int check_bat(const char *xdir)
{
  char *bdir;
  bdir = assemble_path(xdir, XBAT_DIR);
  if (access(bdir, R_OK | X_OK) != 0) {
    free(bdir);
    return 0;
  }
  xbat_dir = bdir;
  return 1;
}

#ifdef USE_FDPP
static int fdpp_l;
void fdpp_loaded(void)
{
    fdpp_l++;
}
#endif

static void set_freedos_dir(void)
{
  const char *ccdir;
  int loaded = 0;
  const char *xdir = getenv("DOSEMU2_EXTRAS_DIR");
  const char *xdirs[] = {
    "/usr/share/dosemu2-extras",
    "/usr/local/share/dosemu2-extras",
    "/opt/dosemu2-extras",			/* gentoo */
    NULL,
  };
#ifdef USE_FDPP
  if (load_plugin("fdpp")) {
    loaded = fdpp_l;
    c_printf("fdpp: plugin loaded: %i\n", loaded);
  } else {
    error("can't load fdpp\n");
  }
#else
  warn("fdpp support is not compiled in.\n");
#endif

  if (xdir && access(xdir, R_OK | X_OK) != 0) {
    error("DOSEMU2_EXTRAS_DIR set incorrectly\n");
    xdir = NULL;
  }
  if (!loaded) {  // no fdpp
    if (xdir && check_freedos(xdir)) {
      config.try_freedos = 1;
    } else {
      int i;
      for (i = 0; xdirs[i]; i++) {
        if (access(xdirs[i], R_OK | X_OK) == 0 && check_freedos(xdirs[i])) {
          config.try_freedos = 1;
          loaded++;
          break;
        }
      }
    }
  }
  if (!loaded)  // neither fdpp nor freedos
    return;

  ccdir = getenv("DOSEMU2_COMCOM_DIR");
  if (ccdir && access(ccdir, R_OK | X_OK) == 0 && check_comcom(ccdir)) {
    comcom_dir = strdup(ccdir);
  } else {
    const char *comcom[] = {
#ifdef USE_DJDEV64
      "/usr/share/comcom64",
      "/usr/local/share/comcom64",
      PREFIX "/share/comcom64",
#endif
      "/usr/share/comcom32",
      "/usr/local/share/comcom32",
      "/opt/comcom32",			/* gentoo */
      NULL,
    };
    int i;
    for (i = 0; comcom[i]; i++) {
      if (access(comcom[i], R_OK | X_OK) == 0 && check_comcom(comcom[i])) {
        comcom_dir = strdup(comcom[i]);
        break;
      }
    }
  }
  if (comcom_dir)
    fatfs_set_sys_hook(comcom_hook);

  if (!fddir_default && !comcom_dir)
    error("Neither freecom nor comcom32 installation found.\n"
        "Use DOSEMU2_EXTRAS_DIR env var to specify location of freedos\n"
        "or DOSEMU2_COMCOM_DIR env var for alternative location of comcom32\n");

  if (!xdir || !check_bat(xdir)) {
    int i;
    for (i = 0; xdirs[i]; i++) {
      if (access(xdirs[i], R_OK | X_OK) == 0 && check_bat(xdirs[i]))
        break;
    }
  }
}

void move_dosemu_local_dir(void)
{
  const char *localdir = getenv("_local_dir");
  if (localdir && localdir[0] && !dosemu_localdir_path) {
    char *ldir = expand_path(localdir);
    if (ldir)
      dosemu_localdir_path = ldir;
    else
      error("local dir %s does not exist\n", localdir);
  }
  if (!dosemu_localdir_path)
    dosemu_localdir_path = get_dosemu_local_home();
  if (!dosemu_localdir_path)
    exit(1);

  if (!dosemu_image_dir_path)
    dosemu_image_dir_path = dosemu_localdir_path;
  setenv("DOSEMU_IMAGE_DIR", dosemu_image_dir_path, 1);

  if (!dosemu_drive_c_path)
    dosemu_drive_c_path = assemble_path(dosemu_localdir_path, DRIVE_C_DIR);
}

static void move_dosemu_lib_dir(void)
{
  char *old_cmd_path;
  const char *rp;
  char buf[256];

  if (!dosemu_plugin_dir_path)
    dosemu_plugin_dir_path = prefix(DOSEMUPLUGINDIR);
  if (!dosemu_lib_dir_path)
    dosemu_lib_dir_path = prefix(DOSEMULIB_DEFAULT);
  setenv("DOSEMU_LIB_DIR", dosemu_lib_dir_path, 1);
  set_freedos_dir();
  if (!commands_path)
    commands_path = prefix(DOSEMUCMDS_DEFAULT);
  if (access(commands_path, R_OK | X_OK) != 0) {
    error("dosemu2 commands not found at %s\n", commands_path);
    free(commands_path);
    commands_path = NULL;
  }
  old_cmd_path = assemble_path(dosemu_lib_dir_path, "dosemu2-cmds-0.1");
  if (access(old_cmd_path, R_OK | X_OK) == 0)
    setenv("DOSEMU_COMMANDS_DIR", old_cmd_path, 1);
  else if (commands_path)
    setenv("DOSEMU_COMMANDS_DIR", commands_path, 1);
  free(old_cmd_path);

  if (keymap_load_base_path != keymaploadbase_default)
    free(keymap_load_base_path);
  keymap_load_base_path = assemble_path(dosemu_lib_dir_path, "");

  sprintf(buf, "%d", get_orig_uid());
  rp = getenv("XDG_RUNTIME_DIR");
  if (rp && rp[0])
    dosemu_rundir_path = mkdir_under(rp, "dosemu2");
  if (dosemu_rundir_path) {
    dosemu_midi_path = assemble_path(dosemu_rundir_path, DOSEMU_MIDI);
    dosemu_midi_in_path = assemble_path(dosemu_rundir_path, DOSEMU_MIDI_IN);
  }
}

static int find_option(const char *option, int argc, char **argv)
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

/*
 * Please keep "getopt_string", secure_option_preparse(), config_init(),
 * usage() and the manpage in sync!
 */
static char * get_option(const char *key, int with_arg, int *argc,
    char **argv)
{
  char *p;
  char *basename;
  int o = find_option(key, *argc, argv);
  if (!o) return 0;
  o = option_delete(o, argc, argv);
  if (!with_arg) return NULL;
  if (!with_arg || o >= *argc) return NULL;
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

static char *path_expand(char *path)
{
  char buf[PATH_MAX];
  char *p, *pp;

  buf[0] = '\0';
  for (pp = path, p = strchr(path, '%'); p; pp = p + 2, p = strchr(pp, '%')) {
    int len = strlen(buf);
    if (p > pp)
      snprintf(buf + len, sizeof(buf) - len, "%.*s", (int)(p - pp), pp);
    switch (p[1]) {
    case 'I':
      strlcat(buf, DOSEMUIMAGE_DEFAULT, sizeof(buf));
      break;
    default:
      error("Unknown substitution %%%c\n", p[1]);
      return NULL;
    }
  }
  strlcat(buf, pp, sizeof(buf));
  return expand_path(buf);
}

void secure_option_preparse(int *argc, char **argv)
{
  char *opt;
  int cnt;

  do {
    cnt = 0;

    opt = get_option("--Ffs_backend", 1, argc, argv);
    if (opt) {
      free(config.fs_backend);
      config.fs_backend = opt;
    }

    opt = get_option("--Flibdir", 1, argc, argv);
    if (opt && opt[0]) {
      char *opt1 = path_expand(opt);
      if (opt1) {
        free(dosemu_lib_dir_path);
        dosemu_lib_dir_path = opt1;
        cnt++;
      } else {
        error("--Flibdir: %s does not exist\n", opt);
        config.exitearly = 1;
      }
      free(opt);
    }

    opt = get_option("--Fexecdir", 1, argc, argv);
    if (opt && opt[0]) {
      char *opt1 = path_expand(opt);
      if (opt1) {
        replace_string(CFG_STORE, dosemu_exec_dir_path, opt1);
        dosemu_exec_dir_path = opt1;
        cnt++;
      } else {
        error("--Fexecdir: %s does not exist\n", opt);
        config.exitearly = 1;
      }
      free(opt);
    }

    opt = get_option("--Fplugindir", 1, argc, argv);
    if (opt && opt[0]) {
      char *opt1 = path_expand(opt);
      if (opt1) {
        free(dosemu_plugin_dir_path);
        dosemu_plugin_dir_path = opt1;
        cnt++;
      } else {
        error("--Fplugindir: %s does not exist\n", opt);
        config.exitearly = 1;
      }
      free(opt);
    }

    opt = get_option("--Fcmddir", 1, argc, argv);
    if (opt && opt[0]) {
      char *opt1 = path_expand(opt);
      if (opt1) {
        free(commands_path);
        commands_path = opt1;
        cnt++;
      } else {
        error("--Fcmddir: %s does not exist\n", opt);
        config.exitearly = 1;
      }
      free(opt);
    }

    opt = get_option("--Fimagedir", 1, argc, argv);
    if (opt && opt[0]) {
      char *opt1 = path_expand(opt);
      if (opt1) {
        free(dosemu_image_dir_path);
        dosemu_image_dir_path = opt1;
        cnt++;
      } else {
        error("--Fimagedir: %s does not exist\n", opt);
        config.exitearly = 1;
      }
      free(opt);
    }

    opt = get_option("--Fdrive_c", 1, argc, argv);
    if (opt && opt[0]) {
      char *opt1 = path_expand(opt);
      if (opt1) {
        free(dosemu_drive_c_path);
        dosemu_drive_c_path = opt1;
        config.alt_drv_c = 1;
        cnt++;
      } else {
        error("--Fdrive_c: %s does not exist\n", opt);
        config.exitearly = 1;
      }
      free(opt);
    }

    opt = get_option("--Flocal_dir", 1, argc, argv);
    if (opt && opt[0]) {
      char *opt1 = path_expand(opt);
      if (opt1) {
        free(dosemu_localdir_path);
        dosemu_localdir_path = opt1;
        cnt++;
      } else {
        error("--Flocal_dir: %s does not exist\n", opt);
        config.exitearly = 1;
      }
      free(opt);
    }

  } while (cnt);

  move_dosemu_lib_dir();
}

static void read_cpu_info(void)
{
    char *cpuflags, *cpu;
    int k = 3;
    int err;

    err = open_proc_scan("/proc/cpuinfo");
    if (err)
      return;
    cpu = get_proc_string_by_key("cpu family");
    if (cpu) {
      k = atoi(cpu);
    } else { /* old kernels < 2.1.74 */
      cpu = get_proc_string_by_key("cpu");
      /* 386, 486, etc */
      if (cpu) k = atoi(cpu) / 100;
    }
    if (k > 5) k = 5;

    cpuflags = get_proc_string_by_key("features");
    if (!cpuflags)
      cpuflags = get_proc_string_by_key("flags");
    if (cpuflags && strstr(cpuflags, "umip"))
      config.umip = 1;
    else
      warn("Your CPU doesn't support UMIP\n");

    switch (k) {
      case 5:
        config.realcpu = CPU_586;
#ifdef X86_EMULATOR
        if (cpuflags && (strstr(cpuflags, "mmxext") ||
			 strstr(cpuflags, "sse"))) {
	  config.cpuprefetcht0 = 1;
        }
#endif
#ifdef __i386__
        if (cpuflags && (strstr(cpuflags, "fxsr"))) {
          config.cpufxsr = 1;
	  if (cpuflags && strstr(cpuflags, "sse"))
	    config.cpusse = 1;
	}
#endif
        if (cpuflags && strstr(cpuflags, "tsc")) {
          /* bogospeed currently returns 0; should it deny
           * pentium features, fall back into 486 case */
	  if ((cpuflags = get_proc_string_by_key("cpu MHz"))) {
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
		while (*p!='.') p++;
		p++;
		for (i=0; i<6; i++) cdd[i]=(*p && isdigit(*p)? *p++:'0');
		cdd[6]=0; sscanf(cdd,"%d",&df);
		/* speed division factor to get 1us from CPU clocks - for
		 * details on fast division see timers.h */
		chz = (di * 1000000LL) + df;

		/* speed division factor to get 1us from CPU clock */
		config.cpu_spd = (LLF_US*1000000)/chz;

		/* speed division factor to get 838ns from CPU clock */
		config.cpu_tick_spd = (LLF_TICKS*1000000)/chz;

		dbug_printf("Linux kernel %d.%d.%d; CPU speed is %lld Hz\n",
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
        }
        /* fall through */
      case 4: config.realcpu = CPU_486;
        /* fall through */
      case 3:
      	break;
      default:
        error("Unknown CPU type!\n");
	/* config.realcpu is set to CPU_386 at this point */
    }
    if (config.mathco) {
      const char *s = get_proc_string_by_key("fpu");
      config.mathco = s && (strcmp(s, "yes") == 0);
    }
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
}

static void config_post_process(void)
{
#ifdef X86_EMULATOR
    char buf[256];
    size_t n;
    char *di;
    FILE *f = popen("uname -r", "r");
    n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    if (strstr(buf, "Microsoft") != NULL) {
	c_printf("CONF: Running on Windows, SIM CPUEMU enabled\n");
	config.cpusim = 1;
	config.cpu_vm = CPUVM_EMU;
	config.cpu_vm_dpmi = CPUVM_EMU;
    }
    pclose(f);
#endif
    config.realcpu = CPU_386;
    if (vm86s.cpu_type > config.realcpu || config.mathco)
	read_cpu_info();
    if (vm86s.cpu_type > config.realcpu) {
	vm86s.cpu_type = config.realcpu;
	fprintf(stderr, "CONF: emulated CPU forced down to real CPU: %d86\n",(int)vm86s.cpu_type);
    }
    if (config.cpu_vm_dpmi == CPUVM_NATIVE)
      error("@Security warning: native DPMI mode is insecure, "
          "adjust $_cpu_vm_dpmi\n");
    c_printf("CONF: V86 cpu vm set to %d\n", config.cpu_vm);
    c_printf("CONF: DPMI cpu vm set to %d\n", config.cpu_vm_dpmi);

    /* console scrub */
    di = getenv("DISPLAY");
    if (!di || !di[0])
	di = getenv("WAYLAND_DISPLAY");
    if (!Video && di && *di && !config.X && !config.term &&
        config.cardtype != CARD_NONE) {
	config.console_video = 0;
	config.emuretrace = 0;	/* already emulated */
#ifdef SDL_SUPPORT
	config.sdl = 1;
	config.sdl_sound = 1;
#else
#ifdef X_SUPPORT
	config.X = 1;
#endif
#endif
    }
#ifdef USE_CONSOLE_PLUGIN
    if (on_console()) {
	c_printf("CONF: running on console, vga=%i cv=%i\n", config.vga,
	        config.console_video);
	if (!can_do_root_stuff && config.console_video) {
	    /* force use of Slang-terminal on console too */
	    config.console_video = 0;
	    dbug_printf("no console on low feature (non-suid root) DOSEMU\n");
	}
	if (config.console_keyb == -1)
	    config.console_keyb = KEYB_RAW;
	if (config.speaker == SPKR_EMULATED) {
	    register_speaker((void *)(uintptr_t)console_fd,
			     console_speaker_on, console_speaker_off);
	}
    } else
#endif
    {
	c_printf("CONF: not running on console\n");
	if (config.console_keyb == -1) {
	    config.console_keyb =
#ifdef USE_SLANG
		    /* Slang will take over KEYB_OTHER */
		    KEYB_OTHER;
		    load_plugin("term");
#else
		    KEYB_TTY;
#endif
	}
	config.console_video = 0;
#if 0
	if (config.speaker == SPKR_NATIVE) {
	    config.speaker = SPKR_EMULATED;
	}
#endif
    }
    if (!config.console_video)
	config.vga = config.mapped_bios = 0;
    if (config.vga)
	config.mapped_bios = config.console_video = 1;
    else
	config.vbios_post = 0;
    if ((config.vga || config.X) && config.mem_size > 640) {
	error("$_dosmem = (%d) not allowed for X and VGA console graphics, "
              "restricting to 640K\n", config.mem_size);
        config.mem_size = 640;
    }
    if (config.umb_a0 == -1) {
	config.umb_a0 = config.term;
#if 0
	if (config.umb_a0) {
	    warn("work around FreeDOS UMB bug\n");
	    config.umb_a0++;
	}
#endif
    }
    if (config.umb_b0 == -1)
	config.umb_b0 = config.dumb_video;
    if (config.umb_b8)
	config.umb_b8 = config.dumb_video;

    /* page-align memory sizes */
    config.ext_mem &= ~3;
    config.xms_size &= ~3;
    config.ems_size &= ~3;
    config.dpmi &= ~3;

    /* UID scrub */
    if (under_root_login)  c_printf("CONF: running exclusively as ROOT:");
    else {
      c_printf("CONF: mostly running as USER:");
    }
    c_printf(" uid=%d euid=%d gid=%d egid=%d\n",
        getuid(), geteuid(), getgid(), getegid());
    c_printf("CONF: priv operations %s\n",
            can_do_root_stuff ? "available" : "unavailable");

    /* Speaker scrub */
#ifdef X86_EMULATOR
    if (IS_EMU() && config.speaker==SPKR_NATIVE) {
	c_printf("SPEAKER: can`t use native mode with cpu-emu\n");
	config.speaker=SPKR_EMULATED;
    }
#endif

    if (config.pci && !can_do_root_stuff) {
        c_printf("CONF: Warning: PCI requires root, disabled\n");
        config.pci = 0;
    }

    if (!config.internal_cset) {
#ifdef LOCALE_PLUGIN
        /* json plugin loads locale settings */
        load_plugin("json");
#else
        set_internal_charset("cp437");
#endif
    }
    if (!config.internal_cset)
        set_internal_charset("cp437");

#ifdef USE_IEEE1284
    if (config.opl2lpt_device)
        load_plugin("lpt");
#endif
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
		c_printf("register_config_scrub failed > %zu config_scrub functions\n",
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
#define I_MAX 5
    int             i_incr[I_MAX] = {};
    int             i_found = 0;
    int             i_cur;
    int             can_do_root_stuff_enabled = 0;
    char           *confname = NULL;
    char           *dosrcname = NULL;
    int             nodosrc = 0;
    char           *basename;
    int             err;
    char            buf[256];
    int             was_exec = 0, was_T1 = 0;
    const char * const getopt_string =
       "23456A::B::C::c::D:d:E:e:f:H:hi:I:K:k::L:M:mNno:P:qSsT::t::VvwXx:Y"
       "gp"/*NOPs kept for compat (not documented in usage())*/;

    basename = strrchr(argv[0], '/');   /* parse the program name */
    basename = basename ? basename + 1 : argv[0];

    dosemu_argc = argc;
    dosemu_argv = argv;
    if (dosemu_proc_self_exe == NULL)
	dosemu_proc_self_exe = dosemu_argv[0];

    memcheck_type_init();
    our_envs_init();
    parse_debugflags("+cw", 1);
#ifdef USE_DJDEV64
    register_debug_class('J', NULL, "dj64");
#endif
    Video = NULL;

    /* options get parsed twice so show our own errors and only once */
    opterr = 0;
    while ((c = getopt(argc, argv, getopt_string)) != EOF) {
	switch (c) {
	case 's':
	    if (can_do_root_stuff)
		can_do_root_stuff_enabled = 1;
	    else
		error("The -s switch requires root privileges\n");
	    break;
	case 'h':
	    usage(basename);
	    exit(0);
	    break;
	case 'f':
	    dosrcname = path_expand(optarg);
	    if (!dosrcname) {
		error("%s is missing\n", optarg);
		leavedos(2);
	    }
	    break;
	case 'I':
	    assert(i_found < I_MAX);
	    commandline_statements = strdup(optarg);
	    if (commandline_statements[0] == '\'') {
		commandline_statements[0] = ' ';
		while (commandline_statements[strlen(commandline_statements) - 1] != '\'') {
		    commandline_statements = realloc(commandline_statements,
			    strlen(commandline_statements) + 1 +
			    strlen(argv[optind]) + 1);
		    strcat(commandline_statements, " ");
		    strcat(commandline_statements, argv[optind]);
		    optind++;
		    i_incr[i_found]++;
		}
		commandline_statements[strlen(commandline_statements) - 1] = 0;
	    }
	    i_found++;
	    break;
	case 'c':
	    config.console_video = 1;
	    config.vga = 0;	// why?
	    if (optarg && optarg[0] == 'd')
		config.detach = 1;
	    break;
	case 'o': {
	    char *tmp = strdup(optarg);
	    char *p = strstr(tmp, ".%");

	    free(config.debugout);
	    config.debugout = NULL;
	    if (!p) {
		config.debugout = tmp;
	    } else {
		*p = '\0';
		switch (p[2]) {
		    case 'P':
			asprintf(&config.debugout, "%s.%i", tmp, getpid());
			break;
		    default:
			error("Unknown suffix %s\n", p + 1);
			break;
		}
		free(tmp);
	    }
	    break;
	}
	case 'n':
	    nodosrc = 1;
	    break;
	case 'q':
	    config.quiet = 1;
	    break;
	case 'v':
	    printf("dosemu2-" VERSTR "\n");
	    printf("Revision: %i\n", REVISION);
	    exit(0);
	}
    }

    if (!can_do_root_stuff_enabled)
	can_do_root_stuff = 0;
    else if (under_root_login)
	fprintf(stderr,"\nRunning as root in full feature mode\n");
    else
	fprintf(stderr,"\nRunning privileged (%s) in full feature mode\n",
		using_sudo ? "via sudo" : "suid-root");

    stdio_init();		/* initialize stdio & open debug file */
    if (config_check_only) set_debug_level('c',1);

    if (running_suid_orig())
	snprintf(buf, sizeof(buf), "dosemu2_%i_%i", getuid(), get_suid());
    else
	snprintf(buf, sizeof(buf), "dosemu2_%i", getuid());
    char *tmp = getenv("TMPDIR");
    dosemu_tmpdir = mkdir_under(tmp ?: "/tmp", buf);
    if (!dosemu_tmpdir) {
	error("failed to create tmpdir\n");
	exit(1);
    }
    shlock_init(dosemu_tmpdir);

    if (nodosrc && dosrcname) {
        c_printf("CONF: using %s as primary config\n", dosrcname);
        confname = dosrcname;
        dosrcname = NULL;
    } else {
        confname = assemble_path(DOSEMU_CONF_DIR, DOSEMU_CONF);
        if (access(confname, R_OK) == -1) {
            free(confname);
            confname = NULL;
        }
    }
    parse_config(confname, dosrcname, nodosrc);
    free(confname);
    free(dosrcname);

    if (config.exitearly && !config_check_only)
	exit(0);

    i_cur = 0;
    GETOPT_RESET();
    while ((c = getopt(argc, argv, getopt_string)) != EOF) {
	switch (c) {
	case 'f':
	case 'c':
	case 'o':
	case 'n':
	case 'q':
	case 's':
	    break;
	case 'L':
	    dbug_printf("%s\n", optarg);
	    break;
	case 'd': {
	    char *p = strdup(optarg);
	    char *d = strchr(p, ':');
	    int ro = 0;
	    int cd = 0;
	    int grp = 0;
	    if (d) {
		switch (d[1]) {
		case 'R':
		case 'r':
		    ro++;
		    break;
		case 'C':
		case 'c':
		    cd++;
		    break;
		case 'G':
		case 'g':
		    grp++;
		    break;
		}
		*d = '\0';
	    }
	    err = add_extra_drive(p, ro, cd, grp);
	    free(p);
	    if (err)
		config.exitearly = 1;
	    break;
	}
	case 'H': {
#ifdef USE_MHPDBG
	    dosdebug_flags = strtoul(optarg,0,0) & 255;
#else
	    error("debugger support not compiled in\n");
#endif
	    break;
            }
	case 'i':
	    append_pre_strokes(optarg);
	    break;
	case 'I':
	    assert(i_cur < i_found);
	    optind += i_incr[i_cur++];
	    break;
	case '2': case '3': case '4': case '5': case '6':
	    {
		int cpu = cpu_override (c-'0');
		if (cpu > 0) {
			fprintf(stderr,"CPU set to %d86\n",cpu);
			vm86s.cpu_type = cpu;
		}
		else
			fprintf(stderr,"error in CPU user override\n");
	    }
	    break;
	case 'g': /* obsolete "graphics" option */
	    break;
	case 'A':
	    config.hdiskboot = 0;
	    if (optarg) {
		if (config.fdisks < 1)
		    config.fdisks = 1;
		assign_floppy(0, optarg);
	    }
	    break;
	case 'B':
	    config.hdiskboot = 1;
	    if (optarg) {
		if (config.fdisks < 2)
		    config.fdisks = 2;
		assign_floppy(1, optarg);
	    }
	    break;
	case 'C':
	    config.hdiskboot = 2;
	    if (optarg && isdigit(optarg[0]) && optarg[0] > '0') {
		config.hdiskboot += optarg[0] - '0';
		config.swap_bootdrv = 1;
	    }
	    break;
	case 'k':
	    if (optarg) {
		switch (optarg[0]) {
		case 's':
		    config.console_keyb = KEYB_STDIO;
		    break;
		case 't':
		    config.console_keyb =
#ifdef USE_SLANG
			/* Slang will take over KEYB_OTHER */
			KEYB_OTHER;
			load_plugin("term");
#else
			KEYB_TTY;
#endif
		    break;
		case 'r':
		    config.console_keyb = KEYB_RAW;
		    break;
		case 'o':
		    config.console_keyb = KEYB_OTHER;
		    break;
		}
	    } else {
		config.console_keyb = KEYB_RAW;
	    }
	    break;
	case 't':
	    if (!optarg || optarg[0] != ':') {
		/* terminal mode */
		config.X = config.console_video = 0;
		config.term = 1;
	    }
	    if (optarg) {
		char *opt_e;
		if (strchr(optarg, 'c'))
		    config.clip_term = 1;
		if (strchr(optarg, 'd'))
		    config.dumb_video = 1;
		if ((opt_e = strchr(optarg, 'e'))) {
		    if (isdigit(opt_e[1]))
			config.tty_stderr = opt_e[1] - '0';
		    else
			config.tty_stderr = 1;
		}
	    }
	    break;
	case 'X':
	    config.X = 1;
	    break;
	case 'S':
	    config.sdl = 1;
	    config.sdl_sound = 1;
	    break;
	case 'w':
            config.X_fullscreen = !config.X_fullscreen;
	    break;
	case 'D':
	    parse_debugflags(optarg, 1);
	    break;
	case 'p':
	    /* unused */
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
	    if (config.mem_size > 640)
		config.mem_size = 640;
	    break;
	case 'Y':
	    config.dos_trace = 1;
	    break;
	case 'N':
	    warn("DOS will not be started\n");
	    config.exitearly = 1;
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
	    g_printf("turning internal MOUSE driver %s\n",
		     config.mouse.intdrv ? "off" : "on");
	    config.mouse.intdrv = !config.mouse.intdrv;
	    break;

	case 'E':
	    g_printf("DOS command given on command line: %s\n", optarg);
	    config.dos_cmd = optarg;
	    was_exec++;
	    break;
	case 'K': {
	    char *p;
	    g_printf("Unix path set to %s\n", optarg);
	    config.unix_path = strdup(optarg);
	    p = strchr(config.unix_path, ':');
	    if (p) {
		*p = '\0';
		config.dos_path = strdup(p + 1);
		if (p == config.unix_path) {
		    free(config.unix_path);
		    config.unix_path = NULL;
		}
	    }
	    if (config.unix_path && !exists_dir(config.unix_path) &&
			!exists_file(config.unix_path)) {
		error("Path %s does not exist\n", config.unix_path);
		config.exitearly = 1;
		break;
	    }
	    break;
	}
	case 'T':
	    if (!optarg || strchr(optarg, '1'))
	      was_T1++;
	    if (optarg && strchr(optarg, 'h'))
	      misc_e6_store_options("SHELL_LOADHIGH_DEFAULT=1");
	    break;

	case '?':
	default:
	    fprintf(stderr, "unrecognized option or missing argument: -%c\n\r", optopt);
	    usage(basename);
	    exit(1);
	}
    }

    /* make-style env vars passing */
    while (optind < argc) {
	char *p, *p1;
again:
	if (strchr(argv[optind], '=') == NULL) {
	    char *fpath;
	    if (config.dos_cmd || config.unix_path)
		break;
	    fpath = expand_path(argv[optind]);
	    if (!fpath) {
		error("%s not found\n", argv[optind]);
		break;
	    }
	    p = strrchr(fpath, '/');
	    assert(p); // expand_path() should add some slashes
	    config.unix_path = strdup(fpath);
	    p1 = config.unix_path + (p - fpath);
	    *p1 = '\0';
	    p++;
	    if (!*p) {
		free(fpath);
		break;
	    }
	    config.dos_cmd = strdup(p);
	    free(fpath);
	    was_exec++;
	    optind++;
	    /* collect args */
	    while (argv[optind]) {
		if (strchr(argv[optind], '='))
			goto again;
		g_printf("DOS command given on command line: %s\n", argv[optind]);
		config.dos_cmd = realloc(config.dos_cmd,
			strlen(config.dos_cmd) + strlen(argv[optind]) + 2);
		strcat(config.dos_cmd, " ");
		strcat(config.dos_cmd, argv[optind]);
		optind++;
	    }
	    break;
	}
	g_printf("ENV given on command line: %s\n", argv[optind]);
	misc_e6_store_options(argv[optind]);
	optind++;
    }
    if (argv[optind]) {
	fprintf(stderr, "unrecognized argument: %s\n\r", argv[optind]);
	exit(1);
    }

    if (was_exec && !was_T1)
      misc_e6_store_options("DOSEMU_EXIT=1");

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

void config_close(void)
{
    closedir_under(dosemu_tmpdir);
    /* /tmp has sticky bit and we changed user, so can't rmdir() */
    free(dosemu_tmpdir);
}

/*
 * Please keep "getopt_string", secure_option_preparse(), config_init(),
 * usage() and the manpage in sync!
 */
static void
usage(char *basename)
{
    fprintf(stderr,
	"dosemu-" VERSTR "\n\n"
	"USAGE:\n"
	"  %s [options] [-K linux path] [-E dos command]\n"
	"\n"
	"    -2,3,4,5,6 choose 286, 386, 486 or 586 or 686 CPU\n"
	"    -A[file] boot from first defined floppy disk (A)\n"
	"    -B[file] boot from second defined floppy disk (B) (#)\n"
	"    -C boot from first defined hard disk (C)\n"
	"    -c use PC console video (!%%)\n"
	"    -X run in X Window (#)\n"
	"    -S run in SDL (#)\n"
    , basename);
    print_debug_usage(stderr);
    fprintf(stderr,
	"    -E STRING pass DOS command on command line\n"
	"    -d DIR - mount DIR as a drive under DOS\n"
	"    -e SIZE enable SIZE K EMS RAM\n"
	"    -f use dosrcFile as user config-file\n"
	"    --Fusers bypass /etc/dosemu.users (^^)\n"
	"    --Flibdir change keymap and FreeDOS location\n"
	"    --Fimagedir bypass systemwide boot path\n"
	"    -n bypass the user configuration file (.dosemurc)\n"
	"    -L load and execute DEXE File\n"
	"    -I insert config statements (on commandline)\n"
	"    -i[bootdir] (re-)install a DOS from bootdir or interactively\n"
	"    -h display this help\n"
	"    -H wait for dosdebug terminal at startup and pass dflags\n"
	"    -k use PC console keyboard (!)\n"
	"    -K Specify unix path for program running with -E\n"
	"    -M set memory size to SIZE kilobytes (!)\n"
	"    -m toggle internal mouse driver\n"
	"    -N No boot of DOS\n"
	"    -o FILE put debug messages in file\n"
	"    -P copy debugging output to FILE\n"
	"    -p stop for prompting with a non-fatal configuration problem\n"
	"    -s enable direct hardware access (full feature) (!%%)\n"
	"    -T set flags for -E or -K options\n"
	"    -t use terminal (S-Lang) mode\n"
	"    -V use BIOS-VGA video modes (!#%%)\n"
	"    -v display version\n"
	"    -w toggle windowed/fullscreen mode in X\n"
	"    -x SIZE enable SIZE K XMS RAM\n"
	"    -Y interactive boot\n"
	"\n"
	"    (!) BE CAREFUL! READ THE DOCS FIRST!\n"
	"    (%%) require DOSEMU be run as root (i.e. suid)\n"
	"    (^^) require DOSEMU not be run as root (i.e. not suid)\n"
	"    (#) options do not fully work yet\n"
	"\n");
    fprintf(stderr,
	"  %s --help\n"
	"  %s --version    print version of dosemu (and show this help)\n",
        basename, basename);
}


	/* charset */
static struct char_set *get_charset(const char *name)
{
	struct char_set *charset;

	charset = lookup_charset(name);
	if (!charset) {
		error("Can't find charset %s\n", name);
	}
	return charset;

}

void set_external_charset(const char *charset_name)
{
	struct char_set *charset;
	charset = get_charset(charset_name);
	if (!charset) {
		error("charset %s not available\n", charset_name);
		return;
	}
	charset = get_terminal_charset(charset);
	if (charset) {
		if (!trconfig.output_charset) {
			trconfig.output_charset = charset;
		}
		if (!trconfig.keyb_charset) {
			trconfig.keyb_charset = charset;
		}
	}

	config.external_cset = strdup(charset_name);
}

void set_internal_charset(const char *charset_name)
{
	struct char_set *charset_video, *charset_config;
	charset_video = get_charset(charset_name);
	if (!charset_video) {
		error("charset %s not available\n", charset_name);
		return;
	}
	if (!is_display_charset(charset_video)) {
		error("%s not suitable as an internal charset", charset_name);
	}
	charset_config = get_terminal_charset(charset_video);
	if (charset_video) {
		trconfig.video_mem_charset = charset_video;
	}
#if 0
	if (charset_config) {
		trconfig.keyb_config_charset = charset_config;
	}
#endif
	if (charset_config) {
		trconfig.dos_charset = charset_config;
	}

	config.internal_cset = strdup(charset_name);
}

void set_country_code(int cntry)
{
	config.country = cntry;
}

int set_floppy_type(struct disk *dptr, const char *name)
{
	struct stat st;

	if (stat(name, &st) < 0)
		return -1;
	if (S_ISREG(st.st_mode))
		dptr->type = IMAGE;
	else if (S_ISBLK(st.st_mode))
		dptr->type = FLOPPY;
	else if (S_ISDIR(st.st_mode))
		dptr->type = DIR_TYPE;
	else
		return -1;
	return 0;
}

void dp_init(struct disk *dptr)
{
  dptr->type    =  NODISK;
  dptr->sectors = -1;
  dptr->heads   = -1;
  dptr->tracks  = -1;
  dptr->fdesc   = -1;
  dptr->hdtype = 0;
  dptr->timeout = 0;
  dptr->dev_name = NULL;              /* default-values */
  dptr->rdonly = 0;
  dptr->header = 0;
  dptr->floppy = 0;
}

static void assign_floppy(int fnum, const char *name)
{
  struct disk *dptr = &disktab[fnum];

  dp_init(dptr);
  dptr->floppy = 1;
  dptr->type = FLOPPY;
  dptr->default_cmos = THREE_INCH_FLOPPY;
  set_floppy_type(dptr, name);
  dptr->dev_name = strdup(name);
  if (dptr->type == DIR_TYPE)
    dptr->mfs_idx = mfs_define_drive(dptr->dev_name);
  else
    dptr->mfs_idx = 0;

  c_printf("floppy %c:\n", 'A' + fnum);
  dptr->drive_num = fnum;
}

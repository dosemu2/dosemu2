/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */
#ifndef DOSEMU_CONFIG_H
#define DOSEMU_CONFIG_H

#include "plugin_config.h"
#include "disks.h"

#ifndef __ASSEMBLER__
/*
 * DANG_BEGIN_REMARK
 * DOSEMU keeps system wide configuration status in a structure
 * called config.
 * DANG_END_REMARK
 */
extern struct config_info config;

extern void config_init(int argc, char **argv);
extern void config_close(void);
extern int set_floppy_type(struct disk *dptr, const char *name);
extern void dp_init(struct disk *dptr);
extern void secure_option_preparse(int *argc, char **argv);
extern int cpu_override (int cpu);

typedef void (*config_scrub_t)(void);
int register_config_scrub(config_scrub_t config_scrub);
void unregister_config_scrub(config_scrub_t old_config_scrub);
int define_config_variable(const char *name);
char *get_config_variable(const char *name);
char *checked_getenv(const char *name);
extern char dosemu_conf[];
extern char global_conf[];
extern char *dosemu_proc_self_exe;
extern int dosemu_proc_self_maps_fd;
extern int dosemu_argc;
extern char **dosemu_argv;
extern char *commandline_statements;
extern int config_check_only;

/* not overridable file constants */
#define  DOSEMU_RC          ".dosemurc"       /* per user config file */
#define  LOCALDIR_BASE_NAME ".dosemu"         /* base directory in $HOME */
#define  DOSEMU_CONF        "dosemu.conf"     /* standard configuration file */
#define  DEFAULT_CONFIG_SCRIPT "builtin"      /* main configuration script */
#define  DOSEMU_LOGLEVEL    "dosemu.loglevel" /* logging configuration file */
#define  DOSEMU_MIDI        "dosemu-midi"     /* fifo for midi daemon */
#define  DOSEMU_MIDI_IN     "dosemu-midi_in"  /* fifo for midi input */
#define  FREEDOS_DIR        "freedos"         /* freedos dir name */
#define  FDBOOT_DIR         "fdboot"          /* freedos boot dir name */
#define  XBAT_DIR           "bat"             /* extras */
#define  DOSEMULIB_DEFAULT  "share/dosemu"
#define  DOSEMUCMDS_DEFAULT DOSEMULIB_DEFAULT "/" CMDS_SUFF
#define  DOSEMUEXEC_DEFAULT  LIBEXECDIR "/dosemu"
#define  DOSEMUIMAGE_DEFAULT "~/" LOCALDIR_BASE_NAME
#define  DRIVE_C_DIR        "drive_c"
#define  DRIVE_C_DEFAULT    DOSEMUIMAGE_DEFAULT "/" DRIVE_C_DIR
#define  DOSEMU_DRIVES_DIR  "drives"
#define  RUNDIR_PREFIX      "/var/run/user"
#define  DOSEMU_CONF_DIR       SYSCONFDIR "/" CONFSUBDIR
#define  DOSEMUPLUGINDIR "lib/dosemu"

extern const char *config_script_name;
extern const char *dosemu_loglevel_file_path;
extern char *dosemu_rundir_path;
extern char *dosemu_localdir_path;
extern char *dosemu_tmpdir;

extern char *fddir_default;
extern char *comcom_dir;
extern char *fddir_boot;
extern char *xbat_dir;
extern char *commands_path;
extern char *dosemu_lib_dir_path;
extern const char *dosemu_exec_dir_path;
extern char *dosemu_plugin_dir_path;
extern char *dosemu_image_dir_path;
extern char *dosemu_drive_c_path;
extern char keymaploadbase_default[];
extern char *keymap_load_base_path;
extern const char *keymap_dir_path;
extern const char *owner_tty_locks;
extern const char *tty_locks_dir_path;
extern const char *tty_locks_name_path;
extern const char *dosemu_midi_path;
extern const char *dosemu_midi_in_path;

extern struct cfg_string_store cfg_store;
#define CFG_STORE (struct string_store *)&cfg_store

extern char *dosemu_map_file_name;
#endif

#endif

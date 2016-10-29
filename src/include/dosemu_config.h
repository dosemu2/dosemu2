/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */
#ifndef DOSEMU_CONFIG_H
#define DOSEMU_CONFIG_H

#include "plugin_config.h"

#ifndef __ASSEMBLER__
/*
 * DANG_BEGIN_REMARK
 * DOSEMU keeps system wide configuration status in a structure
 * called config.
 * DANG_END_REMARK
 */
extern struct config_info config;

extern void config_init(int argc, char **argv);
extern void parse_dosemu_users(void);
extern void secure_option_preparse(int *argc, char **argv);
extern void keyb_layout(int value);
extern int cpu_override (int cpu);

typedef void (*config_scrub_t)(void);
int register_config_scrub(config_scrub_t config_scrub);
void unregister_config_scrub(config_scrub_t old_config_scrub);
int define_config_variable(char *name);
char *get_config_variable(char *name);
char *checked_getenv(const char *name);
extern char dosemu_conf[];
extern char global_conf[];
extern char *dosemu_proc_self_exe;
extern int dosemu_proc_self_maps_fd;
extern int dosemu_argc;
extern char **dosemu_argv;
extern char *commandline_statements;
extern int config_check_only;
extern int dexe_running;

#include "confpath.h"
/* not overridable file constants */
#define  DOSEMU_RC          ".dosemurc"       /* per user config file */
#define  OLD_DOS_RC         ".dosrc"          /* old, obsolete user config file */
#define  LOCALDIR_BASE_NAME ".dosemu"         /* base directory in $HOME */
#define  DOSEMU_CONF        "dosemu.conf"     /* standard configuration file */
#define  DOSEMU_USERS       "dosemu.users"    /* access right configuration file */
#define  DEFAULT_CONFIG_SCRIPT "builtin"      /* main configuration script */
#define  DOSEMU_LOGLEVEL    "dosemu.loglevel" /* logging configuration file */
#define  DOSEMU_MIDI        "dosemu-midi"     /* fifo for midi daemon */
#define  DOSEMU_MIDI_IN     "dosemu-midi_in"  /* fifo for midi input */
#define  FREEDOS_DIR        "freedos"         /* freedos dir name */

extern char *config_script_name;
extern char *config_script_path;
extern char *dosemu_users_file_path;
extern char *dosemu_loglevel_file_path;
extern char *dosemu_rundir_path;
extern char *dosemu_localdir_path;

extern char dosemulib_default[];
extern char *fddir_default;
extern char *dosemu_lib_dir_path;
extern char dosemuhdimage_default[];
extern char *dosemu_hdimage_dir_path;
extern char keymaploadbase_default[];
extern char *keymap_load_base_path;
extern char *keymap_dir_path;
extern char *owner_tty_locks;
extern char *tty_locks_dir_path;
extern char *tty_locks_name_path;
extern char *dexe_load_path;
extern char *dosemu_midi_path;
extern char *dosemu_midi_in_path;

#define    DOSEMU_USERS_FILE     dosemu_users_file_path
#define    DOSEMU_LOGLEVEL_FILE  dosemu_loglevel_file_path
#define    RUNDIR                dosemu_rundir_path
#define    LOCALDIR              dosemu_localdir_path
#define    KEYMAP_LOAD_BASE_PATH keymap_load_base_path
#define    KEYMAP_DIR            keymap_dir_path
#define    OWNER_LOCKS           owner_tty_locks
#define    PATH_LOCKD            tty_locks_dir_path
#define    NAME_LOCKF            tty_locks_name_path
#define    DOSEMU_MAP_PATH       dosemu_map_file_name
#define    DOSEMU_MIDI_PATH      dosemu_midi_path
#define    DOSEMU_MIDI_IN_PATH   dosemu_midi_in_path

extern char *dosemu_map_file_name;
#endif

#define VERSION_OF(a,b,c,d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))
#define DOSEMU_VERSION_CODE VERSION_OF(VERSION_NUM,SUBLEVEL,0,0)
#define IS_DEVEL_RELEASE (DOSEMU_VERSION_CODE && 65536)
#define GCC_VERSION_CODE (__GNUC__ * 1000 + __GNUC_MINOR__)

#endif

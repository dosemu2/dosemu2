/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */
#ifndef DOSEMU_CONFIG_H
#define DOSEMU_CONFIG_H

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
extern int kernel_version_code;
extern int dexe_running;

#endif

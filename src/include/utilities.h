/* 
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef UTILITIES_H
#define UTILITIES_H

struct cmd_db {
	char cmdname[12];
	void (*cmdproc)(int, char *[]);
};

int argparse(char *s, char *argvx[], int maxarg);
typedef void cmdprintf_func(const char *fmt, ...);
void call_cmd(const char *cmd, int maxargs, const struct cmd_db *cmdtab,
	 cmdprintf_func *printf);
void sigalarm_onoff(int on);
int is_console(int fd);

char *strprintable(char *s);
char *chrprintable(char c);
void open_proc_scan(char *name);
void close_proc_scan(void);
char *get_proc_string_by_key(char *key);
void advance_proc_bufferptr(void);
void reset_proc_bufferptr(void);
int get_proc_intvalue_by_key(char *key);
int integer_sqrt(int x);
int exists_dir(char *name);
int exists_file(char *name);
char *strcatdup(char *s1, char *s2);
char *assemble_path(char *dir, char *file, int append_pid);
char *mkdir_under(char *basedir, char *dir, int append_pid);
char *get_path_in_HOME(char *path);
char *get_dosemu_local_home(void);

/* returns y = sqrt(x), for y*y beeing a power of 2 below x
 */
static __inline__ int power_of_2_sqrt(int val)
{
	register int res;
	__asm__ __volatile__("
		bsrl	%2,%0
	" : "=r" (res) : "0" ((int)-1), "r" (val) );
	if (res <0) return 0;
	return 1 << (res >> 1);
}

#if (GLIBC_VERSION_CODE >= 2000) && defined(PORTABLE_BINARY)
  #include <pwd.h>
  #include <sys/types.h>
  struct passwd *our_getpwuid(uid_t uid);
  struct passwd *our_getpwnam(const char *name);
  #define getpwnam our_getpwnam
  #define getpwuid our_getpwuid
#endif

#endif /* UTILITIES_H */

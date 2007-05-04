/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef UTILITIES_H
#define UTILITIES_H

#include "dosemu_debug.h"

struct cmd_db {
	char cmdname[12];
	void (*cmdproc)(int, char *[]);
};

extern char *logptr, *logbuf;
extern int logbuf_size, logfile_limit;

int argparse(char *s, char *argvx[], int maxarg);
typedef void cmdprintf_func(const char *fmt, ...);
void call_cmd(const char *cmd, int maxargs, const struct cmd_db *cmdtab,
	 cmdprintf_func *printf);
void sigalarm_onoff(int on);
void sigalarm_block(int block);

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
void subst_file_ext(char *ptr);
char *strcatdup(char *s1, char *s2);
char *assemble_path(char *dir, char *file, int append_pid);
char *mkdir_under(char *basedir, char *dir, int append_pid);
char *get_path_in_HOME(char *path);
char *get_dosemu_local_home(void);
char *readlink_malloc (const char *filename);
char * strupr(char *s);
char * strlower(char *s);
void dosemu_error(char *fmt, ...) FORMAT(printf, 1, 2);
void *load_plugin(const char *plugin_name);

/* returns y = sqrt(x), for y*y beeing a power of 2 below x
 */
static __inline__ int power_of_2_sqrt(int val)
{
	register int res;
	__asm__ __volatile__(" \
		bsrl	%2,%0\n \
	" : "=r" (res) : "0" ((int)-1), "r" (val) );
	if (res <0) return 0;
	return 1 << (res >> 1);
}

/*
 * from the Linux kernel:
 * min()/max() macros that also do
 * strict type-checking.. See the
 * "unnecessary" pointer comparison.
 */
#define min(x,y) ({ \
	typeof(x) _x = (x);	\
	typeof(y) _y = (y);	\
	(void) (&_x == &_y);		\
	_x < _y ? _x : _y; })

#define max(x,y) ({ \
	typeof(x) _x = (x);	\
	typeof(y) _y = (y);	\
	(void) (&_x == &_y);		\
	_x > _y ? _x : _y; })

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define ALIGN(x,a) (((x)+(a)-1)&~((a)-1))


/* Ring buffer API */

struct rng_s {
  unsigned char *buffer;
  int objnum, objsize, objcnt, tail;
};
void rng_init(struct rng_s *rng, size_t objnum, size_t objsize);
int rng_destroy(struct rng_s *rng);
int rng_get(struct rng_s *rng, void *buf);
int rng_peek(struct rng_s *rng, int idx, void *buf);
int rng_put(struct rng_s *rng, void *obj);
int rng_put_const(struct rng_s *rng, int val);
int rng_poke(struct rng_s *rng, int idx, void *buf);
int rng_add(struct rng_s *rng, int num, void *buf);
int rng_remove(struct rng_s *rng, int num, void *buf);
int rng_count(struct rng_s *rng);
void rng_clear(struct rng_s *rng);

#endif /* UTILITIES_H */

#ifndef UTILITIES_H
#define UTILITIES_H

#include <sys/types.h>
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
char *concat_dir(const char *s1, const char *s2);
char *mkdir_under(char *basedir, char *dir, int append_pid);
char *get_path_in_HOME(char *path);
char *get_dosemu_local_home(void);
char *readlink_malloc (const char *filename);
void dosemu_error(char *fmt, ...) FORMAT(printf, 1, 2);
void *load_plugin(const char *plugin_name);
void close_plugin(void *handle);

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

#define min(x,y) ({ \
	typeof(x) _x = (x);	\
	typeof(y) _y = (y);	\
	_x < _y ? _x : _y; })

#define max(x,y) ({ \
	typeof(x) _x = (x);	\
	typeof(y) _y = (y);	\
	_x > _y ? _x : _y; })

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

// http://bits.stephan-brumme.com/roundUpToNextPowerOfTwo.html
static inline unsigned int roundUpToNextPowerOfTwo(unsigned int x)
{
  x--;
  x |= x >> 1;  // handle  2 bit numbers
  x |= x >> 2;  // handle  4 bit numbers
  x |= x >> 4;  // handle  8 bit numbers
  x |= x >> 8;  // handle 16 bit numbers
  x |= x >> 16; // handle 32 bit numbers
  x++;

  return x;
}

#define P2ALIGN(x, y) (((x) + (y) - 1) & -(y))
#define ALIGN(x, y) (P2ALIGN(x, roundUpToNextPowerOfTwo(y)))

struct popen2 {
    pid_t child_pid;
    int   from_child, to_child;
};

int popen2(const char *cmdline, struct popen2 *childinfo);
int pclose2(struct popen2 *childinfo);

#endif /* UTILITIES_H */

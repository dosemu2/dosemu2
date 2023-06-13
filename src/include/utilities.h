#ifndef UTILITIES_H
#define UTILITIES_H

#include <sys/types.h>
#include <semaphore.h>
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

char *strprintable(char *s);
char *chrprintable(char c);
int is_printable(const char *s);
void open_proc_scan(const char *name);
void close_proc_scan(void);
char *get_proc_string_by_key(const char *key);
void advance_proc_bufferptr(void);
void reset_proc_bufferptr(void);
int get_proc_intvalue_by_key(const char *key);
int exists_dir(const char *name);
int exists_file(const char *name);
void subst_file_ext(char *ptr);
char *strcatdup(char *s1, char *s2);
int tempname(char *tmpl, size_t x_suffix_len);
char *assemble_path(const char *dir, const char *file);
char *expand_path(const char *dir);
char *concat_dir(const char *s1, const char *s2);
char *mkdir_under(const char *basedir, const char *dir);
char *get_path_in_HOME(const char *path);
char *get_dosemu_local_home(void);
char *readlink_malloc (const char *filename);
void dosemu_error(const char *fmt, ...) FORMAT(printf, 1, 2);
void *load_plugin(const char *plugin_name);
void close_plugin(void *handle);

#define _min(x,y) ({ \
	typeof(x) _x = (x);	\
	typeof(y) _y = (y);	\
	_x < _y ? _x : _y; })

#define _max(x,y) ({ \
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
#ifndef ALIGN
#define ALIGN(x, y) (P2ALIGN(x, roundUpToNextPowerOfTwo(y)))
#endif

struct popen2 {
    pid_t child_pid;
    int   from_child, to_child;
};

int popen2(const char *cmdline, struct popen2 *childinfo);
int popen2_custom(const char *cmdline, struct popen2 *childinfo);
int pclose2(struct popen2 *childinfo);

const char *findprog(const char *prog, const char *path);

#define DLSYM_ASSERT(h, s) ({ \
    void *__sym = dlsym(h, s); \
    if (!__sym) \
        error("dlsym (%s:%i): %s: %s\n", __FILE__, __LINE__, s, dlerror()); \
    assert(__sym); \
    __sym; \
})

//size_t strlcpy(char *dst, const char *src, size_t dsize);
char *strupper(char *src);
char *strlower(char *src);

struct string_store {
    const int num;
    char *strings[0];
};

int replace_string(struct string_store *store, const char *old, char *str);
#ifdef HAVE_FOPENCOOKIE
FILE *fstream_tee(FILE *orig, FILE *copy);
#endif

#define cond_wait(c, m) { \
    pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, m); \
    pthread_cond_wait(c, m); \
    pthread_cleanup_pop(0); \
}

pid_t run_external_command(const char *path, int argc,
        const char **argv,
        int use_stdin, int close_from, int pty_fd, sem_t *pty_sem);

#endif /* UTILITIES_H */

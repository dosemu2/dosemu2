/*
 * Various sundry utilities for dos emulator.
 *
 */
#include "emu.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <limits.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <dlfcn.h>
#include <pthread.h>
#include "wordexp.h"
#ifdef HAVE_LIBACL
#include <sys/acl.h>
#endif
#ifdef HAVE_LIBBSD
#include <bsd/unistd.h> // for closefrom()
#endif

#include "debug.h"
#include "utilities.h"
#include "dos2linux.h"
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif

struct dir_fd {
    const char *dir;
    int fd;
};
#define DFD_MAX 10
static struct dir_fd dfd[DFD_MAX];
static int num_dfd;

static pthread_mutex_t log_mtx = PTHREAD_MUTEX_INITIALIZER;

static char hxtab[16]="0123456789abcdef";

int is_printable(const char *s)
{
  int i;
  int l = strlen(s);

  for (i = 0; i < l; i++) {
    if (!isprint(s[i]))
      return 0;
  }
  return 1;
}

char *strprintable(char *s)
{
  static char buf[8][128];
  static int bufi = 0;
  char *t, c;

  bufi = (bufi + 1) & 7;
  t = buf[bufi];
  while (*s) {
    c = *s++;
    if ((unsigned)c < ' ') {
      *t++ = '^';
      *t++ = c | 0x40;
    }
    else if ((unsigned)c > 126) {
      *t++ = 'X';
      *t++ = hxtab[(c >> 4) & 15];
      *t++ = hxtab[c & 15];
    }
    else *t++ = c;
  }
  *t++ = 0;
  return buf[bufi];
}

char *chrprintable(char c)
{
  char buf[2];
  buf[0] = c;
  buf[1] = 0;
  return strprintable(buf);
}

int log_printf(const char *fmt, ...)
{
	va_list args;
	int ret;

	va_start(args, fmt);
	pthread_mutex_lock(&log_mtx);
	ret = vlog_printf(fmt, args);
	pthread_mutex_unlock(&log_mtx);
	va_end(args);
	return ret;
}

void vprint(const char *fmt, va_list args)
{
  pthread_mutex_lock(&log_mtx);
  if (!config.quiet) {
    va_list copy_args;
    va_copy(copy_args, args);
    vfprintf(real_stderr ?: stderr, fmt, copy_args);
    va_end(copy_args);
  }
  vlog_printf(fmt, args);
  pthread_mutex_unlock(&log_mtx);
}

void verror(const char *fmt, va_list args)
{
  char fmtbuf[1025];

  if (fmt[0] == '@') {
    fmt++;
  } else {
    snprintf(fmtbuf, sizeof(fmtbuf), "ERROR: %s", fmt);
    fmt = fmtbuf;
  }
  vprint(fmt, args);
}

void error(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	verror(fmt, args);
	va_end(args);
}


/* write string to dos? */
static int _dos_vstr(const char *fmt, va_list args, void (*cout)(u_char, int))
{
  char buf[1024];
  char *s;
  int i;

  i = com_vsnprintf(buf, sizeof(buf), fmt, args);
  s = buf;
  g_printf("CONSOLE MSG: '%s'\n",buf);
  while (*s)
	cout(*s++, READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
  return i;
}

int p_dos_vstr(const char *fmt, va_list args)
{
  return _dos_vstr(fmt, args, char_out);
}

int p_dos_str(const char *fmt, ...)
{
  va_list args;
  int i;

  va_start(args, fmt);
  i = p_dos_vstr(fmt, args);
  va_end(args);

  return i;
}

/* some stuff to handle reading of /proc */

static char *procfile_name=0;
static char *procbuf=0;
static char *procbufptr=0;
#define PROCBUFSIZE	4096
static char *proclastpos=0;
static char proclastdelim=0;

void close_proc_scan(void)
{
  if (procfile_name) free(procfile_name);
  if (procbuf) free(procbuf);
  procfile_name = procbuf = procbufptr = proclastpos = 0;
}

int open_proc_scan(const char *name)
{
  int size, fd;
  close_proc_scan();
  fd = open(name, O_RDONLY);
  if (fd == -1) {
    error("cannot open %s\n", name);
    return -1;
  }
  procfile_name = strdup(name);
  procbuf = malloc(PROCBUFSIZE);
  size = read(fd, procbuf, PROCBUFSIZE-1);
  procbuf[size] = 0;
  procbufptr = procbuf;
  close(fd);
  return 0;
}

void advance_proc_bufferptr(void)
{
  if (proclastpos) {
    procbufptr = proclastpos;
    if (proclastdelim == '\n') procbufptr++;
  }
}

void reset_proc_bufferptr(void)
{
  if (proclastpos) {
    *proclastpos = proclastdelim;
    proclastpos =0;
  }
  procbufptr = procbuf;
}

char *get_proc_string_by_key(const char *key)
{

  char *p;

  if (proclastpos) {
    *proclastpos = proclastdelim;
    proclastpos =0;
  }
  p = strstr(procbufptr, key);
  if (p) {
    if ((p == procbufptr) || (p[-1] == '\n')) {
      p += strlen(key);
      while (*p && isblank(*p)) p++;
      if (*p == ':') p++;
      while (*p && isblank(*p)) p++;
      proclastpos = p;
      while (*proclastpos && (*proclastpos != '\n')) proclastpos++;
      proclastdelim = *proclastpos;
      *proclastpos = 0;
      return p;
    }
  }
#if 0
  error("Unknown format on %s\n", procfile_name);
  leavedos(5);
#endif
  return 0; /* just to make GCC happy */
}

int get_proc_intvalue_by_key(const char *key)
{
  char *p = get_proc_string_by_key(key);
  int val;

  if (p) {
    if (sscanf(p, "%d",&val) == 1) return val;
  }
  error("Unknown format on %s\n", procfile_name);
  leavedos(5);
  return -1; /* just to make GCC happy */
}

int exists_dir(const char *name)
{
	struct stat st;
	if (stat(name, &st)) return 0;
	return (S_ISDIR(st.st_mode));
}

int exists_file(const char *name)
{
	struct stat st;
	if (stat(name, &st)) return 0;
	return (S_ISREG(st.st_mode));
}

char *strcatdup(char *s1, char *s2)
{
	char *s;
	if (!s1 || !s2) return 0;
	s = malloc(strlen(s1)+strlen(s2)+1);
	if (!s) return 0;
	strcpy(s,s1);
	return strcat(s,s2);
}

// Concatenate a filename, s1+s2, and return a newly-allocated string of it.
// Watch out for "s1=concat(s1,s2)" if s1 is allocated; if there's not
// another pointer to s1, you won't be able to free s1 later.
static char *_concat_dir(const char *s1, const char *s2)
{
  size_t strlen_s1 = strlen(s1);
  char *_new = malloc(strlen_s1+strlen(s2)+2);
//  debug("concat_dir(%s,%s)", s1, s2);
  strcpy(_new,s1);
  if (s1[strlen_s1 - 1] != '/')
    strcat(_new, "/");
  strcat(_new,s2);
//  debug("->%s\n", _new);
  return _new;
}

char *concat_dir(const char *s1, const char *s2)
{
	if (s2[0] == '/')
		return strdup(s2);
	return _concat_dir(s1, s2);
}

char *assemble_path(const char *dir, const char *file)
{
	char *s;
	wordexp_t p = {};
	int err;

	err = wordexp_lite(dir, &p, WRDE_NOCMD);
	assert(!err);
	assert(p.we_wordc == 1);
	asprintf(&s, "%s/%s", p.we_wordv[0], file);
	wordfree_lite(&p);
	return s;
}

char *expand_path(const char *dir)
{
	char *s;
	wordexp_t p;
	int err;

	err = wordexp_lite(dir, &p, WRDE_NOCMD);
	if (err)
		return NULL;
	if (p.we_wordc != 1) {
		wordfree_lite(&p);
		return NULL;
	}
	s = realpath(p.we_wordv[0], NULL);
	wordfree_lite(&p);
	return s;
}

static int set_dir_acl(int fd)
{
    int ret = 0;
#ifdef HAVE_LIBACL
    acl_t acl_p;
    int err;
    const char *acl_str = "user::rwx\ngroup::r-x\nother::---\nuser:%i:rwx\n";
    char acl_buf[256];

    snprintf(acl_buf, sizeof(acl_buf), acl_str, get_suid());
    acl_p = acl_from_text(acl_buf);
    if (!acl_p) {
        perror("acl_from_text()");
        return -1;
    }

    err = acl_calc_mask(&acl_p);
    if (err) {
        perror("acl_calc_mask()");
        ret = -1;
        goto done;
    }

    if (acl_valid(acl_p) < 0) {
        error("invalid ACL\n");
        ret = -1;
        goto done;
    }

     if (acl_set_fd(fd, acl_p) < 0)
         perror("acl_set_fd");

done:
     acl_free(acl_p);
#endif
     return ret;
}

static int lookup_dfd(const char *dir)
{
	int i;

	for (i = 0; i < num_dfd; i++) {
		if (strcmp(dfd[i].dir, dir) == 0)
			return i;
	}
	return -1;
}

char *mkdir_under(const char *basedir, const char *dir)
{
	char *s;
	int fd;

	if (dir)
		s = assemble_path(basedir, dir);
	else
		s = strdup(basedir);
	if (lookup_dfd(s) != -1) {
		error("dir %s already created\n", s);
		free(s);
		return NULL;
	}
	if (!exists_dir(s)) {
		if (mkdir(s, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)) {
			fprintf(stderr, "can't create dir %s: %s\n", s,
					strerror(errno));
			free(s);
			s = NULL;
		}
	}
	if (!s)
		return NULL;
	fd = open(s, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd == -1) {
		perror("open()");
		free(s);
		return NULL;
	}
	if (running_suid_orig())
		set_dir_acl(fd);

	assert(num_dfd < DFD_MAX);
	dfd[num_dfd].dir = s;
	dfd[num_dfd].fd = fd;
	num_dfd++;
	return s;
}

int unlink_under(const char *dir, const char *fname)
{
	int i = lookup_dfd(dir);
	if (i == -1) {
		error("%s not opened\n", dir);
		return -1;
	}
	return unlinkat(dfd[i].fd, fname, 0);
}

int closedir_under(const char *dir)
{
	int ret;
	int i = lookup_dfd(dir);
	if (i == -1) {
		error("%s not opened\n", dir);
		return -1;
	}
	ret = close(dfd[i].fd);
	dfd[i].fd = -1;
	while (num_dfd && dfd[num_dfd - 1].fd == -1)
		num_dfd--;
	return ret;
}

char *get_path_in_HOME(const char *path)
{
	char *home = getenv("HOME");

	if (!home) {
		fprintf(stderr, "odd environment, you don't have $HOME, giving up\n");
		leavedos(0x45);
	}
	if (!path) {
		return strdup(home);
	}
	return assemble_path(home, path);
}

char *get_dosemu_local_home(void)
{
    int err, errn;
    char *ret = get_path_in_HOME(LOCALDIR_BASE_NAME);
    /* usually this dir is created by wrapper script, but to make sure */
    err = mkdir(ret, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    errn = errno;
    if (err == -1 && errn != EEXIST) {
      error("mkdir(%s): %s\n", ret, strerror(errn));
      free(ret);
      return NULL;
    }
    return ret;
}

char *prefix(const char *suffix)
{
    char *p1, *s, *p;
    char *ret = NULL;

    if (dosemu_proc_self_exe[0] != '/') {
      error("cannot evaluate prefix from relative path %s\n",
          dosemu_proc_self_exe);
      return assemble_path(PREFIX, suffix);
    }
    s = strdup(dosemu_proc_self_exe);
    p = dirname(s);
    assert(p);
    p1 = strrchr(p, '/');
    if (p1) {
      *p1 = '\0';
      if (strcmp(p1 + 1, "bin") == 0)
        ret = assemble_path(p, suffix);
      else if (strcmp(p1 + 1, "dosemu2") == 0) {
        p1 = strrchr(p, '/');
        if (p1 && strcmp(p1 + 1, "libexec") == 0) {
          *p1 = '\0';
          ret = assemble_path(p, suffix);
        }
      }
    }
    if (!ret) {
      error("unable to evaluate prefix from %s\n", dosemu_proc_self_exe);
      ret = assemble_path(PREFIX, suffix);
    }
    free(s);
    return ret;
}

int argparse(char *s, char *argvx[], int maxarg)
{
   int mode = 0;
   int argcx = 0;
   char delim = 0;

   maxarg --;
   for ( ; *s; s++) {
      if (!mode) {
         if (*s > ' ') {
            mode = 1;
            argvx[argcx++] = s;
            switch (*s) {
              case '"':
              case '\'':
                delim = *s;
                mode = 2;
            }
            if (argcx >= maxarg)
               break;
         }
      } else if (mode == 1) {
         if (*s <= ' ') {
            mode = 0;
            *s = 0x00;
         }
      } else {
         if (*s == delim) mode = 1;
      }
   }
   argvx[argcx] = NULL;
   return(argcx);
}

void subst_file_ext(char *ptr)
{
#define ext_fix(s) { char *r=(s); \
		     while (*r) { *r=toupperDOS(*r); r++; } }
    static int subst_sys=2;

    if (ptr == NULL) {
      /* reset */
      subst_sys = 2;
      return;
    }

    /* skip leading drive name and \ */
    if (ptr[1]==':' && ptr[2]=='\\') ptr+=3;
    else if (ptr[0]=='\\') ptr++;

    if (subst_sys && config.emusys) {
        char config_name[6+1+3+1];
#if 0	/*
	 * NOTE: as the method used in fatfs.c can't handle multiple
	 * files to be faked, we can't do it here, because this would
	 * confuse more than doing anything valuable --Hans 2001/03/16
	 */

	/* skip the D for DCONFIG.SYS in DR-DOS */
	if (toupperDOS(ptr[0]) == 'D') ptr++;
#endif
        ext_fix(config.emusys);
        snprintf(config_name, sizeof(config_name), "CONFIG.%-.3s", config.emusys);
        if (subst_sys == 1 && !strequalDOS(ptr, config_name) &&
            !strequalDOS(ptr, "CONFIG.SYS")) {
            subst_sys = 0;
        } else if (strequalDOS(ptr, "CONFIG.SYS")) {
            strcpy(ptr, config_name);
	    d_printf("DISK: Substituted %s for CONFIG.SYS\n", ptr);
	    subst_sys = 1;
	}
    }
}

void sigalarm_onoff(int on)
{
  static struct itimerval itv_old;
#ifdef X86_EMULATOR
  static struct itimerval itv_oldp;
#endif
  static struct itimerval itv;
  static volatile int is_off = 0;
  if (on) {
    if (is_off--) {
	setitimer(ITIMER_REAL, &itv_old, NULL);
#ifdef X86_EMULATOR
	setitimer(ITIMER_VIRTUAL, &itv_oldp, NULL);
#endif
    }
  }
  else if (!is_off++) {
    itv.it_interval.tv_sec = itv.it_interval.tv_usec = 0;
    itv.it_value = itv.it_interval;
    setitimer(ITIMER_REAL, &itv, &itv_old);
#ifdef X86_EMULATOR
    setitimer(ITIMER_VIRTUAL, &itv, &itv_oldp);
#endif
  }
}

/* dynamic readlink, adapted from "info libc" */
char *readlink_malloc (const char *filename)
{
  int size = 50;
  int nchars;
  char *buffer;

  do {
    size *= 2;
    nchars = 0;
    buffer = malloc(size);
    if (buffer != NULL) {
      nchars = readlink(filename, buffer, size);
      if (nchars < 0) {
        free(buffer);
        buffer = NULL;
      }
    }
  } while (nchars >= size);
  if (buffer != NULL)
    buffer[nchars] = '\0';
  return buffer;
}

void dosemu_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    verror(fmt, args);
    va_end(args);
    gdb_debug();
}

#ifdef USE_DL_PLUGINS
static void *do_dlopen(const char *filename, int flags)
{
    void *handle = dlopen(filename, flags | RTLD_NOLOAD);
    if (handle)
	return handle;
    handle = dlopen(filename, flags);
    if (handle)
	return handle;
    error("%s: %s\n", filename, dlerror());
    return NULL;
}

void *load_plugin(const char *plugin_name)
{
    char *fullname;
    char *p;
    void *handle;
    int ret;
    static int warned;

    if (!warned && dosemu_proc_self_exe &&
	    (p = strrchr(dosemu_proc_self_exe, '/'))) {
	asprintf(&fullname, "%.*s/libplugin_%s.so",
		(int)(p - dosemu_proc_self_exe),
		dosemu_proc_self_exe, plugin_name);
	if (access(fullname, R_OK) == 0 &&
			strncmp(fullname, dosemu_plugin_dir_path,
			strlen(dosemu_plugin_dir_path)) != 0) {
		error("running from build dir must be done via script\n");
		warned++;
	}
	free(fullname);

    }
    ret = asprintf(&fullname, "%s/libplugin_%s.so",
	     dosemu_plugin_dir_path, plugin_name);
    assert(ret != -1);
    handle = do_dlopen(fullname, RTLD_LOCAL | RTLD_NOW);
    free(fullname);
    return handle;
}

void close_plugin(void *handle)
{
    dlclose(handle);
}
#else
void *load_plugin(const char *plugin_name)
{
    return NULL;
}
void close_plugin(void *handle)
{
}
#endif

/* http://media.unpythonic.net/emergent-files/01108826729/popen2.c */
int popen2_custom(const char *cmdline, struct popen2 *childinfo)
{
    pid_t p;
    int pipe_stdin[2], pipe_stdout[2];
    sigset_t oset;

    if(pipe(pipe_stdin)) return -1;
    if(pipe(pipe_stdout)) return -1;

//    printf("pipe_stdin[0] = %d, pipe_stdin[1] = %d\n", pipe_stdin[0], pipe_stdin[1]);
//    printf("pipe_stdout[0] = %d, pipe_stdout[1] = %d\n", pipe_stdout[0], pipe_stdout[1]);

    signal_block_async_nosig(&oset);
    p = fork();
    assert(p >= 0);
    if(p == 0) { /* child */
        setsid();	// escape ctty
        close(pipe_stdin[1]);
        dup2(pipe_stdin[0], 0);
        close(pipe_stdin[0]);
        close(pipe_stdout[0]);
        dup2(pipe_stdout[1], 1);
        dup2(pipe_stdout[1], 2);
        close(pipe_stdout[1]);

	/* close signals, then unblock */
	/* SIGIOs should not disturb us because they only go
	 * to the F_SETOWN pid. OTOH disarming SIGIOs will cause this:
	 * https://github.com/stsp/dosemu2/issues/455
	 * ioselect_done(); - not calling.
	 * Instead we mark all SIGIO fds with FD_CLOEXEC.
	 */
	signal_done();
	sigprocmask(SIG_SETMASK, &oset, NULL);

        execl("/bin/sh", "sh", "-c", cmdline, NULL);
        perror("execl");
        _exit(99);
    }
    sigprocmask(SIG_SETMASK, &oset, NULL);
    close(pipe_stdin[0]);
    close(pipe_stdout[1]);
    if (fcntl(pipe_stdin[1], F_SETFD, FD_CLOEXEC) == -1)
      error("fcntl failed to set FD_CLOEXEC '%s'\n", strerror(errno));
    if (fcntl(pipe_stdout[0], F_SETFD, FD_CLOEXEC) == -1)
      error("fcntl failed to set FD_CLOEXEC '%s'\n", strerror(errno));
    childinfo->child_pid = p;
    childinfo->to_child = pipe_stdin[1];
    childinfo->from_child = pipe_stdout[0];
    return 0; 
}

int popen2(const char *cmdline, struct popen2 *childinfo)
{
    int ret = popen2_custom(cmdline, childinfo);
    if (ret)
	return ret;
    ret = sigchld_enable_cleanup(childinfo->child_pid);
    if (ret) {
	error("failed to popen %s\n", cmdline);
	pclose2(childinfo);
	kill(childinfo->child_pid, SIGKILL);
    }
    return ret;
}

int pclose2(struct popen2 *childinfo)
{
    int err;
    if (!childinfo->child_pid)
	return -1;
    err = close(childinfo->from_child);
    err |= close(childinfo->to_child);
    /* kill process too? */
    childinfo->child_pid = 0;
    return err;
}

/*
 * Copyright (c) 1998, 2015 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <string.h>
#if 0
/* disable this and use libbsd */
/*
 * Copy string src to buffer dst of size dsize.  At most dsize-1
 * chars will be copied.  Always NUL terminates (unless dsize == 0).
 * Returns strlen(src); if retval >= dsize, truncation occurred.
 */
size_t
strlcpy(char *dst, const char *src, size_t dsize)
{
    const char *osrc = src;
    size_t nleft = dsize;

    /* Copy as many bytes as will fit. */
    if (nleft != 0) {
	while (--nleft != 0) {
	    if ((*dst++ = *src++) == '\0')
		break;
	}
    }

    /* Not enough room in dst, add NUL and traverse rest of src. */
    if (nleft == 0) {
	if (dsize != 0)
	    *dst = '\0';		/* NUL-terminate dst */
	while (*src++)
	    ;
    }

    return(src - osrc - 1);	/* count does not include NUL */
}
#endif

/* Copyright (c) 1997 Todd C. Miller <Todd.Miller@courtesan.com> */
/* modified by stsp */
const char *findprog(const char *prog, const char *pathc)
{
	static char filename[PATH_MAX];
	char *p;
	char *path;
	char dot[] = ".";
	int proglen, plen;
	struct stat sbuf;
	char *pathcpy;

	/* Special case if prog contains '/' */
	if (strchr(prog, '/')) {
		if ((stat(prog, &sbuf) == 0) && S_ISREG(sbuf.st_mode) &&
		    access(prog, X_OK) == 0) {
			return prog;
		} else {
//			warnx("%s: Command not found.", prog);
			return NULL;
		}
	}

	if (!pathc)
		return NULL;
	path = strdup(pathc);
	assert(path);
	pathcpy = path;

	proglen = strlen(prog);
	while ((p = strsep(&pathcpy, ":")) != NULL) {
		if (*p == '\0')
			p = dot;

		plen = strlen(p);
		while (p[plen-1] == '/')
			p[--plen] = '\0';	/* strip trailing '/' */

		if (plen + 1 + proglen >= sizeof(filename)) {
//			warnx("%s/%s: %s", p, prog, strerror(ENAMETOOLONG));
			free(path);
			return NULL;
		}

		snprintf(filename, sizeof(filename), "%s/%s", p, prog);
		if ((stat(filename, &sbuf) == 0) && S_ISREG(sbuf.st_mode) &&
		    access(filename, X_OK) == 0) {
			free(path);
			return filename;
		}
	}
	(void)free(path);
	return NULL;
}

char *strupper(char *src)
{
  char *s = src;
  for (; *src; src++)
    *src = toupper(*src);
  return s;
}

char *strlower(char *src)
{
  char *s = src;
  for (; *src; src++)
    *src = tolower(*src);
  return s;
}

int replace_string(struct string_store *store, const char *old, char *str)
{
    int i;
    int empty = -1;

    for (i = 0; i < store->num; i++) {
        if (old == store->strings[i]) {
            free(store->strings[i]);
            store->strings[i] = str;
            return 1;
        }
        if (!store->strings[i] && empty == -1)
            empty = i;
    }
    assert(empty != -1);
    store->strings[empty] = str;
    return 0;
}

#ifdef HAVE_FOPENCOOKIE
struct tee_struct {
    FILE *stream;
};

static ssize_t tee_write(void *cookie, const char *buf, size_t size)
{
    struct tee_struct *c = cookie;
    log_printf("%s", buf);
    return fwrite(buf, 1, size, c->stream);
}

static int tee_close(void *cookie)
{
    int ret;
    struct tee_struct *c = cookie;
    ret = fclose(c->stream);
    free(c);
    return ret;
}

static cookie_io_functions_t tee_ops = {
    .write = tee_write,
    .close = tee_close,
};

FILE *fstream_tee(FILE *orig)
{
    FILE *f;
    struct tee_struct *c = malloc(sizeof(struct tee_struct));
    assert(c);
    c->stream = orig;
    f = fopencookie(c, "w", tee_ops);
    assert(f);
    setbuf(f, NULL);
    return f;
}
#endif

static int pts_open(int pty_fd)
{
    int err, pts_fd;

    setsid();	// will have ctty
    /* open pts _after_ setsid, or it won't became a ctty */
    err = grantpt(pty_fd);
    if (err) {
	error("grantpt failed: %s\n", strerror(errno));
	return err;
    }
    pts_fd = open(ptsname(pty_fd), O_RDWR | O_CLOEXEC);
    if (pts_fd == -1) {
	error("pts open failed: %s\n", strerror(errno));
	return -1;
    }
    return pts_fd;
}

int pshared_sem_init(pshared_sem_t *sem, unsigned int value)
{
    char sem_name[] = "/dosemu2_psem_%PXXXXXX";
    pshared_sem_t s;
    int ret;

    tempname(sem_name, 6);
    s = sem_open(sem_name, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, value);
    if (!s)
    {
        /* Stalled sem. Name collision is practically impossible
         * because we unlink the sem immediately after creation,
         * and also we can't collide with another running dosemu2
         * instance because we use PID in the sem name. */
        error("sem_open %s failed %s\n", sem_name, strerror(errno));
        sem_unlink(sem_name);
        s = sem_open(sem_name, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, value);
    }
    if (!s)
    {
        error("sem_open failed %s\n", strerror(errno));
        return -1;
    }
    ret = sem_unlink(sem_name);
    if (!ret)
        *sem = s;
    return ret;
}

int pshared_sem_destroy(pshared_sem_t *sem)
{
    int ret = sem_close(*sem);
    *sem = NULL;
    return ret;
}

pid_t run_external_command(const char *path, int argc, const char **argv,
        int use_stdin, int close_from, int pty_fd)
{
    pid_t pid;
    int wt, retval;
    sigset_t set, oset;
    int pts_fd;
    pshared_sem_t pty_sem;

    retval = pshared_sem_init(&pty_sem, 0);
    assert(!retval);
    signal_block_async_nosig(&oset);
    sigprocmask(SIG_SETMASK, NULL, &set);
    /* fork child */
    switch ((pid = fork())) {
    case -1: /* failed */
	sigprocmask(SIG_SETMASK, &oset, NULL);
	g_printf("run_unix_command(): fork() failed\n");
	return -1;
    case 0: /* child */
	priv_drop();
	pts_fd = pts_open(pty_fd);
	/* Reading master side before slave opened, results in EOF.
	 * Notify user that reads are now safe. */
	pshared_sem_post(pty_sem);
	pshared_sem_destroy(&pty_sem);
	if (pts_fd == -1) {
	    error("run_unix_command(): open pts failed %s\n", strerror(errno));
	    _exit(EXIT_FAILURE);
	}
	close(0);
	close(1);
	close(2);
	if (use_stdin)
	    dup(pts_fd);
	else
	    open("/dev/null", O_RDONLY);
	dup(pts_fd);
	dup(pts_fd);
	close(pts_fd);
	close(pty_fd);
	if (close_from != -1)
#ifdef HAVE_CLOSEFROM
	    closefrom(close_from);
#else
	    for (; close_from < sysconf(_SC_OPEN_MAX); close_from++)
		close(close_from);
#endif
	/* close signals, then unblock */
	signal_done();
	/* flush pending signals */
	do {
#ifdef HAVE_SIGTIMEDWAIT
	    struct timespec to = { 0, 0 };
	    wt = sigtimedwait(&set, NULL, &to);
#else
	    int i;
	    sigset_t pending;
	    sigpending(&pending);
	    wt = -1;
	    for (i = 1; i < SIGMAX; i++)
		if (sigismember(&pending, i) && sigismember(&set, i)) {
		    sigwait(&set, NULL);
		    wt = 0;
		}
#endif
	} while (wt != -1);
	sigprocmask(SIG_SETMASK, &oset, NULL);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
	retval = execve(path, argv, dosemu_envp);	/* execute command */
#pragma GCC diagnostic pop
	error("exec failed: %s\n", strerror(errno));
	_exit(retval);
	break;
    }
    sigprocmask(SIG_SETMASK, &oset, NULL);
    /* wait until its safe to read from pty_fd */
    pshared_sem_wait(pty_sem);
    pshared_sem_destroy(&pty_sem);
    return pid;
}

/* ripped out of glibc. same as mktemp() but doesn't check the file */
/*
   congruential generator that starts with Var's value
   mixed in with a clock's low-order bits if available.  */
typedef uint_fast64_t random_value;
#define RANDOM_VALUE_MAX UINT_FAST64_MAX
#define BASE_62_DIGITS 10 /* 62**10 < UINT_FAST64_MAX */
#define BASE_62_POWER (62LL * 62 * 62 * 62 * 62 * 62 * 62 * 62 * 62 * 62)

static random_value
random_bits (random_value var)
{
  struct timespec tv;
  clock_gettime (CLOCK_MONOTONIC, &tv);
  var ^= tv.tv_nsec;
  return 2862933555777941757 * var + 3037000493;
}

/* These are the characters used in temporary file names.  */
static const char letters[] =
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

int tempname(char *tmpl, size_t x_suffix_len)
{
  int suffixlen = 0;
  size_t len;
  char *XXXXXX, *p;

  /* A random variable.  The initial value is used only the for fallback path
     on 'random_bits' on 'getrandom' failure.  Its initial value tries to use
     some entropy from the ASLR and ignore possible bits from the stack
     alignment.  */
  random_value v = ((uintptr_t) &v) / (sizeof(long));

  /* How many random base-62 digits can currently be extracted from V.  */
  int vdigits = 0;

  /* Least unfair value for V.  If V is less than this, V can generate
     BASE_62_DIGITS digits fairly.  Otherwise it might be biased.  */
  random_value const unfair_min
    = RANDOM_VALUE_MAX - RANDOM_VALUE_MAX % BASE_62_POWER;

  len = strlen (tmpl);
  if (len < x_suffix_len + suffixlen
      || strspn (&tmpl[len - x_suffix_len - suffixlen], "X") < x_suffix_len)
    {
      return -1;
    }

  if ((p = strstr(tmpl, "%PX"))) {
    /* reserve at least 1 X */
    int plen = snprintf(p, x_suffix_len + 2, "%02i", getpid());
    if (plen >= x_suffix_len + 2)
      return -1;
    assert(p[plen] == '\0'); // snprintf's trailing 0
    p[plen] = 'X';
    assert(plen >= 2);
    x_suffix_len -= plen - 2;
  }

  /* This is where the Xs start.  */
  XXXXXX = &tmpl[len - x_suffix_len - suffixlen];

    {
      for (size_t i = 0; i < x_suffix_len; i++)
        {
          if (vdigits == 0)
            {
              do
                {
                  v = random_bits (v);
                }
              while (unfair_min <= v);

              vdigits = BASE_62_DIGITS;
            }

          XXXXXX[i] = letters[v % 62];
          v /= 62;
          vdigits--;
        }
    }

  return 0;
}

int mktmp_in(const char *dir_tmpl, const char *fname, mode_t mode,
    char *dir_name, int dir_name_len)
{
  int fd;
  char *p, *d;;
  int wr, err;

  wr = snprintf(dir_name, dir_name_len, "%s/%s", dosemu_tmpdir, dir_tmpl);
  assert(wr < dir_name_len);
  d = mkdtemp(dir_name);
  if (!d)
    return -1;
  err = chmod(d, S_IRWXU | S_IRWXG);
  assert(!err);
  p = assemble_path(d, fname);
  fd = open(p, O_CREAT | O_RDWR | O_EXCL, mode);
  free(p);
  return fd;
}

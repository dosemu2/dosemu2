/*
 * DOSEMU debugger,  1995 Max Parke <mhp@light.lightlink.com>
 *
 * This is file dosdebug.c
 *
 * Terminal client for DOSEMU debugger v0.2
 * by Hans Lermen <lermen@elserv.ffm.fgan.de>
 * It uses /var/run/dosemu.dbgXX.PID for connections.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>	/* for struct timeval */
#include <time.h>	/* for CLOCKS_PER_SEC */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <sys/ioctl.h>
#ifdef HAVE_LIBREADLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "utilities.h"

#define    TMPFILE_VAR		"/var/run/dosemu."
#define    TMPFILE_HOME		".dosemu/run/dosemu."

#define MHP_BUFFERSIZE 8192

#define FOREVER ((((unsigned int)-1) >> 1) / CLOCKS_PER_SEC)
#define KILL_TIMEOUT 2

int kill_timeout=FOREVER;

int fdconin, fddbgin, fddbgout;
FILE *fpconout;
int running;

static int check_pid(int pid);

static int find_dosemu_pid(const char *tmpfile, int local)
{
  DIR *dir;
  struct dirent *p;
  char *dn, *id;
  int i,j,pid=-1;
  static int once =1;

  dn = strdup(tmpfile);
  j=i=strlen(dn);
  while (dn[i--] != '/');  /* remove 'dosemu.dbgin' */
  i++;
  dn[i++]=0;
  id=dn+i;
  j-=i;

  dir = opendir(dn);
  if (!dir) {
    if (local) {
      free(dn);
      return -1;
    }
    fprintf(stderr, "can't open directory %s\n",dn);
    free(dn);
    exit(1);
  }
  i = 0;
  while((p = readdir(dir))) {

    if(!strncmp(id,p->d_name,j) && p->d_name[j] >= '0' && p->d_name[j] <= '9') {
      pid = strtol(p->d_name + j, 0, 0);
      if(check_pid(pid)) {
        if(once && i++ == 1) {
          fprintf(stderr,
            "Multiple dosemu processes running or stalled files in %s\n"
            "restart dosdebug with one of the following pids as first arg:\n"
            "%d", dn, pid
          );
          once = 0;
        }
      }
      if (i > 1) fprintf(stderr, " %d", pid);
    }
  }
  free(dn);
  closedir(dir);
  if (i > 1) {
    fprintf(stderr, "\n");
    if (local) return -1;
    exit(1);
  }
  if (!i) {
    if (local) return -1;
    fprintf(stderr, "No dosemu process running, giving up.\n");
    exit(1);
  }

  return pid;
}

static int check_pid(int pid)
{
  int fd;
  char buf[32], name[128];

  memset(buf, 0, sizeof(buf));
  sprintf(name, "/proc/%d/stat", pid);
  if ((fd = open(name, O_RDONLY)) ==-1) return 0;
  if (read(fd, buf, sizeof(buf)-1) <=0) {
    close(fd);
    return 0;
  }
  close(fd);
  return strstr(buf,"dos") ? 1 : 0;
}

typedef int localfunc_t(char *);

// for readline completion
typedef struct {
  const char *name;   /* User printable name of the function. */
  localfunc_t *func;  /* Function to call if implemented locally */
  const char *doc;    /* Documentation for this function.  */
} COMMAND;

static localfunc_t db_help;
static localfunc_t db_quit;
static localfunc_t db_kill;

static COMMAND cmds[] = {
  {"r", NULL,
   "[REG val]         show regs OR set reg to value\n"
   "                               val can be specified as for modify memory except\n"
   "                               that string values are not supported\n"},
  {"r32", NULL,
   "                  show regs in 32 bit format\n"},
  {"m", NULL,
   "ADDR val [val ..] modify memory (0-1Mb), previous addr for ADDR='-'\n"
   "                               val can be:\n"
   "                                 integer (default decimal)\n"
   "                                 integer (prefixed with '0x' for hexadecimal)\n"
   "                                 integer (prefixed with '\\x' for hexadecimal)\n"
   "                                 integer (prefixed with '\\o' for octal)\n"
   "                                 integer (prefixed with '\\b' for binary)\n"
   "                                 character constant (e.g. 'a')\n"
   "                                 string (\"abcdef\")\n"
   "                                 register symbolic and has its size\n"
   "                               Except for strings and registers, val can be suffixed\n"
   "                               by W(word) or L(dword), default size is byte.\n"},
  {"d", NULL,
   "ADDR SIZE         dump memory (limit 256 bytes)\n"},
  {"dump", NULL,
   "ADDR SIZE FILE    dump memory to file (binary)\n"},
  {"ivec", NULL,
   "[hexnum]          display interrupt vector hexnum (default whole table)\n"},
  {"u", NULL,
   "ADDR SIZE         unassemble memory (limit 256 bytes)\n"},
  {"g", NULL,
   "                  go (if stopped)\n"},
  {"stop", NULL,
   "                  stop (if running)\n"},
  {"mode", NULL,
   "0|1|2|+d|-d       set mode (0=SEG16, 1=LIN32, 2=UNIX32) for u and d commands\n"},
  {"t", NULL,
   "                  single step\n"},
  {"ti", NULL,
   "                  single step into interrupt\n"},
  {"tc", NULL,
   "                  single step, loop forever until key pressed\n"},
  {"bl", NULL,
   "                  list active breakpoints\n"},
  {"bp", NULL,
   "ADDR              set int3 style breakpoint\n"},
  {"bc", NULL,
   "n                 clear breakpoint #n (as listed by bl)\n"},
  {"bpint", NULL,
   "xx                set breakpoint on INT xx\n"},
  {"bcint", NULL,
   "xx                clear breakpoint on INT xx\n"},
  {"bpintd", NULL,
   "xx [ax]           set breakpoint on DPMI INT xx [ax]\n"},
  {"bcintd", NULL,
   "xx [ax]           clear breakpoint on DPMI INT xx [ax]\n"},
  {"bpload", NULL,
   "                  stop at start of next loaded DOS program\n"},
  {"bplog", NULL,
   "REGEX             set breakpoint on logoutput using regex\n"},
  {"bclog", NULL,
   "REGEX             clear breakpoint on logoutput using regex\n"},
  {"rusermap", NULL,
   "org FILE          read MS linker format .MAP file at code origin = 'org'.\n"},
  {"rusermap", NULL,
   "list              list the currently loaded user symbols\n"},
  {"ldt", NULL,
   "[sel]             dump ldt page or specific entry for selector 'sel'\n"},
  {"log", NULL,
   "[on | off | info | FLAGS ] get/set debug-log flags (e.g 'log +M-k')\n"},
  {"mcbs", NULL,
   "                  display MCBs by walking the chain\n"},
  {"devs", NULL,
   "                  display DEVICEs by walking the chain\n"},
  {"kill", db_kill,
   "                  Kill the dosemu process\n"},
  {"quit", db_quit,
   "                  Quit the debug session\n"},
  {"help", db_help,
   "                  Show this help\n"},
  {"?", db_help,
   "                  Synonym for help\n"},
  {"", NULL,
   "<ENTER>           Repeats previous command\n"},
  {NULL, NULL, NULL}
};

static COMMAND *find_cmd(char *name)
{
  int i;
  char *tmp, *p;

  tmp = strdup(name);
  if (!tmp)
    return NULL;

  p = strchr(tmp, ' ');
  if (p)
    *p = '\0';

  for (i = 0; cmds[i].name; i++)
    if (strcmp(tmp, cmds[i].name) == 0) {
      free(tmp);
      return (&cmds[i]);
    }

  free(tmp);
  return NULL;
}

static int db_quit(char *line) {
  running = 0;
  return 1;  // done
}

static int db_kill(char *line) {
  kill_timeout = KILL_TIMEOUT;
  return 0;  // need caller to send the line to dosemu
}

static int db_help(char *line) {
  int i;

  fputs("\n", fpconout);
  for (i = 0; cmds[i].name; i++) {
    if (cmds[i].doc)
      fprintf(fpconout, "%-10s %s", cmds[i].name, cmds[i].doc);
  }

  fflush(fpconout);
  return 1;  // done
}

#ifdef HAVE_LIBREADLINE
static char *db_cmd_generator(const char *text, int state) {
  static int list_index, len;

  const char *name;

  /* If this is a new word to complete, initialize index to 0 and save the
   * length of TEXT for efficiency */
  if (!state) {
    list_index = 0;
    len = strlen(text);
  }

  /* Return the next name which partially matches from the command list. */
  while ((name = cmds[list_index].name)) {
    list_index++;

    if (strncmp(name, text, len) == 0)
      return strdup(name);
  }

  return NULL;
}

static char **db_completion(const char *text, int start, int end)
{
  char **matches = NULL;

  if (start == 0) {  // only do 1st level completion
    matches = rl_completion_matches(text, db_cmd_generator);
  }

  // If we return NULL then the default file generator is called unless
  // we set the following
  rl_attempted_completion_over = 1;
  return matches;
}

static void handle_console_input(char *);

/*
 * Callback function called for each line when accept-line executed, EOF
 * seen, or EOF character read.
 */
static void rl_console_callback(char *line)
{
  handle_console_input(line);
  if (line)
    free(line);
}
#endif

static void handle_console_input(char *line)
{
  static char last_line[MHP_BUFFERSIZE];

  COMMAND *cmd;
  int len;
  char *p;

  if (!line) { // Ctrl-D
    fputs("\n", fpconout);
    fflush(fpconout);
    db_quit(NULL);
    return;
  }

  /* Check if command valid */
  cmd = find_cmd(line);
  if (!cmd) {
    fprintf(fpconout, "Command '%s' not implemented\n", line);
    return;
  }

  /* Update or use history */
  if (*line) {
#ifdef HAVE_LIBREADLINE
    add_history(line);
#endif
    snprintf(last_line, sizeof(last_line), "%s", line);
    if ((strncmp(last_line, "d ", 2) == 0) ||
        (strncmp(last_line, "u ", 2) == 0) ){
      last_line[1] = '\0';
    }
    p = line;      // line okay to execute

  } else {
    cmd = find_cmd(last_line);
    if (!cmd) {
      last_line[0] = '\0';
      return;
    }
    p = last_line; // replace with valid last line
  }

  /* Maybe it's implemented locally (returns 1 if completely handled) */
  if (cmd->func && cmd->func(p)) {
    return;
  }

  /* Pass to dosemu */
  len = strlen(p);
  if (write(fddbgout, p, len) != len) {
    fprintf(fpconout, "write to pipe failed\n");
  }
}

/* returns 0: done, 1: more to do */
static int handle_dbg_input(int *retval)
{
  char buf[MHP_BUFFERSIZE], *p;
  int n;

  do {
    n=read(fddbgin, buf, sizeof(buf));
  } while (n < 0 && errno == EAGAIN);

  if (n > 0) {
    if ((p = memchr(buf, 1, n)) != NULL) {
      fprintf(fpconout, "\nDosemu process ended - quitting\n");
      fflush(fpconout);
      *retval = 0;
      return 0;
    }

    fputs("\n", fpconout);
    fwrite(buf, 1, n, fpconout);
    fflush(fpconout);
#ifdef HAVE_LIBREADLINE
    rl_on_new_line();
    rl_redisplay();
#endif
  }
  if (n == 0) {
    *retval = 1;
    return 0;
  }
  return 1;
}


int main (int argc, char **argv)
{
  fd_set readfds;
  int numfds, dospid, ret;
  char *home_p;
  char *pipename_in, *pipename_out;
  struct timeval timeout;

  FD_ZERO(&readfds);

  home_p = getenv("HOME");

  if (!argv[1]) {
    dospid = -1;
    if (home_p) {
      char *dosemu_tmpfile_pat;

      ret = asprintf(&dosemu_tmpfile_pat, "%s/" TMPFILE_HOME "dbgin.", home_p);
      assert(ret != -1);

      dospid=find_dosemu_pid(dosemu_tmpfile_pat, 1);
      free(dosemu_tmpfile_pat);
    }
    if (dospid == -1) {
      dospid=find_dosemu_pid(TMPFILE_VAR "dbgin.", 0);
    }
  }
  else dospid=strtol(argv[1], 0, 0);

  if (!check_pid(dospid)) {
    fprintf(stderr, "no dosemu running on pid %d\n", dospid);
    exit(1);
  }

  /* NOTE: need to open read/write else O_NONBLOCK would fail to open */
  fddbgout = -1;
  if (home_p) {
    ret = asprintf(&pipename_in, "%s/%sdbgin.%d", home_p, TMPFILE_HOME, dospid);
    assert(ret != -1);

    ret = asprintf(&pipename_out, "%s/%sdbgout.%d", home_p, TMPFILE_HOME, dospid);
    assert(ret != -1);

    fddbgout = open(pipename_in, O_RDWR | O_NONBLOCK);
    if (fddbgout == -1) {
      free(pipename_in);
      free(pipename_out);
    }
  }
  if (fddbgout == -1) {
    /* if we cannot open pipe and we were trying $HOME/.dosemu/run directory,
       try with /var/run/dosemu directory */
    ret = asprintf(&pipename_in, TMPFILE_VAR "dbgin.%d", dospid);
    assert(ret != -1);

    ret = asprintf(&pipename_out, TMPFILE_VAR "dbgout.%d", dospid);
    assert(ret != -1);

    fddbgout = open(pipename_in, O_RDWR | O_NONBLOCK);
  }

  if (fddbgout == -1) {
    perror("can't open output fifo");
    free(pipename_in);
    free(pipename_out);
    exit(1);
  }

  if ((fddbgin = open(pipename_out, O_RDONLY | O_NONBLOCK)) == -1) {
    close(fddbgout);
    perror("can't open input fifo");
    free(pipename_in);
    free(pipename_out);
    exit(1);
  }

#ifdef HAVE_LIBREADLINE
  /* So that we can use conditional ~/.inputrc commands */
  rl_readline_name = "dosdebug";

  /* Install the readline completion function */
  rl_attempted_completion_function = db_completion;

  /* Install the readline handler. */
  rl_callback_handler_install("dosdebug> ", rl_console_callback);

  fdconin = fileno(rl_instream);
  fpconout = rl_outstream;
#else
  fdconin = STDIN_FILENO;
  fpconout = stdout;
#endif

  write(fddbgout,"r0\n",3);

  for (running=1, ret=0; running; /* */) {
    FD_SET(fddbgin, &readfds);
    FD_SET(fdconin, &readfds);
    timeout.tv_sec=kill_timeout;
    timeout.tv_usec=0;

    /* only scan the minimum number of fds */
    numfds=select(((fddbgin > fdconin) ? fddbgin : fdconin) + 1,
                   &readfds,
                   NULL /*no writefds*/,
                   NULL /*no exceptfds*/, &timeout);
    if (numfds > 0) {
      if (FD_ISSET(fdconin, &readfds)) {
#ifdef HAVE_LIBREADLINE
        rl_callback_read_char();
#else
        int num;
        char buf[MHP_BUFFERSIZE], *p;

        num = read(fdconin, buf, sizeof(buf) - 1);
        if (num < 0)
          break;

        if (num == 0) {
          p = NULL;
        } else {
          buf[num] = '\0';
          p = strpbrk(buf, "\r\n");
          if (p) {
            *p = '\0';
            p = buf;
          }
        }

        handle_console_input(p);
#endif

        if (!running) {
          fputs("\n", fpconout);
          fflush(fpconout);
          break;
	}
      }

      if (FD_ISSET(fddbgin, &readfds))
        if (!handle_dbg_input(&ret))
          break;

    } else {
      if (kill_timeout != FOREVER) {
        if (kill_timeout > KILL_TIMEOUT) {
          struct stat st;
          if (stat(pipename_in, &st) != -1) {
            fprintf(fpconout, "no reaction, using kill SIGKILL\n");
            kill(dospid, SIGKILL);
            unlink(pipename_in);
            unlink(pipename_out);
            fprintf(fpconout, "dosemu process (pid %d) was killed\n", dospid);
          } else {
            fprintf(fpconout, "dosemu process (pid %d) was terminated\n", dospid);
	  }
          ret = 1;
          break;
        }
        fprintf(fpconout, "no reaction, trying kill SIGTERM\n");
        kill(dospid, SIGTERM);
        kill_timeout += KILL_TIMEOUT;
      }
    }
  }

  free(pipename_in);
  free(pipename_out);

#ifdef HAVE_LIBREADLINE
  rl_callback_handler_remove();
#endif
  return ret;
}

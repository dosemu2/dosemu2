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
#include "dosemu_config.h"

#define    TMPFILE_VAR		"%s/dosemu2/dosemu."

#define MHP_BUFFERSIZE 8192
/* End of the reply to one command; see MHP_EOR in mhpdbg.h */
#define MHP_EOR 0x00

#define FOREVER ((((unsigned int)-1) >> 1) / CLOCKS_PER_SEC)
#define KILL_TIMEOUT 2

int kill_timeout=FOREVER;

const char *prompt = "dosdebug> ";

int fdconin, fddbgin, fddbgout;
FILE *fpconout;
static int running;
/* commands sent to dosemu whose reply has not arrived in full yet */
static int pending;
/* output arrived that no reply marker ended, so the prompt is held back
 * until the burst goes quiet */
static int prompt_held;
/* commands are being read from a file rather than typed */
static FILE *batchfp;
/* how long a batch command may take before we give up on it, in seconds */
#define BATCH_TIMEOUT 30
/* how long '@wait' lets a report dosemu sends on its own settle, in seconds */
#define BATCH_SETTLE 2
/* how long to wait for the rest of such a burst, in microseconds */
#define PROMPT_SETTLE 200000

static int find_dosemu_pid(const char *tmpfile, int local)
{
  DIR *dir;
  struct dirent *p;
  char *dn, *id;
  int i, j, pid = 0;
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
      int fd = openat(dirfd(dir), p->d_name, O_WRONLY | O_NONBLOCK);
      if (fd == -1)
        continue;  // no reader on that fifo
      close(fd);
      pid = strtol(p->d_name + j, 0, 0);
      if (pid) {
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
   "ADDR val [val ..] modify memory at address ADDR ('-' for previous addr)\n"
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
  {"ivec", NULL,
   "[hexnum]          display interrupt vector hexnum (default whole table)\n"},
  {"u", NULL,
   "ADDR SIZE         unassemble memory (limit 256 bytes)\n"},
  {"g", NULL,
   "                  go (if stopped)\n"},
  {"stop", NULL,
   "                  stop (if running)\n"},
  {"mode", NULL,
   "0|1|2|d|+d|-d     set mode (0=SEG16, 1=LIN32, 2=UNIX32) for u and d commands\n"},
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
  {"usermap", NULL,
   "load-ms FILE [org]   read MS linker format .MAP file at code origin = 'org'.\n"},
  {"usermap", NULL,
   "load-gnu FILE [org]   read Gnu ld linker format .MAP file at code origin = 'org'.\n"},
  {"usermap", NULL,
   "list              list the currently loaded user symbols\n"},
  {"usermap", NULL,
   "clear             clear all user symbols\n"},
  {"symbol", NULL,
   "[ADDR]            Find the previous symbol to current CS:IP or ADDR\n"},
  {"ldt", NULL,
   "[sel]             dump ldt page or specific entry for selector 'sel'\n"},
  {"log", NULL,
   "[on | off | info | FLAGS ] get/set debug-log flags (e.g 'log +M-k')\n"},
  {"mcbs", NULL,
   "                  display MCBs by walking the chain\n"},
  {"devs", NULL,
   "                  display DEVICEs by walking the chain\n"},
  {"ddrh", NULL,
   "ADDR              display the Device Driver Request Header at ADDR\n"},
  {"dpbs", NULL,
   "[ADDR]            display DPBs by walking the chain from LOL or ADDR\n"},
  {"injchar", NULL,
   "ASCII_CODE        inject character to console\n"},
  {"hookcbrk", NULL,
   "[on | off]        hook ^break handling\n"},
  {"dosbreak", NULL,
   "                  command for testing\n"},
  {"reboot", NULL,
   "                  reboot dosemu\n"},
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
    if (!batchfp)
      add_history(line);
#endif
    snprintf(last_line, sizeof(last_line), "%s", line);
    if ((strncmp(last_line, "d ", 2) == 0) ||
        (strncmp(last_line, "u ", 2) == 0) ||
        (strncmp(last_line, "tc", 2) == 0) ){
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
  if (dprintf(fddbgout, "%s\n", p) == -1) {
    fprintf(fpconout, "write to pipe failed (%s)\n", strerror(errno));
    return;
  }

  /* Hold the prompt back until dosemu is done with this command. Printing
   * it right away puts it ahead of the output it belongs to, which leaves
   * a script reading up to the prompt with the previous command's reply. */
  pending++;
  prompt_held = 0;
#ifdef HAVE_LIBREADLINE
  if (!batchfp)
    rl_set_prompt("");
#endif
}

/* returns 0: done, 1: more to do */
static int handle_dbg_input(int *retval)
{
  char buf[MHP_BUFFERSIZE];
  int n, i, len, saw_eor;

  *retval = 0;
  n = read(fddbgin, buf, sizeof(buf));

  if (n > 0) {
    if (memchr(buf, 1, n) != NULL) {
      fprintf(fpconout, "\nDosemu process ended - quitting\n");
      fflush(fpconout);
      *retval = 0;
      return 0;
    }

#ifdef HAVE_LIBREADLINE
    char *saved_line = NULL;
    int saved_point = 0;
    if (!batchfp) {
      saved_point = rl_point;
      saved_line = rl_copy_text(0, rl_end);
      rl_set_prompt("");
      rl_replace_line("", 0);
      rl_redisplay();
    }
#endif

    /* Split at the end of reply markers. What precedes one completes the
     * reply to a command, so the prompt goes out right behind it, once
     * per command; a chunk with no marker is a reply still coming in. */
    saw_eor = 0;
    for (i = 0; i < n; i += len + 1) {
      char *eor = memchr(buf + i, MHP_EOR, n - i);

      len = eor ? eor - (buf + i) : n - i;
      if (len) {
        fwrite(buf + i, 1, len, fpconout);
        fputs("\n", fpconout);
      }
      if (!eor)
        break;
      saw_eor = 1;
      if (pending)
        pending--;
#ifdef HAVE_LIBREADLINE
      if (!batchfp) {
        rl_set_prompt(prompt);
        rl_redisplay();
        rl_set_prompt("");
      }
#endif
    }
    fflush(fpconout);

    /* A report dosemu sends on its own, such as the one it prints when it
     * stops at a breakpoint, is written a line at a time and arrives in as
     * many chunks, none of them carrying a reply marker. Printing a prompt
     * behind every one of them puts prompts in the middle of the report,
     * so hold it back until the burst goes quiet instead. */
    prompt_held = (!pending && !saw_eor);

#ifdef HAVE_LIBREADLINE
    if (!batchfp) {
      rl_set_prompt((pending || prompt_held) ? "" : prompt);
      rl_replace_line(saved_line, 0);
      rl_point = saved_point;
      rl_redisplay();
      free(saved_line);
    }
#endif
  }

  if (n == 0) {
    *retval = 1;
    return 0;
  }
  if (n == -1) {
    *retval = 1;
    return 1;
  }
  return 1;
}


/* Read from dosemu until every command we sent has been answered, which is
 * what the reply markers are for, or until 'secs' pass with nothing.
 * Returns 0 when dosemu went away or stopped answering. */
static int batch_await_replies(int secs)
{
  time_t deadline = time(NULL) + secs;
  int ret;

  while (pending) {
    fd_set rfds;
    struct timeval tv;
    time_t now = time(NULL);

    if (now >= deadline) {
      fprintf(fpconout, "dosdebug: no reply in %d seconds, giving up\n", secs);
      return 0;
    }
    FD_ZERO(&rfds);
    FD_SET(fddbgin, &rfds);
    tv.tv_sec = deadline - now;
    tv.tv_usec = 0;
    if (select(fddbgin + 1, &rfds, NULL, NULL, &tv) <= 0)
      continue;
    if (!handle_dbg_input(&ret))
      return 0;
  }
  return 1;
}

/* Wait for a report dosemu sends on its own, the one it prints when it stops,
 * and for it to finish arriving.  This is what a script needs after a 'g':
 * the reply to 'g' comes back at once, the stop comes whenever it comes. */
static int batch_await_report(int secs)
{
  time_t deadline = time(NULL) + secs;
  int seen = 0, ret;

  for (;;) {
    fd_set rfds;
    struct timeval tv;
    time_t now = time(NULL);

    if (!seen && now >= deadline)
      return 1;                 /* nothing stopped; the script may mean that */
    FD_ZERO(&rfds);
    FD_SET(fddbgin, &rfds);
    tv.tv_sec = seen ? BATCH_SETTLE : deadline - now;
    tv.tv_usec = 0;
    if (select(fddbgin + 1, &rfds, NULL, NULL, &tv) <= 0) {
      if (seen)
        return 1;               /* it arrived and has gone quiet */
      continue;
    }
    if (!handle_dbg_input(&ret))
      return 0;
    seen = 1;
  }
}

/* Run the commands in the batch file, one at a time, each one waited for.
 * Returns the exit status. */
static int batch_run(void)
{
  char line[MHP_BUFFERSIZE];

  /* the r0 sent at startup is already outstanding; its reply is the banner */
  if (!batch_await_replies(BATCH_TIMEOUT))
    return 1;

  while (running && fgets(line, sizeof(line), batchfp)) {
    char *p = line, *e;

    e = strpbrk(p, "\r\n");
    if (e)
      *e = '\0';
    while (*p == ' ' || *p == '\t')
      p++;
    if (!*p || *p == '#')       /* blank line or comment */
      continue;
    if (!strncmp(p, "@wait", 5) && (!p[5] || p[5] == ' ')) {
      int secs = p[5] ? atoi(p + 6) : BATCH_TIMEOUT;

      if (secs <= 0)
        secs = BATCH_TIMEOUT;
      if (!batch_await_report(secs))
        return 1;
      continue;
    }
    handle_console_input(p);
    if (!batch_await_replies(BATCH_TIMEOUT))
      return 1;
  }

  if (running) {
    char quitcmd[] = "quit";

    /* detach without leaving dosemu stopped, the way 'quit' does */
    handle_console_input(quitcmd);
    batch_await_replies(BATCH_TIMEOUT);
  }
  return 0;
}

int main (int argc, char **argv)
{
  fd_set readfds;
  int numfds, dospid, ret, argi = 1;
  char *pipename_in, *pipename_out;
  struct timeval timeout;
  const char *rp = getenv("XDG_RUNTIME_DIR");

  while (argi < argc && argv[argi][0] == '-' && argv[argi][1]) {
    if (!strcmp(argv[argi], "-c") && argi + 1 < argc) {
      const char *fname = argv[++argi];

      batchfp = strcmp(fname, "-") ? fopen(fname, "r") : stdin;
      if (!batchfp) {
        fprintf(stderr, "dosdebug: can't read %s: %s\n", fname,
                strerror(errno));
        exit(1);
      }
      argi++;
    } else {
      fprintf(stderr, "usage: dosdebug [-c file] [pid]\n"
              "  -c file   read commands from 'file' ('-' for stdin), run\n"
              "            them in order and exit\n");
      exit(1);
    }
  }

  if (!rp || !rp[0]) {
    perror("XDG_RUNTIME_DIR unset or empty");
    exit(1);
  }

  FD_ZERO(&readfds);

  /* the r0 sent below is outstanding from the start, and its reply is
   * what carries the banner, so the first prompt waits for it too */
  pending = 1;

  if (argi >= argc) {
    char fname[256];

    snprintf(fname, sizeof(fname), TMPFILE_VAR "dbgin.", rp);
    dospid = find_dosemu_pid(fname, 0);
  } else
    dospid = strtol(argv[argi], 0, 0);

  /* NOTE: need to open read/write else O_NONBLOCK would fail to open */
  ret = asprintf(&pipename_in, TMPFILE_VAR "dbgin.%d", rp, dospid);
  assert(ret != -1);

  ret = asprintf(&pipename_out, TMPFILE_VAR "dbgout.%d", rp, dospid);
  assert(ret != -1);

  fddbgout = open(pipename_in, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
  if (fddbgout == -1) {
    perror("can't open output fifo");
    free(pipename_in);
    free(pipename_out);
    exit(1);
  }

  if ((fddbgin = open(pipename_out, O_RDONLY | O_NONBLOCK | O_CLOEXEC)) == -1) {
    close(fddbgout);
    perror("can't open input fifo");
    free(pipename_in);
    free(pipename_out);
    exit(1);
  }

#ifdef HAVE_LIBREADLINE
  if (!batchfp) {
    /* So that we can use conditional ~/.inputrc commands */
    rl_readline_name = "dosdebug";

    /* Install the readline completion function */
    rl_attempted_completion_function = db_completion;

    /* Install the readline handler. The prompt starts out empty: the r0
     * below is already outstanding, and its reply carries the banner. */
    rl_callback_handler_install("", rl_console_callback);

    fdconin = fileno(rl_instream);
    fpconout = rl_outstream;
  } else {
    fdconin = STDIN_FILENO;
    fpconout = stdout;
  }
#else
  fdconin = STDIN_FILENO;
  fpconout = stdout;
#endif

  if (write(fddbgout, "r0\n", 3) != 3) {
    fprintf(fpconout, "write to pipe failed\n");
    exit(1);
  }

  if (batchfp) {
    running = 1;
    ret = batch_run();
    if (batchfp != stdin)
      fclose(batchfp);
    free(pipename_in);
    free(pipename_out);
    return ret;
  }

  for (running=1, ret=0; running; /* */) {
    FD_SET(fddbgin, &readfds);
    FD_SET(fdconin, &readfds);
    if (prompt_held) {
      timeout.tv_sec = 0;
      timeout.tv_usec = PROMPT_SETTLE;
    } else {
      timeout.tv_sec = kill_timeout;
      timeout.tv_usec = 0;
    }

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
          /* collect all remaining input */
          do
            usleep(100000);
          while (handle_dbg_input(&ret) && ret == 0);
          fputs("\n", fpconout);
          fflush(fpconout);
          break;
        }
      }

      if (FD_ISSET(fddbgin, &readfds))
        if (!handle_dbg_input(&ret))
          break;

    } else {
      if (prompt_held) {
        /* the burst is over, so the prompt can go out now */
        prompt_held = 0;
#ifdef HAVE_LIBREADLINE
        rl_set_prompt(prompt);
        rl_redisplay();
#endif
        continue;
      }
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

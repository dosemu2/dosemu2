#include "emu.h"
#include "dosemu_config.h"
#include "debug.h"
#include "emudpmi.h"
#include "cpu-emu.h"
#include "sig.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
#include "execinfo_wrp.h"

static FILE *gdb_f = NULL;

static void gdb_command(const char *cmd)
{
  printf("%s", cmd);
  fflush(stdout);
  fprintf(gdb_f, "%s", cmd);
  fflush(gdb_f);
}

static int start_gdb(pid_t dosemu_pid)
{
  char *buf;
  int ret;

  printf("Debug info:\n");
  fflush(stdout);

#ifdef __APPLE__
  ret = asprintf(&buf, "lldb %s", dosemu_proc_self_exe);
#else
  ret = asprintf(&buf, "gdb --readnow %s", dosemu_proc_self_exe);
#endif
  assert(ret != -1);

  printf("%s", buf);
  putchar('\n');
  fflush(stdout);

  if (!(gdb_f = popen(buf, "w"))) {
    free(buf);
    return 0;
  }
  free(buf);

  ret = asprintf(&buf, "attach %i\n", dosemu_pid);
  assert(ret != -1);

  gdb_command(buf);
  free(buf);

  return 1;
}

static void do_debug(void)
{
#ifdef __APPLE__
  const char *cmd1 = "register read\n";
//  const char *cmd2 = "bt\n";
  const char *cmd3 = "thread backtrace all\n";
#else
  const char *cmd1 = "info registers\n";
//  const char *cmd2 = "backtrace\n";
  const char *cmd3 = "thread apply all backtrace full\n";
#endif

  gdb_command(cmd1);
//  gdb_command(cmd2);
  gdb_command(cmd3);
}

static int stop_gdb(void)
{
  const char *cmd1 = "detach\n";
  const char *cmd2 = "quit\n";
  int status;

  gdb_command(cmd1);
  gdb_command(cmd2);
  wait(&status);
  pclose(gdb_f);
  putchar('\n');
  fflush(stdout);
  return !WEXITSTATUS(status);
}

/* disable as this crashes under DPMI trying to trace through DOS stack */
/* ... and re-enable because people fuck up instead of installing gdb.
 * But run this only if the gdb trace fails. */
#ifdef HAVE_BACKTRACE
/* Obtain a backtrace and print it to `stdout'.
   (derived from 'info libc')
 */
static void print_trace (void)
{
#define MAX_FRAMES 256
  void *array[MAX_FRAMES];
  int size;
  char **strings;
  size_t i;

  size = backtrace (array, MAX_FRAMES);
  strings = backtrace_symbols (array, size);
  log_printf("Obtained %d stack frames.\n", size);

  for (i = 0; i < size; i++)
    log_printf("%s\n", strings[i]);

  free (strings);
  log_printf("Backtrace finished\n");
}
#endif

static void collect_info(pid_t pid)
{
  const char *cmd0 = "ldd %s";
  const char *cmd1 = "getconf GNU_LIBC_VERSION";
  const char *cmd2 = "getconf GNU_LIBPTHREAD_VERSION";
  const char *cmd3 = "cat /proc/%i/maps";
  char *tmp;
  int ret;

  printf("System info:\n");
  fflush(stdout);

  ret = asprintf(&tmp, cmd0, dosemu_proc_self_exe);
  assert(ret != -1);

  if(system(tmp)) {
    printf("command '%s' failed\n", tmp);
  }
  free(tmp);

  if(system(cmd1)) {
    printf("command '%s' failed\n", cmd1);
  }

  if(system(cmd2)) {
    printf("command '%s' failed\n", cmd2);
  }

  ret = asprintf(&tmp, cmd3, pid);
  assert(ret != -1);

  if(system(tmp)) {
    printf("command '%s' failed\n", tmp);
  }
  free(tmp);

  fflush(stdout);
}

static int do_gdb_debug(void)
{
  int ret = 0;
  pid_t dosemu_pid = getpid();
  pid_t dbg_pid;
  int status;
  sigset_t oset;

  if (getuid() != geteuid())
    return 0;

  sigprocmask(SIG_BLOCK, &q_mask, &oset);
  switch ((dbg_pid = fork())) {
    case 0:
      signal_done();
      sigprocmask(SIG_SETMASK, &oset, NULL);

      dup2(vlog_get_fd(), STDOUT_FILENO);
      dup2(vlog_get_fd(), STDERR_FILENO);

      collect_info(dosemu_pid);

      if (!start_gdb(dosemu_pid))
        _exit(1);
      do_debug();
      if (!stop_gdb())
        _exit(1);
      _exit(0);
      break;
    case -1:
      error("fork failed, %s\n", strerror(errno));
      break;
    default:
      waitpid(dbg_pid, &status, 0);
      if (WEXITSTATUS(status)) {
        dbug_printf("backtrace failure\n");
      } else {
        ret = 1;
        dbug_printf("done backtrace\n");
      }
      break;
  }
  sigprocmask(SIG_SETMASK, &oset, NULL);
  return ret;
}

void gdb_debug(void)
{
    int ret = do_gdb_debug();
#if 0
    if (!ret) {
#ifdef HAVE_BACKTRACE
        print_trace();
#endif
        error("Please install gdb!\n");
    }
#else
    /* the problem with the above is that gdb usually doesn't work
     * because of the security restrictions */
    error("Please ");
    if (!ret)
        error("@install gdb, ");
    error("@update dosemu from git, compile it with debug\n"
        "info and make a bug report with the content of ~/.dosemu/boot.log at\n"
"https://github.com/dosemu2/dosemu2/issues\n");
    error("@Please provide any additional info you can, like the test-cases,\n"
          "URLs and all the rest that fits.\n\n");
#endif

    log_printf("\n");
}

void siginfo_debug(const siginfo_t *si)
{
    error("@\n");
#ifdef __linux__
    psiginfo(si, "");
#else
    error("Got signal: %s\n", strsignal(si->si_signo));
#endif
    error("@\n");
    dbug_printf("%s\nsig: %i code: 0x%02x  errno: 0x%08x  fault address: %p\n",
	  strsignal(si->si_signo),
	  si->si_signo, si->si_code, si->si_errno, si->si_addr);

#ifdef X86_EMULATOR
    /* gdb_debug() will crash in jit code doing backtrace() */
    if (!(IS_EMU_JIT() && e_in_compiled_code()))
#endif
    gdb_debug();
#ifdef HAVE_BACKTRACE
    print_trace();
#endif
    dump_state();
}

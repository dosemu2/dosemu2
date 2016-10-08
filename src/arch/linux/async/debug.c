#include "emu.h"
#include "dosemu_config.h"
#include "debug.h"
#include "sig.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <execinfo.h>

static FILE *gdb_f = NULL;

static void gdb_command(char *cmd)
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

  ret = asprintf(&buf, "gdb %s", dosemu_proc_self_exe);
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
  char *cmd1 = "info registers\n";
//  char *cmd2 = "backtrace\n";
  char *cmd3 = "thread apply all backtrace full\n";

  gdb_command(cmd1);
//  gdb_command(cmd2);
  gdb_command(cmd3);
}

static int stop_gdb(void)
{
  char *cmd1 = "detach\n";
  char *cmd2 = "quit\n";
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
#if 1
/* Obtain a backtrace and print it to `stdout'.
   (derived from 'info libc')
 */
static void print_trace (void)
{
  void *array[10];
  int size;
  char **strings;
  size_t i;

  size = backtrace (array, 10);
  strings = backtrace_symbols (array, size);

  fprintf(dbg_fd, "Obtained %d stack frames.\n", size);

  for (i = 0; i < size; i++)
    fprintf(dbg_fd, "%s\n", strings[i]);

  free (strings);
  fprintf(dbg_fd, "Backtrace finished\n");
  fflush(dbg_fd);
}
#endif

static void collect_info(pid_t pid)
{
  char *cmd0 = "ldd %s";
  char *cmd1 = "getconf GNU_LIBC_VERSION";
  char *cmd2 = "getconf GNU_LIBPTHREAD_VERSION";
  char *cmd3 = "cat /proc/%i/maps";
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
  sigset_t set, oset;

  if (getuid() != geteuid())
    return 0;

  sigemptyset(&set);
  sigaddset(&set, SIGIO);
  sigaddset(&set, SIGALRM);
  sigprocmask(SIG_BLOCK, &set, &oset);
  switch ((dbg_pid = fork())) {
    case 0:
      ioselect_done();
      signal_done();
      sigprocmask(SIG_SETMASK, &oset, NULL);

      dup2(fileno(dbg_fd), STDOUT_FILENO);
      dup2(fileno(dbg_fd), STDERR_FILENO);

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
        print_trace();
        error("Please install gdb!\n");
    }
#else
    /* the problem with the above is that gdb usually doesn't work
     * because of the security restrictions */
    if (!ret)
        error("Please install gdb!\n");
    print_trace();
#endif
}

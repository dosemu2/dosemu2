#include "emu.h"
#include "dosemu_config.h"
#include "debug.h"
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

static void stop_gdb(void)
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
}

/* disable as this crashes under DPMI trying to trace through DOS stack */
#if 0
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

  printf ("Obtained %d stack frames.\n", size);

  for (i = 0; i < size; i++)
    printf ("%s\n", strings[i]);

  free (strings);
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

//  print_trace();
  fflush(stdout);
}

void gdb_debug(void)
{
  pid_t dosemu_pid = getpid();
  pid_t dbg_pid;
  int status;

  if (getuid() != geteuid())
    return;

  switch ((dbg_pid = fork())) {
    case 0:
      dup2(fileno(dbg_fd), STDOUT_FILENO);
      dup2(fileno(dbg_fd), STDERR_FILENO);

      collect_info(dosemu_pid);

      if (start_gdb(dosemu_pid)) {
        do_debug();
        stop_gdb();
      }

      _exit(0);
      break;
    case -1:
      error("fork failed, %s\n", strerror(errno));
      return;
    default:
      waitpid(dbg_pid, &status, 0);
      dbug_printf("done backtrace\n");
  }
}

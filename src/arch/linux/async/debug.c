#include "emu.h"
#include "dosemu_config.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

static FILE *gdb_f = NULL;

static void gdb_command(char *cmd)
{
  printf(cmd);
  fflush(stdout);
  fprintf(gdb_f, cmd);
  fflush(gdb_f);
}

static int start_gdb(pid_t dosemu_pid)
{
  char *buf;

  printf("Debug info:\n");
  fflush(stdout);
  asprintf(&buf, "gdb %s", dosemu_proc_self_exe);
  printf(buf);
  putchar('\n');
  fflush(stdout);
  if (!(gdb_f = popen(buf, "w")))
    return 0;
  sprintf(buf, "attach %i\n", dosemu_pid);
  gdb_command(buf);
  free(buf);
  return 1;
}

static void do_debug(void)
{
  char *cmd1 = "info registers\n";
  char *cmd2 = "backtrace\n";

  gdb_command(cmd1);
  gdb_command(cmd2);
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

static void collect_info(void)
{
  char *cmd0;
  char *cmd1 = "getconf GNU_LIBC_VERSION";
  char *cmd2 = "getconf GNU_LIBPTHREAD_VERSION";
  char *cmd3 = "gcc -v";
  char *cmd4 = "uname -a";

  printf("System info:\n");
  fflush(stdout);
  asprintf(&cmd0, "ldd %s", dosemu_proc_self_exe);
  system(cmd0);
  free(cmd0);
  system(cmd1);
  system(cmd2);
  system(cmd3);
  system(cmd4);
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

      collect_info();

      if (start_gdb(dosemu_pid)) {
        do_debug();
        stop_gdb();
      }

      _exit(0);
      break;
    case -1:
      return;
    default:
      waitpid(dbg_pid, &status, 0);
  }
}

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_ether.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/mman.h>

#include "../include/shared.h"
#include "get_info.h"

#define E_printf printf
#define leavedos(x) exit(x)

char *memory;

int
shared_memory_init (char *pid)
{

  static int ret_val;

  get_shared_memory_info (pid);

  if (shm_qf_id)
    {
      memory = malloc (SHARED_QUEUE_FLAGS_AREA);

      if (((caddr_t) ret_val = (caddr_t) shmat (shm_qf_id, (u_char *) memory, SHM_REMAP | SHM_RND)) == (caddr_t) 0xffffffff)
	{
	  E_printf ("SHM: Mapping to 0 unsuccessful: %s\n", strerror (errno));
	  leavedos (44);
	}
      if (shmctl (shm_qf_id, IPC_RMID, (struct shmid_ds *) 0) < 0)
	{
	  E_printf ("SHM: Shmctl SHM unsuccessful: %s\n", strerror (errno));
	}
      return 0;
    }
  else
    return 1;


}

void
main (int argc, char **argv)
{
  int i, j;
  if (argc < 2)
    {
      fprintf (stderr, "Please give me a pid\n");
      return;
    }

  if (shared_memory_init (argv[1]))
    {
      fprintf (stderr, "Unable to access keyboard, check server\n");
    }
  else
    {
      E_printf ("keybuffer start = 0x%04x\n", (int) memory[1816]);
      E_printf ("keybuffer end   = 0x%04x\n", (int) memory[1820]);
      for (j = 0; j < 10; j++)
	{
	  for (i = 0; i < 10; i += 2)
	    {
	      E_printf ("0x%02x ", (u_char) memory[1824 + i + (j * 10)]);
	      if ((u_char) memory[1824 + i + (j * 10)] >= 32 &&
		  (u_char) memory[1824 + i + (j * 10)] <= 121)
		E_printf ("%c ", (u_char) memory[1824 + i + (j * 10)]);
	      else
		E_printf ("  ");
	      E_printf ("0x%02x  ", (u_char) memory[1824 + i + (j * 10) + 1]);
	    }
	  putchar ('\n');
	}
      putchar ('\n');
      getchar ();
    }
}

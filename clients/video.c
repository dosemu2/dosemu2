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

  FILE *tmpfile_fd;
  static int ret_val;
  u_char devname[30], in_string[81];

  get_shared_memory_info (pid);
  if (shm_video_id)
    {

      memory = malloc (SHARED_VIDEO_AREA);

      if (((caddr_t) ret_val = (caddr_t) shmat (shm_video_id, (u_char *) memory, SHM_REMAP | SHM_RND)) == (caddr_t) 0xffffffff)
	{
	  E_printf ("SHM: Mapping to 0 unsuccessful: %s\n", strerror (errno));
	  leavedos (44);
	}
      if (shmctl (shm_video_id, IPC_RMID, (struct shmid_ds *) 0) < 0)
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
      fprintf (stderr, "Unable to access video, check server video mode.\n");
    }
  else
    {

      for (j = 0; j < 25; j++)
	{
	  putchar ('\n');
	  for (i = 0; i < 160; i += 2)
	    {
	      E_printf ("%c", (u_char) memory[0x18000 + i + (j * 160)]);
	    }
	}
      getchar ();
    }
}

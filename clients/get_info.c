#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#ifdef __linux__
#include <linux/if_ether.h>
#endif
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

int shm_qf_id=0;
int shm_video_id=0;

void get_shared_memory_info(char *pid) {

  FILE *tmpfile_fd, *dbg_fd;
  u_char in_string[81];
  u_char devname[81];
  struct stat statout;

  sprintf(devname, "%s%s", TMPFILE, pid);
  
  if ((tmpfile_fd = fopen(devname, "r")) == NULL) {
    fprintf(stderr, "CLIENT: Unable to open %s%s: %s\n",TMPFILE, pid, strerror(errno));
    exit(0);
  }
 /* 
  * try to go to fd-3 -- if its there, do an fdopen to it,
  * otherwise use /dev/null for fd3
  */
#if 0
  if(!fstat(3, &statout)) {
        dbg_fd = fdopen(3, "w");
  } else {
#endif
        dbg_fd = fopen("/dev/null", "w");
#if 0
  }
#endif
  if(!dbg_fd) {
        fprintf(stderr, "Can't open fd3\n");
        exit(1);
  }
  fgets(in_string, 80, tmpfile_fd);
  fprintf(dbg_fd, "%s", in_string);
  fgets(in_string, 80, tmpfile_fd);
  fprintf(dbg_fd, "%s", in_string);
  shm_qf_id = atoi(&in_string[4]);
  fprintf(dbg_fd, "shmid for queues and flags = %d\n", shm_qf_id);
  fgets(in_string, 80, tmpfile_fd);
  fprintf(dbg_fd, "%s", in_string);
  shm_video_id = atoi(&in_string[4]);
  fprintf(dbg_fd, "shmid for video            = %d\n", shm_video_id);

  fclose(dbg_fd);
}

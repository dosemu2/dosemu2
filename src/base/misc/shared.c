/* 
 * (C) Copyright 1992, ..., 2002 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * REMARK
 *	Memory areas needed to be shared between the DOSEMU process
 *	and other clients.
 * /REMARK
 *
 * DANG_END_MODULE
 *
 *
 */
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include "config.h"
#include "emu.h"
#include "shared.h"
#include "priv.h"
#include "mapping.h"



static char devname_[30];

/*
 * DANG_BEGIN_FUNCTION shared_qf_memory_init
 *
 * Setup all memory areas to be shared with clients.
 *
 * DANG_END_FUNCTION
 */
void shared_memory_init(void)
{
  PRIV_SAVE_AREA
  int        pid;
  char info[81];
  int tmpfile_fd;

  open_mapping(MAPPING_SHARED);
/*
 *
 * The video area
 *
 */

 if(!config.dualmon && !config.X && !config.vga && !config.console_video) {

  void *shm_video = alloc_mapping(MAPPING_SHARED | MAPPING_SHM, SHARED_VIDEO_AREA, 0);
  if (!shm_video) {
    E_printf("SHM: Initial Video IPC mapping unsuccessful: %s\n", strerror(errno));
    E_printf("SHM: Do you have IPC in the kernel?\n");
    leavedos(43);
  }
  E_printf("SHM: shm_video allocated, master mapping at %p\n", shm_video);

  shm_video = mmap_mapping(MAPPING_SHARED | MAPPING_SHM, (void *)0xA0000,
				-1, 0, shm_video);
  if ((int)shm_video == -1) {
    E_printf("SHM: Mapping to Video 0 unsuccessful: %s\n", strerror(errno));
    leavedos(44);
  }
 }


/* 
 * DANG_BEGIN_REMARK
 *	Output info required for client activity
 *	(NOTE: 'client activity' as of 2000/02/02 totally disabled,
 *	but left file structure compatible, --Hans)
 *
 * DANG_END_REMARK
 */
  pid = getpid();
  sprintf(devname_, "%s%d", TMPFILE, pid);

  enter_priv_off();
  tmpfile_fd = open(devname_, O_WRONLY|O_CREAT, 0600);
  leave_priv_setting();
  if (tmpfile_fd < 1) {
    E_printf("SHM: Unable to open %s%d for sending client data: %s\n",TMPFILE, pid, strerror(errno));
  }
  sprintf(info, "dosemu-%s\n", VERSTR);
  write(tmpfile_fd, info, strlen(info));

  sprintf(info, "SQF=-1\nSVA=-1\n");
  write(tmpfile_fd, info, strlen(info));
}

void shared_memory_exit(void)
{

  PRIV_SAVE_AREA
  enter_priv_on();
  unlink(devname_);
  leave_priv_setting();
  close_mapping(MAPPING_SHARED);
}

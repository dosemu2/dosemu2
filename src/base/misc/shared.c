/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
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
#include "mapping.h"



/*
 * DANG_BEGIN_FUNCTION shared_qf_memory_init
 *
 * Setup all memory areas to be shared with clients.
 *
 * DANG_END_FUNCTION
 */
void shared_memory_init(void)
{
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
}

void shared_memory_exit(void)
{
  close_mapping(MAPPING_SHARED);
}

/*
 * DANG_BEGIN_MODULE
 *
 *	Memory areas needed to be shared between the DOSEMU process
 *	and other clients.
 *
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * $Date: 1995/04/08 22:30:40 $
 * $Source: /usr/src/dosemu0.60/dosemu/RCS/shared.c,v $
 * $Revision: 1.4 $
 * $State: Exp $
 *
 * DANG_END_CHANGELOG
 *
 */
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
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
#include <sys/mman.h>

#include "config.h"
#include "emu.h"
#include "shared.h"

static char devname_[30];
#ifdef __NetBSD__
#define SHM_REMAP 0			/* XXX? */
#endif

/*
 * DANG_BEGIN_FUNCTION shared_qf_memory_init
 *
 * Setup all memory areas to be shared with clients.
 *
 * DANG_END_FUNCTION
 */
void shared_memory_init(void) {
  static int shm_qf_id=0;
  static int shm_video_id=0;
  static int ret_val;
  int        pid;
  char info[81];
  int tmpfile_fd;

/*
 *
 * First the keyboard
 *
 */

  if ((shm_qf_id = shmget(IPC_PRIVATE, SHARED_QUEUE_FLAGS_AREA, 0755)) < 0) {
    E_printf("SHM: Initial QF IPC mapping unsuccessful: %s\n", strerror(errno));
    E_printf("SHM: Do you have IPC in the kernel?\n");
    leavedos(43);
  }
  else
    E_printf("SHM: shm_qf_id=%x\n", shm_qf_id);

  if (((caddr_t)shared_qf_memory = (caddr_t) shmat(shm_qf_id, (u_char *)0, 0)) == (caddr_t) 0xffffffff) {
    E_printf("SHM: Mapping to QF 0 unsuccessful: %s\n", strerror(errno));
    leavedos(44);
  }
  if (shmctl(shm_qf_id, IPC_RMID, (struct shmid_ds *) 0) < 0) {
    E_printf("SHM: Shmctl QF SHM unsuccessful: %s\n", strerror(errno));
  }

/*
 *
 * Next the video area
 *
 */

 if(!config.dualmon && !config.X && !config.vga && !config.console_video) {

  if ((shm_video_id = shmget(IPC_PRIVATE, SHARED_VIDEO_AREA, 0755)) < 0) {
    E_printf("SHM: Initial Video IPC mapping unsuccessful: %s\n", strerror(errno));
    E_printf("SHM: Do you have IPC in the kernel?\n");
    leavedos(43);
  }
  else
    E_printf("SHM: shm_video_id=%x\n", shm_video_id);

  if (((caddr_t)ret_val = (caddr_t) shmat(shm_video_id, (u_char *)0xA0000, SHM_REMAP /* |SHM_RND */)) == (caddr_t) 0xffffffff) {
    E_printf("SHM: Mapping to Video 0 unsuccessful: %s\n", strerror(errno));
    leavedos(44);
  }
  if (shmctl(shm_video_id, IPC_RMID, (struct shmid_ds *) 0) < 0) {
    E_printf("SHM: Shmctl Video SHM unsuccessful: %s\n", strerror(errno));
  }
 }

/* 
 * DANG_BEGIN_REMARK
 *	Set to 0 all client request area
 * DANG_END_REMARK
 */
*(int *)(shared_qf_memory + CLIENT_REQUEST_FLAG_AREA) = (int)0;
E_printf("SHM: Client request area set to %04d\n", *(int *)(shared_qf_memory + CLIENT_REQUEST_FLAG_AREA));

/* 
 * DANG_BEGIN_REMARK
 *	Output info required for client activity
 * DANG_END_REMARK
 */
  pid = getpid();
  sprintf(devname_, "%s%d", TMPFILE, pid);

  if ((tmpfile_fd = open(devname_, O_WRONLY|O_CREAT, 0666)) < 1) {
    E_printf("SHM: Unable to open %s%d for sending client data: %s\n",TMPFILE, pid, strerror(errno));
  }
  sprintf(info, "dosemu-%s\n", VERSTR);
  write(tmpfile_fd, info, strlen(info));

  sprintf(info, "SQF=%08d\n", shm_qf_id);
  write(tmpfile_fd, info, strlen(info));

  sprintf(info, "SVA=%08d\n", shm_video_id);
  write(tmpfile_fd, info, strlen(info));

}

void shared_memory_exit(void) {

#if 1
  unlink(devname_);
#endif

}

/* 
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */
#if 0
#define NCU
#endif
/*
 * Robert Sanders, started 3/1/93
 *
 *
 */
#include <features.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#ifndef __NetBSD__
#if GLIBC_VERSION_CODE >= 2000
#include <netinet/if_ether.h>
#else
#include <linux/if_ether.h>
#endif
#endif
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <termios.h>			/* for mouse.h */
#ifdef __NetBSD__
#include <features.h>
#endif
#if GLIBC_VERSION_CODE >= 2000
#include <features.h>
#include <sys/mman.h>
#else
#define MAP_ANON MAP_ANONYMOUS
extern caddr_t mmap __P ((caddr_t __addr, size_t __len,
			  int __prot, int __flags, int __fd, off_t __off));
extern int munmap __P ((caddr_t __addr, size_t __len));
#include <linux/mman.h>
#endif

#include "memory.h"
#include "emu.h"
#include "termio.h"
#include "dosio.h"
#include "mouse.h"
#include "int.h"

#include "pic.h"
#include "bitops.h"
#include "priv.h"

extern void DOSEMUMouseEvents(void);

extern void xms_init(void);
extern void video_memory_setup(void);
extern void dump_kbuffer(void);
extern int int_count[];
extern int in_readkeyboard, keybint;

#define PAGE_SIZE	4096

/* my test shared memory IDs */
static struct {
  int 			/* normal mem if idt not attached at 0x100000 */
  hmastate;		/* 1 if HMA mapped in, 0 if idt mapped in */
}
sharedmem;

/* Correct HMA size is 64*1024 - 16, but IPC seems not to like this
   hence I would consider that those 16 missed bytes get swapped back
   and forth and may cause us grief - a BUG */
#define HMASIZE (64*1024)
#define HMAAREA (u_char *)0x100000
#ifdef __NetBSD__
#define SHM_REMAP 0			/* XXX? */
#endif

/* Used for all IPC HMA activity */
static u_char *HMAkeepalive; /* Use this to keep shm_hma_alive */
static int shm_hma_id;
static int shm_wrap_id;
#ifdef NCU
static int shm_video_id;
#endif
static caddr_t ipc_return;

void HMA_MAP(int HMA)
{
  PRIV_SAVE_AREA
  int retval;

  E_printf("Entering HMA_MAP with HMA=%d\n", HMA);
  enter_priv_on();
  retval = shmdt(HMAAREA);
  leave_priv_setting();
  if (retval < 0) {
    E_printf("HMA: Detaching HMAAREA unsuccessful: %s\n", strerror(errno));
    leavedos(48);
  }
  E_printf("HMA: detached at %p\n", HMAAREA);
  if (HMA){
    enter_priv_on();
    ipc_return = (caddr_t) shmat(shm_hma_id, HMAAREA, SHM_REMAP );
    leave_priv_setting();
    if (ipc_return == (caddr_t) 0xffffffff) {
      E_printf("HMA: Mapping HMA id %x to HMAAREA %p unsuccessful: %s\n",
	       shm_hma_id, HMAAREA, strerror(errno));
      leavedos(47);
    }
    E_printf("HMA: mapped id %x to %p\n", shm_hma_id, ipc_return);
  }
  else {
    enter_priv_on();
    ipc_return = (caddr_t) shmat(shm_wrap_id, HMAAREA, 0 );
    leave_priv_setting();
    if (ipc_return == (caddr_t) 0xffffffff) {
      E_printf("HMA: Mapping WRAP to HMAAREA unsuccessful: %s\n", strerror(errno));
      leavedos(47);
    }
    E_printf("HMA: mapped id %x to %p\n", shm_wrap_id, ipc_return);
  }
}

void
set_a20(int enableHMA)
{
  if (sharedmem.hmastate == enableHMA)
    g_printf("WARNING: redundant %s of A20!\n", enableHMA ? "enabling" :
	  "disabling");

  /* to turn the A20 on, one must unmap the "wrapped" low page, and
   * map in the real HMA memory. to turn it off, one must unmap the HMA
   * and make FFFF:xxxx addresses wrap to the low page.
   */
  HMA_MAP(enableHMA);

  sharedmem.hmastate = enableHMA;
}

void HMA_init(void)
{
  /* initially, no HMA */
  HMAkeepalive = valloc(HMASIZE); /* This is used only so that shmdt stays going */
  sharedmem.hmastate = 0;

  if ((shm_hma_id = shmget(IPC_PRIVATE, HMASIZE, 0755)) < 0) {
    E_printf("HMA: Initial HMA mapping unsuccessful: %s\n", strerror(errno));
    E_printf("HMA: Do you have IPC in the kernel?\n");

    leavedos(43);
  }
  if ((shm_wrap_id = shmget(IPC_PRIVATE, HMASIZE, 0755)) < 0) {
    E_printf("HMA: Initial WRAP at 0x0 mapping unsuccessful: %s\n", strerror(errno));
    leavedos(43);
  }

#ifdef NCU
/* Here is an IPC shm area for looking at DOS's video area */
  if ((shm_video_id = shmget(IPC_PRIVATE, GRAPH_SIZE, 0755)) < 0) {
    E_printf("VIDEO: Initial IPC GET mapping unsuccessful: %s\n", strerror(errno));
    leavedos(43);
  }
  else
    v_printf("VIDEO: SHM_VIDEO_ID = 0x%x\n", shm_video_id);
#endif

  /* attach regions: page 0 (idt) at address 0 (must be specified as 1 and
   * SHM_RND, and must specify new SHM_REMAP flag to overwrite existing
   * memory (cannot munmap() it!) and code space.
   */

  if ((ipc_return = (caddr_t) shmat(shm_wrap_id, (u_char *)0x1, SHM_REMAP|SHM_RND)) == (caddr_t) 0xffffffff) {
    E_printf("HMA: Mapping to 0 unsuccessful: %s\n", strerror(errno));
    leavedos(44);
  }
  E_printf("HMA: mapped id %x to %p\n", shm_wrap_id, ipc_return);
  if ((ipc_return = (caddr_t) shmat(shm_wrap_id, HMAAREA, SHM_REMAP)) == (caddr_t) 0xffffffff) {
    E_printf("HMA: Adding mapping to 0x100000 unsuccessful: %s\n", strerror(errno));
    leavedos(44);
  }
  E_printf("HMA: mapped id %x to %p\n", shm_wrap_id, ipc_return);
  if ((ipc_return = (caddr_t) shmat(shm_hma_id, HMAkeepalive, SHM_REMAP)) == (caddr_t) 0xffffffff) {
    E_printf("HMA: Adding mapping to HMAkeepalive unsuccessful: %s\n", strerror(errno));
    leavedos(45);
  }
  E_printf("HMA: mapped id %x to %p\n", shm_hma_id, ipc_return);
#ifdef NCU
  if ((ipc_return = (caddr_t) shmat(shm_video_id, (caddr_t)0xb0000, SHM_REMAP)) == (caddr_t) 0xffffffff) {
    E_printf("VIDEO: Adding mapping to video memory unsuccessful: %s\n", strerror(errno));
    leavedos(45);
  }
  else
    v_printf("VIDEO: Video attached %x to %p\n", shm_video_id, ipc_return);
#endif
#ifdef __linux__
  /* On other systems, you'll have to delete the segments in hma_exit... */
  if (shmctl(shm_hma_id, IPC_RMID, (struct shmid_ds *) 0) < 0) {
    E_printf("HMA: Shmctl HMA unsuccessful: %s\n", strerror(errno));
  }
  if (shmctl(shm_wrap_id, IPC_RMID, (struct shmid_ds *) 0) < 0) {
    E_printf("HMA: Shmctl WRAP unsuccessful: %s\n", strerror(errno));
  }
#ifdef NCU
  if (shmctl(shm_video_id, IPC_RMID, (struct shmid_ds *) 0) < 0) {
    E_printf("VIDEO: Shmctl VIDEO unsuccessful: %s\n", strerror(errno));
  }
#endif
#endif
}






void
hma_exit(void)
{
#ifdef __NetBSD__
  if (shmctl(shm_hma_id, IPC_RMID, (struct shmid_ds *) 0) < 0) {
    E_printf("HMA: Shmctl HMA unsuccessful: %s\n", strerror(errno));
  }
  if (shmctl(shm_wrap_id, IPC_RMID, (struct shmid_ds *) 0) < 0) {
    E_printf("HMA: Shmctl WRAP unsuccessful: %s\n", strerror(errno));
  }
#ifdef NCU
  if (shmctl(shm_video_id, IPC_RMID, (struct shmid_ds *) 0) < 0) {
    E_printf("VIDEO: Shmctl VIDEO unsuccessful: %s\n", strerror(errno));
  }
#endif
#endif
}

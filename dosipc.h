#ifndef DOSIPC_H
#define DOSIPC_H

#include <sys/ipc.h>
#include <sys/shm.h>
#include "mutex.h"

void start_dosipc(void), stop_dosipc(void), main_dosipc(void), ipc_send2child(int);
extern void child_close_mouse(), child_open_mouse();

void memory_setup(void), set_a20(int);

void sigipc(int);

extern pid_t parent_pid, ipc_pid, ser_pid;

/* these are the UNIX domain stream sockets used for communication.
 * the parent process writes to and reads from PARENT_FD
 */

extern int ipc_fd[2], key_fd[2];

#define PARENT_FD	ipc_fd[0]
#define CHILD_FD	ipc_fd[1]
#define PK_FD		key_fd[0]
#define CK_FD		key_fd[1]

#define SIGIPC		SIGUSR2

/* these are the IPC command bytes */
#define DMSG_EXIT		1
#define DMSG_ACK		2
#define DMSG_SHMTEST		3
#define DMSG_FKEY		4
#define DMSG_PRINT		5
#define DMSG_READKEY		6
#define DMSG_SENDKEY		7
#define DMSG_NOREADKEY		8
#define DMSG_CTRLBRK		9
#define DMSG_PAUSE		10
#define DMSG_UNPAUSE		11
#define DMSG_INT8		12
#define DMSG_INT9		13
#define DMSG_SETSCAN            14
#define DMSG_SER		15
#define DMSG_SENDINT		18	/* Interrupt */
#define DMSG_PKTDRVR            19	/* virtual pkt drvr */

struct ipcpkt {
  u_int cmd;

  union {
    unsigned short key;
    struct {
      int param1, param2;
    }
    params;
  }
  u;
};

#define MSGSIZE		sizeof(struct ipcpkt)

/* do, while necessary because if { ... }; precludes an else, while
 * do { ... } while(); does not. ugly */
#define HMA_MAP(newshm) \
   do { \
      if (shmdt((char *)0x100000) == -1)    \
 error("ERROR: HMA detach: %s (%d)\n", strerror(errno), errno); \
      if (shmat(newshm, (char *)0x100000, 0) == (char *)-1)  \
     error("ERROR: HMA attach: %s (%d)\n", strerror(errno), errno); \
   } while (0)

/* these are for the shared parameter area */
#define PARAM_SIZE	(64*1024)

/* SER_QUEUE_LEN MUST be a power of 2 */
/* #define SER_QUEUE_LEN	400 */
#define SER_QUEUE_LEN	(1024)

struct ser_param {
  volatile char queue[SER_QUEUE_LEN];
  volatile int start, end;
  volatile int num_ints;
  volatile u_char dready;
};

typedef struct param_struct {
  volatile struct ser_param ser[MAX_SER];
  sem_t ser_locked;
} param_t;

extern param_t *param;

extern void set_keyboard_bios();
extern void insert_into_keybuffer();

#endif /* DOSIPC_H */

/* mutex.c for the DOS emulator
 *       Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1994/06/12 23:15:37 $
 * $Source: /home/src/dosemu0.52/RCS/mutex.c,v $
 * $Revision: 2.1 $
 * $State: Exp $
 *
 * $Log: mutex.c,v $
 * Revision 2.1  1994/06/12  23:15:37  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.3  1994/03/13  01:07:31  root
 * Poor attempt to optimize.
 *
 * Revision 1.2  1994/01/20  21:14:24  root
 * Indent.
 *
 * Revision 1.1  1993/11/12  12:32:17  root
 * Initial revision
 *
 * Revision 1.1  1993/07/07  00:49:06  root
 * Initial revision
 *
 * Revision 1.1  1993/05/04  05:29:22  root
 * Initial revision
 *
 *
 */

#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>

#include "config.h"
#include "emu.h"
#include "mutex.h"

sem_t
mutex_init(void)
{
  sem_t sem;
  union semun arg;

  sem = DOS_SYSCALL(semget(IPC_PRIVATE, 1, 0));
  arg.val = 1;
  DOS_SYSCALL(semctl(sem, 0, SETVAL, arg));
  return sem;
}

void
mutex_deinit(sem_t sem)
{
  union semun arg;

  DOS_SYSCALL(semctl(sem, 0, IPC_RMID, arg));
}

void
mutex_get(sem_t sem)
{
  struct sembuf semoa[1];

  semoa[0].sem_num = 0;		/* only one sem/id */
  semoa[0].sem_op = -1;		/* get resource */
  semoa[0].sem_flg = 0;

  DOS_SYSCALL(semop(sem, semoa, 1));
}

void
mutex_release(sem_t sem)
{
  struct sembuf semoa[1];

  semoa[0].sem_num = 0;
  semoa[0].sem_op = 1;		/* return resource */
  semoa[0].sem_flg = 0;

  DOS_SYSCALL(semop(sem, semoa, 1));
}

/* mutex.c for the DOS emulator 
 *       Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1993/05/04 05:29:22 $
 * $Source: /usr/src/dos/RCS/mutex.c,v $
 * $Revision: 1.1 $
 * $State: Exp $
 *
 * $Log: mutex.c,v $
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
  DOS_SYSCALL( semctl(sem, 0, SETVAL, arg) );
  return sem;
}


int
mutex_deinit(sem_t sem)
{
  union semun arg;

  DOS_SYSCALL( semctl(sem, 0, IPC_RMID, arg) );
}


int
mutex_get(sem_t sem)
{
  struct sembuf semoa[1];

  semoa[0].sem_num = 0;  /* only one sem/id */
  semoa[0].sem_op = -1;  /* get resource */
  semoa[0].sem_flg = 0;

  DOS_SYSCALL( semop(sem, semoa, 1) );
}


int
mutex_release(sem_t sem)
{
  struct sembuf semoa[1];

  semoa[0].sem_num = 0;
  semoa[0].sem_op =  1;  /* return resource */
  semoa[0].sem_flg = 0;

  DOS_SYSCALL( semop(sem, semoa, 1) );
}

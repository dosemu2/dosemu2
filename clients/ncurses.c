#define NCURSES_C 1

 
/* 
 * DANG_BEGIN_MODULE
 *
 * This is James' client based around NCURSES. It (un-)succesfully
 * demonstrates the procedure for making initial connections to the
 * server.
 *
 * DANG_END_MODULE 
 */
 
/* 
 * DANG_BEGIN_CHANGELOG
 *
 * $Date: 1994/09/23 01:33:26 $
 * $Source: /home/src/dosemu0.60/clients/RCS/ncurses.c,v $
 * $Revision: 2.3 $
 * $State: Exp $
 *
 * $Log: ncurses.c,v $
 * Revision 2.3  1994/09/23  01:33:26  root
 * Prep for pre53_21.
 *
 * Revision 2.2  1994/06/14  22:01:48  root
 * Alistair's DANG inserted for the first time :-).
 *
 * Revision 2.1  1994/06/12  23:18:11  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.8  1994/04/29  23:53:20  root
 * Prior to Lutz's latest 94/04/29
 *
 * Revision 1.7  1994/04/27  21:37:08  root
 * *** empty log message ***
 *
 * Revision 1.6  1994/04/23  20:52:58  root
 * Get new stack over/underflow working in VM86 mode..
 *
 * Revision 1.5  1994/04/18  20:59:49  root
 * Checkin prior to Jochen's latest patches.
 *
 * Revision 1.4  1994/04/13  00:11:58  root
 * Jochen's patches
 *
 * Revision 1.3  1994/04/04  22:56:36  root
 * Just having fun :-).
 *
 * Revision 1.2  1994/03/28  23:45:59  root
 * *** empty log message ***
 *
 * Revision 1.1  1994/03/24  23:39:23  root
 * Initial revision
 *
 * DANG_END_CHANGELOG
 */

#include <linux/ipc.h>
#include <linux/shm.h>
#include <errno.h>
#if 1
#include <ncurses.h>
#endif
#include "ncurses.h"

#define u_char unsigned char

/*
 * DANG_BEGIN_REMARK
 *
 * Some of the functions are undocumented. These are the trivial ones.
 *
 * DANG_END_REMARK
 */

int client_init(void){
	return((int)initscr());
}

int client_exit(void){
	return(endwin());
}


/*
 * DANG_BEGIN_FUNCTION main
 *
 * arguments:
 * argc - Number of Command Line arguments.
 * argv - Command Line arguments - as an array of char*'s
 * 
 * description:
 * This function does all of the work associated with this client. It
 * grabs the shared memory used to hold the current Video data and makes some
 * comments about its current screen.
 *
 * DANG_END_FUNCTION
 */

void main(int argc,u_char **argv) {
   u_char c;
   u_char *ptr;
   int shm_video_id;
   caddr_t ipc_return;

/*
 * DANG_BEGIN_REMARK
 *
 * You must be mad if you want to get involved in all of this Shared memory
 * stuff .... 8-)
 *
 * DANG_END_REMARK
 */

#if 0
/* Here is an IPC shm area for looking at DOS's video area */
  if ((shm_video_id = shmget(IPC_PRIVATE, GRAPH_SIZE, 0755)) < 0) {
    fprintf(stderr, "VIDEO: Initial IPC GET mapping unsuccessful: %s\n", strerror(errno));
    return;
  }
  else
    fprintf(stderr, "VIDEO: SHM_VIDEO_ID = 0x%x\n", shm_video_id);
#else
  shm_video_id=0x2;
#endif

  if ((ipc_return = (caddr_t) shmat(shm_video_id, (caddr_t)0x0, 0)) == (caddr_t) 0xffffffff) {
    fprintf(stderr, "VIDEO: Adding mapping to video memory unsuccessful: %s\n", strerror(errno));
  }
  else
    fprintf(stderr, "VIDEO: Video attached\n");

  if (shmctl(shm_video_id, IPC_RMID, (struct shmid_ds *) 0) < 0) {
    fprintf(stderr, "VIDEO: Shmctl VIDEO unsuccessful: %s\n", strerror(errno));
  }

printf("%x\n", ipc_return);
{
 int x;
 for(x=0;x<0x30000;x++) {
   if (ipc_return[x] != 0)
     printf("%c", ipc_return[x]);
 }
}
printf("\n");
return;

   fprintf(stderr, "%d\n", (WINDOW *)client_init());
   move(0,0);
   refresh();
   c=getch();
   if (has_colors()==FALSE) fprintf(stderr,"No COLOR\n");
   start_color();
     {
	  int x,y; 
	for(x=0;x<0xff;x++)  {
	  y=(x<<12);
	  fprintf(stderr, "%x\n", y);
	  attrset(0x7a000); 
	  attrset(y); 
	  mvaddch(0,x,'A'+(x&0xf));
	}
     }
  refresh(); 
  c=getch();
  fprintf(stderr, "%d\n", client_exit());   
}

#undef NCURSES_C

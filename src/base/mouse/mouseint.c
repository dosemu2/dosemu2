/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* Modified for DOSEMU's internal mouse driver support
 * by Alan Hourihane (alanh@fairlite.demon.co.uk)
 * (29/4/94)
 */

/*
 *
 * Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany.
 * Copyright 1993 by David Dawes <dawes@physics.su.oz.au>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Thomas Roell and David Dawes not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  Thomas Roell
 * and David Dawes makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * THOMAS ROELL AND DAVID DAWES DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THOMAS ROELL OR DAVID DAWES BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "emu.h"
#include "mouse.h"
#include "serial.h"
#include "iodev.h"

static void DOSEMUSetMouseSpeed(int old, int new, unsigned cflag);

static int mouse_frozen = 0;

/*
 * DOSEMUSetupMouse --
 *	Sets up the mouse parameters
 */

static void DOSEMUSetupMouse(void)
{
      mouse_t *mice = &config.mouse;
      m_printf("MOUSE: DOSEMUSetupMouse called\n");
      mice->oldset = malloc(sizeof(*mice->oldset));
      tcgetattr(mice->fd, mice->oldset);
      if (mice->type == MOUSE_MOUSEMAN)
        {
          DOSEMUSetMouseSpeed(1200, 1200, mice->flags);
          RPT_SYSCALL(write(mice->fd, "*X", 2));
          DOSEMUSetMouseSpeed(1200, mice->baudRate, mice->flags);
        }
      else if (mice->type != MOUSE_BUSMOUSE && mice->type != MOUSE_PS2 &&
	       mice->type != MOUSE_IMPS2) 
	{
	  m_printf("MOUSE: setting speed to %d baud\n",mice->baudRate);
#if 0   /* this causes my dosemu to hang [rz] */
	  DOSEMUSetMouseSpeed(9600, mice->baudRate, mice->flags);
	  DOSEMUSetMouseSpeed(4800, mice->baudRate, mice->flags);
	  DOSEMUSetMouseSpeed(2400, mice->baudRate, mice->flags);
#endif
	  DOSEMUSetMouseSpeed(1200, mice->baudRate, mice->flags);
	  if (mice->type == MOUSE_LOGITECH)
	    {
	      m_printf("MOUSEINT: Switching to MM-SERIES protocol...\n");

	      /* Switch the mouse into MM series mode; actually, chances
	      	are, if you have an older logitech mouse (like I do), you
	      	were already *in* MM series mode. */
	      RPT_SYSCALL(write(mice->fd, "S", 1));

	      /* Need to use flags for MM series, not Logitech (bugfix) */
	      DOSEMUSetMouseSpeed(mice->baudRate, mice->baudRate,
	      	CS8 | PARENB | PARODD | CREAD | CLOCAL | HUPCL);
	    }

	  if (mice->type == MOUSE_HITACHI)
	  {
	    char speedcmd;

	    RPT_SYSCALL(write(mice->fd, "z8", 2));	/* Set Parity = "NONE" */
	    usleep(50000);
	    RPT_SYSCALL(write(mice->fd, "zb", 2));	/* Set Format = "Binary" */
	    usleep(50000);
	    RPT_SYSCALL(write(mice->fd, "@", 1));	/* Set Report Mode = "Stream" */
	    usleep(50000);
	    RPT_SYSCALL(write(mice->fd, "R", 1));	/* Set Output Rate = "45 rps" */
	    usleep(50000);
	    RPT_SYSCALL(write(mice->fd, "I\x20", 2));	/* Set Incrememtal Mode "20" */
	    usleep(50000);
	    RPT_SYSCALL(write(mice->fd, "E", 1));	/* Set Data Type = "Relative */
	    usleep(50000);

	    /* These sample rates translate to 'lines per inch' on the Hitachi
	       tablet */
	    if      (mice->sampleRate <=   40) speedcmd = 'g';
	    else if (mice->sampleRate <=  100) speedcmd = 'd';
	    else if (mice->sampleRate <=  200) speedcmd = 'e';
	    else if (mice->sampleRate <=  500) speedcmd = 'h';
	    else if (mice->sampleRate <= 1000) speedcmd = 'j';
	    else                               speedcmd = 'd';
	    RPT_SYSCALL(write(mice->fd, &speedcmd, 1));
	    usleep(50000);

	    RPT_SYSCALL(write(mice->fd, "\021", 1));	/* Resume DATA output */
	  }
	  else
	  {
	    m_printf("MOUSE: set sample rate to %d\n",mice->sampleRate);
	    if      (mice->sampleRate <=   0)  { RPT_SYSCALL(write(mice->fd, "O", 1));}
	    else if (mice->sampleRate <=  15)  { RPT_SYSCALL(write(mice->fd, "J", 1));}
	    else if (mice->sampleRate <=  27)  { RPT_SYSCALL(write(mice->fd, "K", 1));}
	    else if (mice->sampleRate <=  42)  { RPT_SYSCALL(write(mice->fd, "L", 1));}
	    else if (mice->sampleRate <=  60)  { RPT_SYSCALL(write(mice->fd, "R", 1));}
	    else if (mice->sampleRate <=  85)  { RPT_SYSCALL(write(mice->fd, "M", 1));}
	    else if (mice->sampleRate <= 125)  { RPT_SYSCALL(write(mice->fd, "Q", 1));}
	    else                               { RPT_SYSCALL(write(mice->fd, "N", 1));}
	  }
        }

      if (mice->type == MOUSE_IMPS2)
        {
	  static unsigned char s1[] = { 243, 200, 243, 100, 243, 80, };
	  static unsigned char s2[] = { 246, 230, 244, 243, 100, 232, 3, };
	  write (mice->fd, s1, sizeof (s1));
	  usleep (30000);
	  write (mice->fd, s2, sizeof (s2));
	  usleep (30000);
	  tcflush (mice->fd, TCIFLUSH);
	}

#ifdef CLEARDTR_SUPPORT
      if (mice->type == MOUSE_MOUSESYSTEMS && (mice->cleardtr))
        {
          int val = TIOCM_DTR;
	  m_printf("MOUSE: clearing DTR\n");
          ioctl(mice->fd, TIOCMBIC, &val);
        }
#if 0     /* Jochen 05.05.94 */
/* Not used for my 3 button mouse */
      if (mice->type == MOUSE_MOUSESYSTEMS && (mice->flags & MF_CLEAR_RTS))
        {
          int val = TIOCM_RTS;
	  m_printf("MOUSE: clearing RTS\n");
          ioctl(mice->fd, TIOCMBIC, &val);
        }
#endif
#endif
}
 
static int DOSEMUMouseProtocol(unsigned char *rBuf, int nBytes)
{
  int                  i, buttons=0, dx=0, dy=0;
  mouse_t             *mice = &config.mouse;
  static int           pBufP = 0;
  static unsigned char pBuf[8];

  static unsigned char proto[11][5] = {
    /*  hd_mask hd_id   dp_mask dp_id   nobytes */
    {	0x40,	0x40,	0x40,	0x00,	3 	},  /* MicroSoft */
    {	0x40,	0x40,	0x40,	0x00,	3 	},  /* MicroSoft-3Bext */
    {	0xf8,	0x80,	0x00,	0x00,	5	},  /* MouseSystems */
    {	0xe0,	0x80,	0x80,	0x00,	3	},  /* MMSeries */
    {	0xe0,	0x80,	0x80,	0x00,	3	},  /* Logitech */
    {	0xf8,	0x80,	0x00,	0x00,	5	},  /* BusMouse */
    {	0x40,	0x40,	0x40,	0x00,	3 	},  /* MouseMan
                                                       [CHRIS-211092] */
    {	0xc0,	0x00,	0x00,	0x00,	3	},  /* PS/2 mouse */
    {	0xe0,	0x80,	0x80,	0x00,	3	},  /* MM_HitTablet */
    {	0x00,	0x00,	0x00,	0x00,	0	},  /* X-mouse (unused entry) */
    {	0xc0,	0x00,	0x00,	0x00,	4	},  /* IMPS/2 mouse */
  };
  
     for ( i=0; i < nBytes; i++) {
	/*
	 * check, if we have a usable data byte
	 */
	if (pBufP != 0 && mice->type != MOUSE_PS2 &&
	    mice->type != MOUSE_IMPS2 &&
	    ((rBuf[i] & proto[mice->type][2]) != proto[mice->type][3]
	     || rBuf[i] == 0x80)) {
	   m_printf("MOUSEINT: Skipping package, pBufP = %d\n",pBufP);
	   pBufP = 0;          /* skip package */
	}

	/*
	 * check, if the current byte is the start of a new package
	 * if not, skip it
	 */
	if (pBufP == 0 &&
	    (rBuf[i] & proto[mice->type][0]) != proto[mice->type][1]) {
	   m_printf("MOUSEINT: Skipping byte %02x\n",rBuf[i]);
	   continue;
	}
	
	pBuf[pBufP++] = rBuf[i];
	if (pBufP != proto[mice->type][4]) continue;
	m_printf("MOUSEINT: package %02x %02x %02x\n",pBuf[0],pBuf[1],pBuf[2]);
	/*
	 * assembly full package
	 */
	switch(mice->type) {
	   
	case MOUSE_MICROSOFT:	    /* The ms protocol, unextended. */
	   buttons = ((pBuf[0] & 0x20) >> 3) | ((pBuf[0] & 0x10) >> 4);
	   dx = (char)(((pBuf[0] & 0x03) << 6) | (pBuf[1] & 0x3F));
	   dy = (char)(((pBuf[0] & 0x0C) << 4) | (pBuf[2] & 0x3F));
	   break;

	case MOUSE_MS3BUTTON:       /* Microsoft */
	{
	   /* The original micro$oft protocol, with a middle-button extension */
	   /*
	    * some devices report a change of middle-button state by
	    * repeating the current button state  (patch by Mark Lord)
	    */
	   static unsigned char prev=0;

	   if (pBuf[0]==0x40 && !(prev|pBuf[1]|pBuf[2]))
	     buttons = 2;           /* third button on MS compatible mouse */
	   else
	     buttons= ((pBuf[0] & 0x20) >> 3) | ((pBuf[0] & 0x10) >> 4);
	     prev = buttons;
	   dx = (char)(((pBuf[0] & 0x03) << 6) | (pBuf[1] & 0x3F));
	   dy = (char)(((pBuf[0] & 0x0C) << 4) | (pBuf[2] & 0x3F));
	}
	break;

	case MOUSE_MOUSEMAN:	    /* MouseMan / TrackMan   [CHRIS-211092] */
	/*
	 * the damned MouseMan has 3/4 bytes packets. The extra byte 
	 * is only there if the middle button is active.
	 * I get the extra byte as a packet with magic numbers in it.
	 * and then switch to 4-byte mode (A.Rubini in gpm-1.10)
	 * Don't complain if you use this code with hi-res timers - AV
	 */
	   /* chordMiddle is always 0! see config.c */
	   if (mice->chordMiddle)
	      buttons = ((pBuf[0] & 0x30) == 0x30) ? 2 : 
	      (((pBuf[0] & 0x20) >> 3) | ((pBuf[0] & 0x10) >> 4));
	   else {
	      buttons = ((pBuf[0] & 0x20) >> 3) | ((pBuf[0] & 0x10) >> 4);
	      m_printf("MOUSEINT: buttons=%02x\n",buttons);

	      /*
	       * if the middle button was pressed or released
	       * we get a fourth byte. Check if it is in the buffer
	       */
	      if (i+1 < nBytes) {
		 if ((rBuf[i+1] & ~0x23)==0) {
		    buttons |= (rBuf[++i] & 0x20) >> 4;
		    m_printf("MOUSEINT: middle button from buffer: %02x\n",
			     buttons);
		 }
	      } else {
		 /*
		  * no data there, try to read it
		  * select seems not to work here ( it returns -1 with
		  * errno==EINTR ), so we poll for a few clock ticks
		  * I really don't like this. So, if anybody has a better
		  * idea...
		  */
		 clock_t c;
		 int     n;
		 /*
		  * Wait from 10 to 20ms for the data to arrive
		  * (8.3ms for a byte at 1200 baud)
		  */
		 c = clock() + 3;
		 n=0;
		 do {
		    n = RPT_SYSCALL(read(mice->fd, rBuf+i, 1));
		    m_printf("MOUSEINT: Inside read waiting for input!\n");
		 } while (n < 1 && clock() < c);
		 if (n==1) {
		    /*
		     * There is data!
		     * Check if it is the status of the middle button,
		     * if not, decrement i so that the byte is used
		     * during the next loop
		     */
		    if ((rBuf[i] & ~0x23)==0) {
		       buttons |= (rBuf[i] & 0x20) >> 4;
		       m_printf("MOUSEINT: middle button read: %02x\n",
				buttons);
		    } else {
		       i--;
		    }
		 }
	      }
	   }	/* chordMiddle */
	   dx = (char)(((pBuf[0] & 0x03) << 6) | (pBuf[1] & 0x3F));
	   dy = (char)(((pBuf[0] & 0x0C) << 4) | (pBuf[2] & 0x3F));
	   break;
      
	case MOUSE_MOUSESYSTEMS:             /* Mouse Systems Corp */
	   buttons = (~pBuf[0]) & 0x07;
	   dx =    (char)(pBuf[1]) + (char)(pBuf[3]);
	   dy = - ((char)(pBuf[2]) + (char)(pBuf[4]));
	   break;
      
	case MOUSE_HITACHI:           /* MM_HitTablet */
	   buttons = pBuf[0] & 0x07;
	   if (buttons != 0)
	      buttons = 1 << (buttons - 1);
	   dx = (pBuf[0] & 0x10) ?   pBuf[1] : -pBuf[1];
	   dy = (pBuf[0] & 0x08) ? -pBuf[2] :   pBuf[2];
	   break;

	case MOUSE_MMSERIES:              /* MM Series */
	case MOUSE_LOGITECH:            /* Logitech Mice */
	   buttons = pBuf[0] & 0x07;
	   dx = (pBuf[0] & 0x10) ?   pBuf[1] : -pBuf[1];
	   dy = (pBuf[0] & 0x08) ? -pBuf[2] :   pBuf[2];
	   break;
      
	case MOUSE_BUSMOUSE:              /* BusMouse */
	   buttons = (~pBuf[0]) & 0x07;
	   dx =   (char)pBuf[1];
	   dy = - (char)pBuf[2];
	   break;

	case MOUSE_PS2:		    /* PS/2 mouse */
	case MOUSE_IMPS2:	    /* IMPS/2 mouse */
	   buttons = (pBuf[0] & 0x04) >> 1 |       /* Middle */
	      (pBuf[0] & 0x02) >> 1 |       /* Right */
	      (pBuf[0] & 0x01) << 2;        /* Left */
	   dx = (pBuf[0] & 0x10) ?    pBuf[1]-256  :  pBuf[1];
	   dy = (pBuf[0] & 0x20) ?  -(pBuf[2]-256) : -pBuf[2];
	   break;
	}

	/*
	 * calculate the new values for buttons, dx and dy
  	 * Ensuring that speed is calculated from current values.
	 */
	mouse_move_buttons(buttons & 0x04, buttons & 0x02, buttons & 0x01);
	mouse_move_relative(dx, dy);

	pBufP = 0;
	return (i + 1);
     }	/* assembly full package */
     return nBytes;
}

static void DOSEMUSetMouseSpeed(int old, int new, unsigned cflag)
{
	struct termios tty;
	char *c;
        mouse_t *mice = &config.mouse;

        m_printf("MOUSE: set speed %d -> %d\n",old,new);
   
	if (tcgetattr(mice->fd, &tty) < 0)
	{
		m_printf("MOUSE: Unable to get status of mouse. Mouse may not function properly.\n");
	}

	tty.c_iflag = IGNBRK | IGNPAR;
	tty.c_oflag = 0;
	tty.c_lflag = 0;
	tty.c_cflag = (tcflag_t)cflag;
	tty.c_cc[VTIME] = 0;
	tty.c_cc[VMIN] = 1;

	switch (old)
	{
	case 9600:
		cfsetispeed(&tty, B9600);
		cfsetospeed(&tty, B9600);
		break;
	case 4800:
		cfsetispeed(&tty, B4800);
		cfsetospeed(&tty, B4800);
		break;
	case 2400:
		cfsetispeed(&tty, B2400);
		cfsetospeed(&tty, B2400);
		break;
	case 1200:
	default:
		cfsetispeed(&tty, B1200);
		cfsetospeed(&tty, B1200);
	}

        m_printf("MOUSE: calling tcsetattr\n");
	if (tcsetattr(mice->fd, TCSADRAIN, &tty) < 0)
	{
		m_printf("MOUSE: Unable to set mouse attributes. Mouse may not function properly.\n");
	}
   
	switch (new)
	{
	case 9600:
		c = "*q";
		cfsetispeed(&tty, B9600);
		cfsetospeed(&tty, B9600);
		break;
	case 4800:
		c = "*p";
		cfsetispeed(&tty, B4800);
		cfsetospeed(&tty, B4800);
		break;
	case 2400:
		c = "*o";
		cfsetispeed(&tty, B2400);
		cfsetospeed(&tty, B2400);
		break;
	case 1200:
	default:
		c = "*n";
		cfsetispeed(&tty, B1200);
		cfsetospeed(&tty, B1200);
	}

        m_printf("MOUSE: writing speed\n");
	if (write(mice->fd, c, 2) != 2)
	{
		m_printf("MOUSE: Unable to write to mouse fd. Mouse may not function properly.\n");
	}

        m_printf("MOUSE: calling tcsetattr\n");
	if (tcsetattr(mice->fd, TCSADRAIN, &tty) < 0)
	{
		m_printf("MOUSE: Unable to set mouse attributes. Mouse may not function properly.\n");
	}
}

static void raw_mouse_getevent(void)
{
/* We define a large buffer, because of high overheads with other processes */
#define MOUSE_BUFFER 1024
	static unsigned char rBuf[MOUSE_BUFFER];
	static int qBeg = 0, qEnd = 0;
        mouse_t *mice = &config.mouse;
        
	int nBytes, nBytesProc;

        nBytes = RPT_SYSCALL(read(mice->fd, (char *)(rBuf+qEnd),
          sizeof(rBuf)-qEnd));
	if (nBytes>0)
	  qEnd += nBytes;
	if (!mouse.enabled)
	  return;
	if (qBeg < qEnd) {
	  m_printf("MOUSE: Read %d bytes. %d bytes in queue\n",
	    nBytes>0 ? nBytes : 0, qEnd-qBeg);
	  nBytesProc = DOSEMUMouseProtocol(rBuf+qBeg, qEnd-qBeg);
	  qBeg += nBytesProc;
	  m_printf("MOUSE: Processed %d bytes. %d bytes still in queue\n",
	    nBytesProc, qEnd-qBeg);
	  if (qBeg >= qEnd)
	    qBeg = qEnd = 0;
	}
	if (qBeg < qEnd) {
	  m_printf("MOUSE: Requesting for the next event\n");
	  pic_request(PIC_IMOUSE);
	}
}

static void parent_close_mouse (void)
{
  mouse_t *mice = &config.mouse;
  if (mice->intdrv && (mice->type == MOUSE_GPM ||
      mice->type == MOUSE_XTERM || mice->type == MOUSE_X ||
      mice->type == MOUSE_SDL))
    return;
  if (mice->intdrv)
     {
	if (mice->fd > 0) {
   	   remove_from_io_select(mice->fd, 1);
           DOS_SYSCALL(close (mice->fd));
	}
    }
  else
    child_close_mouse ();
}

static int parent_open_mouse (void)
{
  mouse_t *mice = &config.mouse;
  if (mice->intdrv && (mice->type == MOUSE_GPM ||
      mice->type == MOUSE_XTERM || mice->type == MOUSE_X ||
      mice->type == MOUSE_SDL))
    return 1;
  if (mice->intdrv)
    {
      struct stat buf;
      int mode = O_RDWR | O_NONBLOCK;

      if (!mice->dev || !strlen(mice->dev))
        return 0;
      stat(mice->dev, &buf);
      if (S_ISFIFO(buf.st_mode) || mice->type == MOUSE_BUSMOUSE || mice->type == MOUSE_PS2) {
	/* no write permission is necessary for FIFO's (eg., gpm) */
        mode = O_RDONLY | O_NONBLOCK;
      }
      mice->fd = -1;
      /* gpm + non-graphics mode doesn't work */
      if ((!S_ISFIFO(buf.st_mode) || config.vga) && mice->dev)
      {
	mice->fd = DOS_SYSCALL(open(mice->dev, mode));
        if (mice->fd == -1) {
          error("Cannot open internal mouse device %s\n",mice->dev);
        }
      }
      if (mice->fd == -1) {
 	mice->intdrv = FALSE;
 	mice->type = MOUSE_NONE;
 	return 0;
      }
      add_to_io_select(mice->fd, 1, mouse_io_callback);
    }
  else
    child_open_mouse ();
  return 1;
}

void freeze_mouse(void)
{
  mouse_t *mice = &config.mouse;
  if (mouse_frozen || mice->fd == -1)
    return;
  remove_from_io_select(mice->fd, 1);
  mouse_frozen = 1;
}

void unfreeze_mouse(void)
{
  mouse_t *mice = &config.mouse;
  if (!mouse_frozen || mice->fd == -1)
    return;
  add_to_io_select(mice->fd, 1, mouse_io_callback);
  mouse_frozen = 0;
}

static int raw_mouse_init(void)
{
  mouse_t *mice = &config.mouse;
  struct stat buf;

  if (!mice->intdrv)
    return FALSE;
  
  m_printf("Opening internal mouse: %s\n", mice->dev);
  if (!parent_open_mouse())
    return FALSE;

  fstat(mice->fd, &buf);
  if (!S_ISFIFO(buf.st_mode) && mice->type != MOUSE_BUSMOUSE && mice->type != MOUSE_PS2)
    DOSEMUSetupMouse();
  /* this is only to try to get the initial internal driver two/three
     button mode state correct; user can override it later. */ 
  if (mice->type == MOUSE_MICROSOFT || mice->type == MOUSE_MS3BUTTON ||
      mice->type == MOUSE_BUSMOUSE || mice->type == MOUSE_PS2)
    mice->has3buttons = FALSE;
  else
    mice->has3buttons = TRUE;
  iodev_add_device(mice->dev);
  return TRUE;
}

static void raw_mouse_close(void)
{
  int result;
  mouse_t *mice = &config.mouse;

  if (mice->fd == -1)
    return;

  if (mice->oldset) {
    m_printf("mouse_close: calling tcsetattr\n");
    result=tcsetattr(mice->fd, TCSANOW, mice->oldset);
    if (result==0)
      m_printf("mouse_close: tcsetattr ok\n");
    else
      m_printf("mouse_close: tcsetattr failed: %s\n",strerror(errno));
  }
  m_printf("mouse_close: closing mouse device, fd=%d\n",mice->fd);
  parent_close_mouse();
  m_printf("mouse_close: ok\n");
}

struct mouse_client Mouse_raw =  {
  "raw",              /* name */
  raw_mouse_init,     /* init */
  raw_mouse_close,    /* close */
  raw_mouse_getevent,  /* run */
  NULL
};


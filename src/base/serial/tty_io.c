/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>
#include <linux/serial.h>

#include "emu.h"
#include "dosemu_config.h"
#include "ser_defs.h"
#include "tty_io.h"

/* This function flushes the internal unix receive buffer [num = port] */
static void tty_rx_buffer_dump(com_t *com)
{
  tcflush(com->fd, TCIFLUSH);
}

/* This function flushes the internal unix transmit buffer [num = port] */
static void tty_tx_buffer_dump(com_t *com)
{
  tcflush(com->fd, TCOFLUSH);
}

static int tty_get_tx_queued(com_t *com)
{
  int ret, queued;
  ret = ioctl(com->fd, TIOCOUTQ, &queued);
  if (ret < 0)
    return ret;
  return queued;
}

/* This function updates the line settings of a serial line (parity,
 * stop bits, word size, and baudrate) to conform to the value
 * stored in the Line Control Register (com[].LCR) and the Baudrate
 * Divisor Latch Registers (com[].dlm and com[].dll)     [num = port]
 */
static void tty_termios(com_t *com)
{
  speed_t baud;
  long int rounddiv;

  if (com->is_file)
    return;

  /* The following is the same as (com[num].dlm * 256) + com[num].dll */
  #define DIVISOR ((com->dlm << 8) | com->dll)

  /* Return if not a tty */
  if (tcgetattr(com->fd, &com->newset) == -1) {
    if(s1_printf) s_printf("SER%d: Line Control: NOT A TTY (%s).\n",com->num,strerror(errno));
    return;
  }

  s_printf("SER%d: LCR = 0x%x, ",com->num,com->LCR);

  /* Set the word size */
  com->newset.c_cflag &= ~CSIZE;
  switch (com->LCR & UART_LCR_WLEN8) {
  case UART_LCR_WLEN5:
    com->newset.c_cflag |= CS5;
    s_printf("5");
    break;
  case UART_LCR_WLEN6:
    com->newset.c_cflag |= CS6;
    s_printf("6");
    break;
  case UART_LCR_WLEN7:
    com->newset.c_cflag |= CS7;
    s_printf("7");
    break;
  case UART_LCR_WLEN8:
    com->newset.c_cflag |= CS8;
    s_printf("8");
    break;
  }

  /* Set the parity.  Rarely-used MARK and SPACE parities not supported yet */
  if (com->LCR & UART_LCR_PARITY) {
    com->newset.c_cflag |= PARENB;
    if (com->LCR & UART_LCR_EPAR) {
      com->newset.c_cflag &= ~PARODD;
      s_printf("E");
    }
    else {
      com->newset.c_cflag |= PARODD;
      s_printf("O");
    }
  }
  else {
    com->newset.c_cflag &= ~PARENB;
    s_printf("N");
  }

  /* Set the stop bits: UART_LCR_STOP set means 2 stop bits, 1 otherwise */
  if (com->LCR & UART_LCR_STOP) {
    /* This is actually 1.5 stop bit when word size is 5 bits */
    com->newset.c_cflag |= CSTOPB;
    s_printf("2, ");
  }
  else {
    com->newset.c_cflag &= ~CSTOPB;
    s_printf("1, ");
  }

  /* Linux 1.1.65 and above supports 115200 and 57600 directly, while
   * Linux 1.1.64 and below do not support them.  For these kernels, make
   * B115200 and B57600 equal to B38400.  These defines also may be
   * important if DOSEMU is ported to other nix-like operating systems.
   */
  #ifndef B115200
  #define B115200 B38400
  #endif
  #ifndef B57600
  #define B57600 B38400
  #endif

  /* The following sets the baudrate.  These nested IF statements rounds
   * upwards to the next higher baudrate. (ie, rounds downwards to the next
   * valid divisor value) The formula is:  bps = 1843200 / (divisor * 16)
   *
   * Note: 38400, 57600 and 115200 won't work properly if setserial has
   * the 'spd_hi' or the 'spd_vhi' setting turned on (they're obsolete!)
   */
  if ((DIVISOR < DIV_57600) && DIVISOR) {               /* 115200 bps */
    s_printf("bps = 115200, ");
    rounddiv = DIV_115200;
    baud = B115200;
  } else if (DIVISOR < DIV_38400) {                     /* 57600 bps */
    s_printf("bps = 57600, ");
    rounddiv = DIV_57600;
    baud = B57600;
  } else if (DIVISOR < DIV_19200) {			/* 38400 bps */
    s_printf("bps = 38400, ");
    rounddiv = DIV_38400;
    baud = B38400;
  } else if (DIVISOR < DIV_9600) {			/* 19200 bps */
    s_printf("bps = 19200, ");
    rounddiv = DIV_19200;
    baud = B19200;
  } else if (DIVISOR < DIV_4800) {			/* 9600 bps */
    s_printf("bps = 9600, ");
    rounddiv = DIV_9600;
    baud = B9600;
  } else if (DIVISOR < DIV_2400) {			/* 4800 bps */
    s_printf("bps = 4800, ");
    rounddiv = DIV_4800;
    baud = B4800;
  } else if (DIVISOR < DIV_1800) {			/* 2400 bps */
    s_printf("bps = 2400, ");
    rounddiv = DIV_2400;
    baud = B2400;
  } else if (DIVISOR < DIV_1200) {			/* 1800 bps */
    s_printf("bps = 1800, ");
    rounddiv = DIV_1800;
    baud = B1800;
  } else if (DIVISOR < DIV_600) {			/* 1200 bps */
    s_printf("bps = 1200, ");
    rounddiv = DIV_1200;
    baud = B1200;
  } else if (DIVISOR < DIV_300) {			/* 600 bps */
    s_printf("bps = 600, ");
    rounddiv = DIV_600;
    baud = B600;
  } else if (DIVISOR < DIV_150) {			/* 300 bps */
    s_printf("bps = 300, ");
    rounddiv = DIV_300;
    baud = B300;
  } else if (DIVISOR < DIV_110) {			/* 150 bps */
    s_printf("bps = 150, ");
    rounddiv = DIV_150;
    baud = B150;
  } else if (DIVISOR < DIV_50) {			/* 110 bps */
    s_printf("bps = 110, ");
    rounddiv = DIV_110;
    baud = B110;
  } else {						/* 50 bps */
    s_printf("bps = 50, ");
    rounddiv = DIV_50;
    baud = B50;
  }
  s_printf("divisor 0x%x -> 0x%lx\n", DIVISOR, rounddiv);

  /* The following does the actual system calls to set the line parameters */
  cfsetispeed(&com->newset, baud);
  cfsetospeed(&com->newset, baud);
  if (debug_level('s') > 7) {
    s_printf("SER%d: iflag=%x oflag=%x cflag=%x lflag=%x\n", com->num,
	    com->newset.c_iflag, com->newset.c_oflag,
	    com->newset.c_cflag, com->newset.c_lflag);
  }
  tcsetattr(com->fd, TCSANOW, &com->newset);
}

static int tty_brkctl(com_t *com, int brkflg)
{
  int ret;
  /* there is change of break state */
  if (brkflg) {
    s_printf("SER%d: Setting BREAK state.\n", com->num);
    tcdrain(com->fd);
    ret = ioctl(com->fd, TIOCSBRK);
  } else {
    s_printf("SER%d: Clearing BREAK state.\n", com->num);
    ret = ioctl(com->fd, TIOCCBRK);
  }
  return ret;
}

static ssize_t tty_write(com_t *com, char *buf, size_t len)
{
  if (com->ro)
    return len;
  return RPT_SYSCALL(write(com->fd, buf, len));   /* Attempt char xmit */
}

static int tty_dtr(com_t *com, int flag)
{
  int ret, control;
  control = TIOCM_DTR;
  if (flag)
    ret = ioctl(com->fd, TIOCMBIS, &control);
  else
    ret = ioctl(com->fd, TIOCMBIC, &control);
  return ret;
}

static int tty_rts(com_t *com, int flag)
{
  int ret, control;
  control = TIOCM_RTS;
  if (flag)
    ret = ioctl(com->fd, TIOCMBIS, &control);
  else
    ret = ioctl(com->fd, TIOCMBIC, &control);
  return ret;
}

/*  Determines if the tty is already locked.  Stolen from uri-dip-3.3.7k
 *  Nice work Uri Blumenthal & Ian Lance Taylor!
 *  [nam = complete path to lock file, return = nonzero if locked]
 */
static int tty_already_locked(char *nam)
{
  int  i = 0, pid = 0;
  FILE *fd = (FILE *)0;

  /* Does the lock file on our device exist? */
  if ((fd = fopen(nam, "r")) == (FILE *)0)
    return 0; /* No, return perm to continue */

  /* Yes, the lock is there.  Now let's make sure at least */
  /* there's no active process that owns that lock.        */
  if(config.tty_lockbinary)
    i = read(fileno(fd), &pid, sizeof(pid)) == sizeof(pid);
  else
    i = fscanf(fd, "%d", &pid);

  (void) fclose(fd);

  if (i != 1) /* Lock file format's wrong! Kill't */
    return 0;

  /* We got the pid, check if the process's alive */
  if (kill(pid, 0) == 0)      /* it found process */
    return 1;                 /* Yup, it's running... */

  /* Dead, we can proceed locking this device...  */
  return 0;
}

/*  Locks or unlocks a terminal line Stolen from uri-dip-3.3.7k
 *  Nice work Uri Blumenthal & Ian Lance Taylor!
 *  [path = device name,
 *   mode: 1 = lock, 2 = reaquire lock, anythingelse = unlock,
 *   return = zero if success, greater than zero for failure]
 */
static int tty_lock(char *path, int mode)
{
  char saved_path[strlen(config.tty_lockdir) + 1 +
                  strlen(config.tty_lockfile) +
                  strlen(path) + 1];
  struct passwd *pw;
  pid_t ime;
  char *slash;

  if (path == NULL) return(0);        /* standard input */
  slash = strrchr(path, '/');
  if (slash == NULL)
    slash = path;
  else
    slash++;

  sprintf(saved_path, "%s/%s%s", config.tty_lockdir, config.tty_lockfile,
	  slash);

  if (mode == 1) {      /* lock */
    {
      FILE *fd;
      if (tty_already_locked(saved_path) == 1) {
        error("attempt to use already locked tty %s\n", saved_path);
        return (-1);
      }
      unlink(saved_path);	/* kill stale lockfiles, if any */
      fd = fopen(saved_path, "w");
      if (fd == (FILE *)0) {
        error("tty: lock: (%s): %s\n", saved_path, strerror(errno));
        return(-1);
      }

      ime = getpid();
      if(config.tty_lockbinary)
	write (fileno(fd), &ime, sizeof(ime));
      else
	fprintf(fd, "%10d\n", (int)ime);

      (void)fclose(fd);
    }

    /* Make sure UUCP owns the lockfile.  Required by some packages. */
    if ((pw = getpwnam(OWNER_LOCKS)) == NULL) {
      error("tty: lock: UUCP user %s unknown!\n", OWNER_LOCKS);
      return(0);        /* keep the lock anyway */
    }

    (void) chown(saved_path, pw->pw_uid, pw->pw_gid);
    (void) chmod(saved_path, 0644);
  }
  else if (mode == 2) { /* re-acquire a lock after a fork() */
    FILE *fd;

     fd = fopen(saved_path,"w");
     if (fd == (FILE *)0) {
      error("tty_lock: reacquire (%s): %s\n",
              saved_path, strerror(errno));
      return(-1);
    }
    ime = getpid();

    if(config.tty_lockbinary)
      write (fileno(fd), &ime, sizeof(ime));
    else
      fprintf(fd, "%10d\n", (int)ime);

    (void) fclose(fd);
    (void) chmod(saved_path, 0444);
    return(0);
  }
  else {    /* unlock */
    FILE *fd;
    int retval;

    fd = fopen(saved_path,"w");
    if (fd == (FILE *)0) {
      error("tty_lock: can't reopen %s to delete: %s\n",
             saved_path, strerror(errno));
      return (-1);
    }

    retval = unlink(saved_path);
    if (retval < 0) {
      error("tty: unlock: (%s): %s\n", saved_path,
             strerror(errno));
      return(-1);
    }
  }
  return(0);
}

static void ser_set_params(com_t *com)
{
  int data = 0;
  com->newset.c_cflag = CS8 | CLOCAL | CREAD;
  com->newset.c_iflag = IGNBRK | IGNPAR;
  com->newset.c_oflag = 0;
  com->newset.c_lflag = 0;

#ifdef __linux__
  com->newset.c_line = 0;
#endif
  com->newset.c_cc[VMIN] = 1;
  com->newset.c_cc[VTIME] = 0;
  if (com->cfg->system_rtscts)
    com->newset.c_cflag |= CRTSCTS;
  tcsetattr(com->fd, TCSANOW, &com->newset);

  if(s2_printf) s_printf("SER%d: do_ser_init: running ser_termios\n", com->num);
  tty_termios(com);			/* Set line settings now */

  /* Pull down DTR and RTS.  This is the most natural for most comm */
  /* devices including mice so that DTR rises during mouse init.    */
  if (!com->cfg->pseudo) {
    data = TIOCM_DTR | TIOCM_RTS;
    if (ioctl(com->fd, TIOCMBIC, &data) && errno == EINVAL) {
      s_printf("SER%d: TIOCMBIC unsupported, setting pseudo flag\n", com->num);
      com->cfg->pseudo = 1;
    }
  }
}

/* This function checks for newly received data and fills the UART
 * FIFO (16550 mode) or receive register (16450 mode).
 *
 * Note: The receive buffer is now a sliding buffer instead of
 * a queue.  This has been found to be more efficient here.
 *
 * [num = port]
 */
static int tty_uart_fill(com_t *com)
{
  int size = 0;

  if (com->fd < 0)
    return 0;

  /* Return if in loopback mode */
  if (com->MCR & UART_MCR_LOOP)
    return 0;

  /* Is it time to do another read() of the serial device yet?
   * The rx_timer is used to prevent system load caused by empty read()'s
   * It also skip the following code block if the receive buffer
   * contains enough data for a full FIFO (at least 16 bytes).
   * The receive buffer is a sliding buffer.
   */
  if (RX_BUF_BYTES(com->num) >= RX_BUFFER_SIZE) {
    if(s3_printf) s_printf("SER%d: Too many bytes (%i) in buffer\n", com->num,
        RX_BUF_BYTES(com->num));
    return 0;
  }

  /* Slide the buffer contents to the bottom */
  rx_buffer_slide(com->num);

  /* Do a block read of data.
   * Guaranteed minimum requested read size of (RX_BUFFER_SIZE - 16)!
   */
  size = RPT_SYSCALL(read(com->fd,
                              &com->rx_buf[com->rx_buf_end],
                              RX_BUFFER_SIZE - com->rx_buf_end));
  if (size <= 0)
    return 0;
  if(s3_printf) s_printf("SER%d: Got %i bytes, %i in buffer\n", com->num,
        size, RX_BUF_BYTES(com->num));
  if (debug_level('s') >= 9) {
    int i;
    for (i = 0; i < size; i++)
      s_printf("SER%d: Got data byte: %#x\n", com->num,
          com->rx_buf[com->rx_buf_end + i]);
  }
  com->rx_buf_end += size;
  return size;
}

static void async_serial_run(void *arg)
{
  com_t *com = arg;
  int size;
  s_printf("SER%d: Async notification received\n", com->num);
  size = tty_uart_fill(com);
  if (size > 0)
    receive_engine(com->num, size);
}

static int ser_open_existing(com_t *com)
{
  struct stat st;
  int err, io_sel = 0, oflags = O_NONBLOCK;

  err = stat(com->cfg->dev, &st);
  if (err) {
    error("SERIAL: stat(%s) failed: %s\n", com->cfg->dev,
	    strerror(errno));
    com->fd = -2;
    return -1;
  }
  if (S_ISFIFO(st.st_mode)) {
    s_printf("SER%i: %s is fifo, setting pseudo flag\n", com->num,
	    com->cfg->dev);
    com->is_file = TRUE;
    com->ro = TRUE;
    com->cfg->pseudo = TRUE;
    oflags |= O_RDONLY;
    io_sel = 1;
  } else {
    com->ro = FALSE;
    if (S_ISREG(st.st_mode)) {
      s_printf("SER%i: %s is file, setting pseudo flag\n", com->num,
	    com->cfg->dev);
      com->is_file = TRUE;
      com->cfg->pseudo = TRUE;
      oflags |= O_WRONLY | O_APPEND;
    } else {
      oflags |= O_RDWR;
      io_sel = 1;
    }
  }

  com->fd = RPT_SYSCALL(open(com->cfg->dev, oflags));
  if (com->fd < 0) {
    error("SERIAL: Unable to open device %s: %s\n",
      com->cfg->dev, strerror(errno));
    return -1;
  }

  if (!com->is_file && !isatty(com->fd)) {
    s_printf("SERIAL: Serial port device %s is not a tty\n",
      com->cfg->dev);
    com->is_file = TRUE;
    com->cfg->pseudo = TRUE;
  }

  if (!com->is_file) {
    RPT_SYSCALL(tcgetattr(com->fd, &com->oldset));
    RPT_SYSCALL(tcgetattr(com->fd, &com->newset));

    if (com->cfg->low_latency) {
      struct serial_struct ser_info;
      int err = ioctl(com->fd, TIOCGSERIAL, &ser_info);
      if (err) {
        error("SER%d: failure getting serial port settings, %s\n",
          com->num, strerror(errno));
      } else {
        ser_info.flags |= ASYNC_LOW_LATENCY;
        err = ioctl(com->fd, TIOCSSERIAL, &ser_info);
        if (err)
          error("SER%d: failure setting low_latency flag, %s\n",
            com->num, strerror(errno));
        else
          s_printf("SER%d: low_latency flag set\n", com->num);
      }
    }
    ser_set_params(com);
  }
  if (io_sel)
    add_to_io_select(com->fd, async_serial_run, (void *)com);
  return 0;
}

/* This function opens ONE serial port for DOSEMU.  Normally called only
 * by do_ser_init below.   [num = port, return = file descriptor]
 */
static int tty_open(com_t *com)
{
  int err;

  if (com->fd != -1)
    return -1;
  s_printf("SER%d: Running ser_open, %s, fd=%d\n", com->num,
	com->cfg->dev, com->fd);

  if (com->fd != -1)
    return (com->fd);

  if (com->cfg->virtual)
  {
    /* don't try to remove any lock: they don't make sense for ttyname(0) */
    s_printf("Opening Virtual Port\n");
    com->dev_locked = FALSE;
  } else if (config.tty_lockdir[0]) {
    if (tty_lock(com->cfg->dev, 1) >= 0) {		/* Lock port */
      /* We know that we have access to the serial port */
      /* If the port is used for a mouse, then don't lock, because
       * the use of the mouse serial port can be switched between processes,
       * such as on Linux virtual consoles.
       */
      com->dev_locked = TRUE;
    } else {
      /* The port is in use by another process!  Don't touch the port! */
      com->dev_locked = FALSE;
      com->fd = -2;
      return(-1);
    }
  } else {
    s_printf("Warning: Port locking disabled in the config.\n");
    com->dev_locked = FALSE;
  }

  err = access(com->cfg->dev, F_OK);
  if (!err) {
    err = ser_open_existing(com);
    if (err)
      goto fail_unlock;
  } else {
    com->fd = open(com->cfg->dev, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (com->fd == -1)
      goto fail_unlock;
  }

  modstat_engine(com->num);
  return com->fd;

  close(com->fd);
  /* fall through */
fail_unlock:
  if (com->dev_locked && tty_lock(com->cfg->dev, 0) >= 0) /* Unlock port */
    com->dev_locked = FALSE;

  com->fd = -2; // disable permanently
  return -1;
}

/* This function closes ONE serial port for DOSEMU.  Normally called
 * only by do_ser_init below.   [num = port, return = file error code]
 */
static int tty_close(com_t *com)
{
  int i;
  if (com->fd < 0)
    return -1;
  s_printf("SER%d: Running ser_close\n", com->num);
  remove_from_io_select(com->fd);

  /* save current dosemu settings of the file and restore the old settings
   * before closing the file down.
   */
  if (!com->is_file) {
    RPT_SYSCALL(tcgetattr(com->fd, &com->newset));
    RPT_SYSCALL(tcsetattr(com->fd, TCSANOW, &com->oldset));
  }
  i = RPT_SYSCALL(close(com->fd));
  com->fd = -1;

  /* Clear the lockfile from DOSEMU */
  if (com->dev_locked) {
    if (tty_lock(com->cfg->dev, 0) >= 0)
      com->dev_locked = FALSE;
  }
  return (i);
}

static int tty_get_msr(com_t *com)
{
  int control, err;
  err = ioctl(com->fd, TIOCMGET, &control);
  if (err)
    return 0;
  return (((control & TIOCM_CTS) ? UART_MSR_CTS : 0) |
          ((control & TIOCM_DSR) ? UART_MSR_DSR : 0) |
          ((control & TIOCM_RNG) ? UART_MSR_RI : 0) |
          ((control & TIOCM_CAR) ? UART_MSR_DCD : 0));
}


struct serial_drv tty_drv = {
  tty_rx_buffer_dump,
  tty_tx_buffer_dump,
  tty_get_tx_queued,
  tty_termios,
  tty_brkctl,
  tty_write,
  tty_dtr,
  tty_rts,
  tty_open,
  tty_close,
  tty_uart_fill,
  tty_get_msr,
  "tty_io"
};

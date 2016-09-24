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

/*
 * Serial mouse
 *
 * Author: Stas Sergeev, with some help from gpm code
 */
#include <string.h>
#include "emu.h"
#include "init.h"
#include "utilities.h"
#include "ser_defs.h"
#include "sermouse.h"

struct serm_state {
  int enabled;
  int nrst;
  int opened;
  int div;
  char but;
  int x, y;
};
static struct serm_state serm;

/*========================================================================*/
/*
 * When repeating, it is important not to try to repeat more bits of dx and
 * dy than the protocol can handle.  Otherwise, you may end up repeating the
 * low bits of a large value, which causes erratic motion.
 */
/*========================================================================*/
static int limit_delta(int delta, int min, int max)
{
   return delta > max ? max :
          delta < min ? min : delta;
}

static int ser_mouse_accepts(void *udata)
{
  com_t *com = udata;
  if (!serm.opened)
    return 0;
  if (!com) {
    dosemu_error("sermouse NULL udata\n");
    return 0;
  }
  return com->cfg->mouse;
}

static int add_buf(com_t *com, const char *buf, int len)
{
  if (!serm.enabled || !serm.opened || serm.div != DIV_1200)
    return 0;
  if (RX_BUF_BYTES(com->num) + len > RX_BUFFER_SIZE) {
    if(s3_printf) s_printf("SER%d: Too many bytes (%i) in buffer\n", com->num,
        RX_BUF_BYTES(com->num));
    return 0;
  }

  /* Slide the buffer contents to the bottom */
  rx_buffer_slide(com->num);

  memcpy(&com->rx_buf[com->rx_buf_end], buf, len);
  if (debug_level('s') >= 9) {
    int i;
    for (i = 0; i < len; i++)
      s_printf("SER%d: Got mouse data byte: %#x\n", com->num,
          com->rx_buf[com->rx_buf_end + i]);
  }
  com->rx_buf_end += len;
  receive_engine(com->num, len);
  return len;
}

static void ser_mouse_move_buttons(int lbutton, int mbutton, int rbutton,
	void *udata)
{
  com_t *com = udata;
  char buf[3] = {0x40, 0, 0};

  s_printf("SERM: buttons %i %i %i\n", lbutton, mbutton, rbutton);
  serm.but = 0;
  if (lbutton)
    serm.but |= 0x20;
  if (rbutton)
    serm.but |= 0x10;
  buf[0] |= serm.but;
  /* change in mbutton is signalled by sending the prev state */

  add_buf(com, buf, sizeof(buf));
}

static void ser_mouse_move_mickeys(int dx, int dy, void *udata)
{
  com_t *com = udata;
  char buf[3] = {0x40, 0, 0};

  s_printf("SERM: movement %i %i\n", dx, dy);
  buf[0] |= serm.but;
  dx = limit_delta(dx, -128, 127);
  buf[1]   = dx & ~0xC0;
  buf[0]  |= (dx & 0xC0) >> 6;

  dy = limit_delta(dy, -128, 127);
  buf[2] = dy & ~0xC0;
  buf[0] |= (dy & 0xC0) >> 4;

  add_buf(com, buf, sizeof(buf));
}

static void ser_mouse_move_relative(int dx, int dy, int x_range, int y_range,
	void *udata)
{
  /* oops, ignore ranges and use hardcoded ratio for now */
  ser_mouse_move_mickeys(dx, dy * 2, udata);
}

static void ser_mouse_move_absolute(int x, int y, int x_range, int y_range,
	void *udata)
{
  int dx = x - serm.x;
  int dy = y - serm.y;
  ser_mouse_move_relative(dx, dy, x_range, y_range, udata);
  serm.x = x;
  serm.y = y;
}

static void ser_mouse_drag_to_corner(int x_range, int y_range, void *udata)
{
  int i;
  for (i = 0; i < 10; i++)
    ser_mouse_move_mickeys(-100, -100, udata);
  serm.x = serm.y = 0;
}

struct mouse_drv ser_mouse = {
  NULL, /* init */
  ser_mouse_accepts,
  ser_mouse_move_buttons,
  ser_mouse_move_relative,
  ser_mouse_move_mickeys,
  ser_mouse_move_absolute,
  ser_mouse_drag_to_corner,
  NULL, /* ser_mouse_enable_native_cursor */
  "serial mouse"
};

CONSTRUCTOR(static void serial_mouse_register(void))
{
  register_mouse_driver(&ser_mouse);
}


static void serm_rx_buffer_dump(com_t *com)
{
}

static void serm_tx_buffer_dump(com_t *com)
{
}

static int serm_get_tx_queued(com_t *com)
{
  return 0;
}

static void serm_termios(com_t *com)
{
  serm.div = ((com->dlm << 8) | com->dll);
  s_printf("SERM: set div to %i\n", serm.div);
}

static int serm_brkctl(com_t *com, int flag)
{
  return 0;
}

static ssize_t serm_write(com_t *com, char *buf, size_t len)
{
  return 0;
}

static int serm_dtr(com_t *com, int flag)
{
  serm.enabled = flag;
  modstat_engine(com->num);	// update DSR
  return 0;
}

static int serm_rts(com_t *com, int flag)
{
  if (flag && !serm.nrst) {
    const char *id = "M3";
    /* Many mouse drivers require this, they detect for Framing Errors
     * coming from the mouse, during initialization, usually right after
     * the LCR register is set, so this is why this line of code is here
     */
    com->LSR |= UART_LSR_FE; 		/* Set framing error */
    if(s3_printf) s_printf("SERM: framing error\n");
    rx_buffer_slide(com->num);
    if (com->rx_buf_end >= com->rx_fifo_size) {
      error("SERM: fifo overflow\n");
      return 0;
    }
    com->rx_buf[com->rx_buf_end++] = 0;
    serial_int_engine(com->num, LS_INTR);		/* Update interrupt status */
    add_buf(com, id, strlen(id));
  }
  serm.nrst = flag;
  modstat_engine(com->num);	// update DSR
  return 0;
}

static int serm_open(com_t *com)
{
  s_printf("SERM: open for port %i\n", com->num);
  mousedrv_set_udata(ser_mouse.name, com);
  serm.opened = 1;
  serm.but = 0;
  return 1;
}

static int serm_close(com_t *com)
{
  serm.opened = 0;
  return 0;
}

static int serm_uart_fill(com_t *com)
{
  return 0;
}

static int serm_get_msr(com_t *com)
{
  return ((serm.enabled && serm.nrst) ? UART_MSR_DSR : 0);
}


struct serial_drv serm_drv = {
  serm_rx_buffer_dump,
  serm_tx_buffer_dump,
  serm_get_tx_queued,
  serm_termios,
  serm_brkctl,
  serm_write,
  serm_dtr,
  serm_rts,
  serm_open,
  serm_close,
  serm_uart_fill,
  serm_get_msr,
  "serial_mouse_tty"
};

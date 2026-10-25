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
 * Null-modem connection
 *
 * Author: stsp
 */
#include "emu.h"
#include "init.h"
#include "utilities.h"
#include "ser_defs.h"
#include "nullmm.h"

static int nullmm_get_tx_queued(com_t *c)
{
  return 0;
}

static void nullmm_termios(com_t *c)
{
}

static int nullmm_brkctl(com_t *c, int flag)
{
  return 0;
}

static ssize_t nullmm_write(com_t *c, char *buf, size_t len)
{
  int idx = get_com_idx(c->cfg->nullmm);
  if (idx == -1)
    return -1;
  return serial_rx_push(&com[idx], buf, len);
}

static int nullmm_dtr(com_t *c, int flag)
{
  return 0;
}

static int nullmm_rts(com_t *c, int flag)
{
  return 0;
}

static int nullmm_open(com_t *c)
{
  return 1;
}

static int nullmm_close(com_t *c)
{
  return 0;
}

static int nullmm_uart_fill(com_t *c)
{
  return 0;
}

static int nullmm_get_msr(com_t *c)
{
  com_t *c2;
  int idx = get_com_idx(c->cfg->nullmm);
  if (idx == -1)
    return -1;
  c2 = &com[idx];
  unsigned char msr = UART_MSR_DCD;
  if (c2->MCR & UART_MCR_DTR)
    msr |= UART_MSR_DSR;
  if (c2->MCR & UART_MCR_RTS)
    msr |= UART_MSR_CTS;
  return msr;
}


struct serial_drv nullmm_drv = {
  nullmm_get_tx_queued,
  nullmm_termios,
  nullmm_brkctl,
  nullmm_write,
  nullmm_dtr,
  nullmm_rts,
  nullmm_open,
  nullmm_close,
  nullmm_uart_fill,
  nullmm_get_msr,
  NULL,
  "serial_mouse_tty"
};

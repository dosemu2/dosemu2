/*
 * dosemu2 vmodem serial driver backed by the external libmodemu2k
 * library (https://github.com/theimpossibleastronaut/modemu2k), a
 * maintained continuation of the modemu code this plugin used to
 * vendor.
 *
 * The library runs in its "app I/O" mode: no pty is involved at all,
 * the emulated UART hands bytes straight to it and takes them back.
 * All Hayes AT parsing, telnet negotiation and socket I/O live in the
 * library; this file only pumps its state machine from dosemu2's event
 * loop and maps the two ends onto struct serial_drv.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <poll.h>
#include <time.h>

#include <modemu2k.h>

#include "emu.h"
#include "init.h"
#include "serial.h"
#include "ser_defs.h"
#include "ioselect.h"

struct vmodem {
    m2k_t *ctx;
    /* fds currently registered with dosemu2's io_select loop */
    int reg_fds[M2K_MAX_POLLFDS];
    size_t nreg;
    int in_use;
};

static struct vmodem vmodems[MAX_SER];

static void modemu_async_callback(int fd, void *arg);

static void m2k_log_cb(const char *msg, void *userdata)
{
    s_printf("MODEMU2K: %s", msg);
}

static void unregister_fds(struct vmodem *vm)
{
    size_t i;
    for (i = 0; i < vm->nreg; i++)
	remove_from_io_select(vm->reg_fds[i]);
    vm->nreg = 0;
}

static void register_fds(com_t *c)
{
    struct vmodem *vm = &vmodems[c->num];
    struct pollfd fds[M2K_MAX_POLLFDS];
    size_t nfds = M2K_MAX_POLLFDS;
    int timeout_ms;
    size_t i, n = 0;

    if (m2k_get_pollfds(vm->ctx, fds, &nfds, &timeout_ms) != M2K_OK) {
	/* Can't happen with a valid ctx today; scream rather than
	 * silently losing every registration (invisible stall). */
	error("MODEMU2K: m2k_get_pollfds failed, modem stalled\n");
	return;
    }
    for (i = 0; i < nfds; i++) {
	/* A pollfd with no events set means the library has nothing to
	 * do with this fd right now (e.g. the socket while our own
	 * receive queue is full). add_to_io_select() has no event
	 * mask of its own, and dosemu2's select() loop is level
	 * triggered, so registering it anyway means: the fd is still
	 * readable, the callback fires immediately, finds nothing to
	 * do, and re-registers it -- a busy spin. Skip it; the next
	 * tick (modemu_tick(), 20-100/sec) retries once conditions
	 * change. */
	if (!fds[i].events)
	    continue;
	add_to_io_select(fds[i].fd, modemu_async_callback, c);
	vm->reg_fds[n++] = fds[i].fd;
    }
    vm->nreg = n;
}

static int fd_still_registered(struct vmodem *vm, int fd)
{
    size_t i;
    for (i = 0; i < vm->nreg; i++)
	if (vm->reg_fds[i] == fd)
	    return 1;
    return 0;
}

/* Move what the library has for the DTE into the UART receive queue.
 * Bounded by the room there: what does not fit stays in the library and
 * comes over on a later tick. */
static void drain_rx(com_t *c)
{
    struct vmodem *vm = &vmodems[c->num];
    char buf[RX_BUFFER_SIZE];
    size_t len;
    int room;

    while ((room = RX_BUFFER_SIZE - RX_BUF_BYTES(c->num)) > 0) {
	if (m2k_read_to_app(vm->ctx, buf, room, &len) != M2K_OK || !len)
	    break;
	serial_rx_push(c, buf, len);
    }
}

/* Run one iteration of the modem state machine: collect the events the
 * library is waiting for (0-timeout poll) and step it once.
 *
 * The io_select registrations are dropped before stepping and rebuilt
 * afterwards: m2k_step() may close fds (e.g. the TCP socket on hangup),
 * and iosel requires an fd to still be open when it is removed. The
 * fd set also changes across state transitions (the socket appears on
 * connect), so rebuilding
 * from m2k_get_pollfds() after each step keeps the loop accurate.
 */
static void pump(com_t *c)
{
    struct vmodem *vm = &vmodems[c->num];
    struct pollfd fds[M2K_MAX_POLLFDS];
    size_t nfds = M2K_MAX_POLLFDS;
    int timeout_ms;

    unregister_fds(vm);
    if (m2k_get_pollfds(vm->ctx, fds, &nfds, &timeout_ms) == M2K_OK) {
	if (nfds > 0)
	    poll(fds, nfds, 0);
	m2k_step(vm->ctx, fds, nfds);
    }
    drain_rx(c);
    register_fds(c);
}

static void modemu_async_callback(int fd, void *arg)
{
    com_t *c = arg;

    pump(c);
    /* If the event was the fd's own death (socket closed by hangup or
     * the peer), pump() removed it and did not re-add it -- completing it
     * then would unmask an fd number we no longer own (it may already
     * belong to another subsystem). */
    if (fd_still_registered(&vmodems[c->num], fd))
	ioselect_complete(fd);
}

/* Called from the serial engine tick (20-100/sec). Drives the parts of
 * the state machine that need time rather than fd activity: dial
 * progress and timeouts, the "+++" guard time, RING cadence and S7
 * expiry. It is also what retries drain_rx() after
 * the UART receive queue has run full. */
static void modemu_tick(com_t *c)
{
    pump(c);
}

static ssize_t modemu_write(com_t *c, char *buf, size_t len)
{
    struct vmodem *vm = &vmodems[c->num];
    size_t consumed = 0;

    m2k_write_from_app(vm->ctx, buf, len, &consumed);
    /* Deliberately no pump() here. A byte reaches the wire on a later
     * step, but put_tx() calls this once per byte and pump() rebuilds
     * the whole io_select registration, so pumping here would do that
     * per byte. modemu_tick() covers it at 20-100/sec, and
     * modemu_close() flushes before the connection goes away. */
    return consumed;
}

static int modemu_get_tx_queued(com_t *c)
{
    m2k_t *ctx = vmodems[c->num].ctx;
    size_t pending;

    /* Reported in every state on purpose. Returning 0 while offline
     * would be the obvious way to avoid a count that cannot drain, but
     * it removes the only back-pressure there is: put_tx() drops a byte
     * outright when this driver cannot take it, so DOS has to be told
     * to stop before that happens. The library discards socket-bound
     * data when it leaves ONLINE (>= 0.2.5), so the count does reach
     * zero on its own and the special case is not needed.
     *
     * The figure is post-telnet-encoding wire bytes (line mode adds
     * an LF per CR; binary mode doubles 0xFF), so it can run ahead of
     * what c->tx_cnt -- the count the serial core itself is keeping --
     * thinks is outstanding. Reporting more than that only trips
     * update_tx_cnt()'s consistency check and, once the difference
     * exceeds TX_QUEUE_THRESHOLD, holds THRE off longer than the real
     * backlog warrants; clamp to what the host already believes. */
    pending = m2k_pending_to_line(ctx);
    if (pending > (size_t) c->tx_cnt)
	pending = c->tx_cnt;
    return (int) pending;
}

static int modemu_get_msr(com_t *c)
{
    int msr = UART_MSR_DSR | UART_MSR_CTS;

    if (m2k_has_carrier(vmodems[c->num].ctx))
	msr |= UART_MSR_DCD;
    return msr;
}

static int modemu_dtr(com_t *c, int flag)
{
    struct vmodem *vm = &vmodems[c->num];

    if (flag) {
	m2k_set_dtr(vm->ctx, 1);
	return 0;
    }
    /* Dropping DTR can close the socket, and iosel requires an fd to
     * still be open when it is removed -- so drop the registrations
     * first and rebuild after, the same order pump() uses. This runs
     * straight off the MCR write rather than through pump(), so
     * nothing else would do it.
     *
     * m2k_set_dtr() alone only hangs up a call already online (per its
     * docs); it is a no-op on the classic DOS abort of pulsing DTR low
     * while a dial is still in progress. m2k_hangup() covers all three
     * phases (dial, answer wait, live call) and is a no-op when idle. */
    unregister_fds(vm);
    m2k_set_dtr(vm->ctx, 0);
    m2k_hangup(vm->ctx);
    register_fds(c);
    return 0;
}

static int modemu_rts(com_t *c, int flag)
{
    m2k_set_rts(vmodems[c->num].ctx, flag);
    return 0;
}

static void modemu_termios(com_t *c)
{
}

static int modemu_brkctl(com_t *c, int flag)
{
    return 0;
}

static int modemu_uart_fill(com_t *c)
{
    return 0;
}

static int modemu_open(com_t *c)
{
    struct vmodem *vm = &vmodems[c->num];

    if (vm->in_use) {
	error("MODEMU2K: port %d already initialized\n", c->num);
	config.exitearly = 1;
	return -1;
    }
    vm->ctx = m2k_new();
    if (!vm->ctx) {
	error("MODEMU2K: out of memory\n");
	config.exitearly = 1;
	return -1;
    }
    m2k_set_log_fn(vm->ctx, m2k_log_cb, NULL);
    m2k_setup_app_io(vm->ctx);
    vm->in_use = 1;
    register_fds(c);
    return 1;
}

static int modemu_close(com_t *c)
{
    struct vmodem *vm = &vmodems[c->num];
    struct timespec ts = { 0, 2 * 1000 * 1000 };
    int i;

    if (!vm->in_use)
	return 0;
    /* Push whatever DOS wrote last out to the line before tearing the
     * connection down: sending a byte needs a further m2k_step() after
     * the one that takes it in, and at shutdown the serial tick has
     * usually stopped coming. Bounded so an unresponsive peer cannot
     * hold up dosemu's exit. */
    for (i = 0; i < 100; i++) {
	/* Nothing drains toward the line once the call is down, so stop
	 * rather than sleeping out the whole bound for no purpose. */
	if (i > 0 && (!m2k_is_online(vm->ctx)
		      || !m2k_pending_to_line(vm->ctx)))
	    break;
	pump(c);
	nanosleep(&ts, NULL);
    }
    unregister_fds(vm);
    m2k_free(vm->ctx);
    vm->ctx = NULL;
    vm->in_use = 0;
    return 0;
}

static struct serial_drv modemu_drv = {
    modemu_get_tx_queued,
    modemu_termios,
    modemu_brkctl,
    modemu_write,
    modemu_dtr,
    modemu_rts,
    modemu_open,
    modemu_close,
    modemu_uart_fill,
    modemu_get_msr,
    modemu_tick,
    "vmodem"
};

CONSTRUCTOR(static void modemu_register(void))
{
    serial_register_drv(&modemu_drv);
}

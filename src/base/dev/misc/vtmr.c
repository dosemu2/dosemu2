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
 * Purpose: virtual timer device extension
 *
 * Author: Stas Sergeev.
 *
 * Note: this code shows how to run coopth in a separate thread.
 * Many things are written here just as an example.
 * Perhaps in the future the example should be moved elsewhere.
 */
#include <stdint.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include "port.h"
#include "pic.h"
#include "cpu.h"
#include "int.h"
#include "bitops.h"
#include "emudpmi.h"
#include "emu.h"
#include "coopth.h"
#if MULTICORE_EXAMPLE
#include "lowmem.h"
#include "hlt.h"
#endif
#include "utilities.h"
#include "timers.h"
#include "chipset.h"
#include "vint.h"
#include "vtmr.h"

#define VTMR_FIRST_PORT 0x550
#define VTMR_VPEND_PORT VTMR_FIRST_PORT
#define VTMR_IRR_PORT (VTMR_FIRST_PORT + 2)
#define VTMR_ACK_PORT (VTMR_FIRST_PORT + 3)
#define VTMR_REQUEST_PORT (VTMR_FIRST_PORT + 4)
#define VTMR_MASK_PORT (VTMR_FIRST_PORT + 5)
#define VTMR_UNMASK_PORT (VTMR_FIRST_PORT + 6)
#define VTMR_LATCH_PORT (VTMR_FIRST_PORT + 7)
#define VTMR_TOTAL_PORTS 8

static uint16_t vtmr_irr;
static uint16_t vtmr_imr;
static uint16_t vtmr_pirr;

static pthread_t vtmr_thr;
static sem_t vtmr_sem;
static int latch_tid;
static pthread_mutex_t irr_mtx = PTHREAD_MUTEX_INITIALIZER;
#if MULTICORE_EXAMPLE
static int smi_tid;
static char *rmstack;
static uint16_t hlt_off;
#endif

struct vthandler {
    int (*handler)(int);
    int (*latch)(void);
    int vint;
    int done_pred;
    pthread_mutex_t done_mtx;
    pthread_cond_t done_cnd;
};
struct vthandler vth[VTMR_MAX];

struct vint_presets {
    uint8_t irq;
    uint8_t orig_irq;
    uint8_t interrupt;
};
static struct vint_presets vip[VTMR_MAX] = {
    [VTMR_PIT] = { .irq = VTMR_IRQ, .orig_irq = 0,
            .interrupt = VTMR_INTERRUPT },
    [VTMR_RTC] = { .irq = VRTC_IRQ, .orig_irq = 8,
            .interrupt = VRTC_INTERRUPT },
};

static int do_vtmr_raise(int timer);

static Bit8u vtmr_irr_read(ioport_t port, void *arg)
{
    return vtmr_irr;
}

static Bit16u vtmr_vpend_read(ioport_t port, void *arg)
{
    /* clang has __atomic_swap() */
    return __atomic_exchange_n(&vtmr_pirr, 0, __ATOMIC_RELAXED);
}

static void post_req(int timer)
{
    if (vth[timer].handler) {
        int rc = vth[timer].handler(0);
        if (rc)
            do_vtmr_raise(timer);
    }
    h_printf("vtmr: post-REQ on %i, irr=%x\n", timer, vtmr_irr);
}

static void vtmr_io_write(ioport_t port, Bit8u value, void *arg)
{
    int masked = (value >> 7) & 1;
    int timer = value & 0x7f;
    uint16_t msk = 1 << timer;

    if (timer >= VTMR_MAX)
        return;
    switch (port) {
    case VTMR_REQUEST_PORT:
        if (!masked) {
            uint16_t irr;
            pthread_mutex_lock(&irr_mtx);
            irr = __sync_fetch_and_or(&vtmr_irr, msk);
            if (!(irr & msk)) {
                if (!(vtmr_imr & msk))
                    pic_request(vip[timer].irq);
            } else {
                error("vtmr %i already requested\n", timer);
            }
            pthread_mutex_unlock(&irr_mtx);
        } else {
            pic_untrigger(vip[timer].orig_irq);
            pic_request(vip[timer].orig_irq);
            post_req(timer);
        }
        h_printf("vtmr: REQ on %i, irr=%x, pirr=%x masked=%i\n", timer,
                vtmr_irr, vtmr_pirr, masked);
        break;
    case VTMR_MASK_PORT: {
        uint16_t imr = __sync_fetch_and_or(&vtmr_imr, msk);
        if (!(imr & msk)) {
            if (vtmr_irr & msk)
                pic_untrigger(vip[timer].irq);
        }
        break;
    }
    case VTMR_UNMASK_PORT: {
        uint16_t imr = __sync_fetch_and_and(&vtmr_imr, ~msk);
        if (imr & msk) {
            if (vtmr_irr & msk)
                pic_request(vip[timer].irq);
        }
        break;
    }
    case VTMR_ACK_PORT: {
        uint16_t irr;
        pthread_mutex_lock(&irr_mtx);
        irr = __sync_fetch_and_and(&vtmr_irr, ~msk);
        if (irr & msk) {
            pic_untrigger(vip[timer].irq);
            if (vth[timer].handler) {
                int rc = vth[timer].handler(masked);
                if (rc)
                    do_vtmr_raise(timer);
            }
        } else {
            error("vtmr %i not requested\n", timer);
        }
        pthread_mutex_unlock(&irr_mtx);
        h_printf("vtmr: ACK on %i, irr=%x pirr=%x\n", timer, vtmr_irr,
                vtmr_pirr);
        break;
    }
    case VTMR_LATCH_PORT: {
        int from_irq = masked;
        if (vth[timer].latch) {
            int rc = vth[timer].latch();
            if (rc && !from_irq) {  // underflow seen not from IRQ
                uint16_t irr;
                pthread_mutex_lock(&irr_mtx);
                irr = __sync_fetch_and_and(&vtmr_irr, ~msk);
                if (irr & msk) {
                    pic_untrigger(vip[timer].irq);
                    if (vth[timer].handler) {
                        rc = vth[timer].handler(1);
                        if (rc)
                            do_vtmr_raise(timer);
                    }
                }
                pthread_mutex_unlock(&irr_mtx);
            }
        }
        h_printf("vtmr: LATCH on %i, irr=%x pirr=%x\n", timer, vtmr_irr,
                vtmr_pirr);
        break;
    }
    }

}

static void do_ack(int timer, int masked)
{
    port_outb(VTMR_ACK_PORT, timer | (masked << 7));
}

static void ack_handler(int vint, int masked)
{
    int i;

    for (i = 0; i < VTMR_MAX; i++) {
        if (vth[i].vint == vint) {
            do_ack(i, masked);
            break;
        }
    }
}

static void do_mask(int timer)
{
    port_outb(VTMR_MASK_PORT, timer);
}

static void do_unmask(int timer)
{
    port_outb(VTMR_UNMASK_PORT, timer);
}

static void mask_handler(int vint, int masked)
{
    int i;

    for (i = 0; i < VTMR_MAX; i++) {
        if (vth[i].vint == vint) {
            if (masked)
                do_mask(i);
            else
                do_unmask(i);
            break;
        }
    }
}

int vtmr_pre_irq_dpmi(uint8_t *imr)
{
    int masked = vint_is_masked(vth[VTMR_PIT].vint, imr);
    do_mask(VTMR_PIT);
    do_ack(VTMR_PIT, masked);
    vint_post_irq_dpmi(vth[VTMR_PIT].vint, masked);
    return masked;
}

void vtmr_post_irq_dpmi(int masked)
{
    do_unmask(VTMR_PIT);
}

int vrtc_pre_irq_dpmi(uint8_t *imr)
{
    int masked = vint_is_masked(vth[VTMR_RTC].vint, imr);
    do_mask(VTMR_RTC);
    do_ack(VTMR_RTC, masked);
    vint_post_irq_dpmi(vth[VTMR_RTC].vint, masked);
    return masked;
}

void vrtc_post_irq_dpmi(int masked)
{
    do_unmask(VTMR_RTC);
}

static int vtmr_is_masked(int timer)
{
    uint8_t imr[2] = { [0] = port_inb(0x21), [1] = port_inb(0xa1) };
    uint16_t real_imr = (imr[1] << 8) | imr[0];
    return ((imr[0] & 4) || !!(real_imr & (1 << vip[timer].irq)));
}

static void vtmr_smi(void *arg)
{
    int timer;
    uint16_t pirr = port_inw(VTMR_VPEND_PORT);

    while ((timer = find_bit(pirr)) != -1) {
        int masked = vtmr_is_masked(timer);
        pirr &= ~(1 << timer);
        port_outb(VTMR_REQUEST_PORT, timer | (masked << 7));
        if (!masked)
            port_outb(0x4d2, 1);  // set fake IRR
        pthread_mutex_lock(&vth[timer].done_mtx);
        vth[timer].done_pred = 1;
        pthread_mutex_unlock(&vth[timer].done_mtx);
        pthread_cond_signal(&vth[timer].done_cnd);
    }
}

static void vtmr_latch_smi(void *arg)
{
  uint16_t isr;
  int from_irq;
  int timer = (uintptr_t)arg;

  assert(timer < VTMR_MAX);
  port_outb(0x20, 0xb);
  isr = port_inb(0x20);
  port_outb(0xa0, 0xb);
  isr = (port_inb(0xa0) << 8);
  from_irq = !!(isr & (1 << vip[timer].orig_irq));
  port_outb(VTMR_LATCH_PORT, timer | (from_irq << 7));
}

#if MULTICORE_EXAMPLE
static void thr_cleanup(void *arg)
{
    coopth_done();
    lowmem_free(rmstack);
}
#endif

#define RMSTACK_SIZE 32

static void *vtmr_thread(void *arg)
{
#if MULTICORE_EXAMPLE
    /* init coopth in new thread */
    coopth_init();
    pthread_cleanup_push(thr_cleanup, NULL);
    smi_tid = coopth_create("vtmr smi", vtmr_smi);
    cpu_reset();
    _SS = DOSEMU_LMHEAP_SEG;
    _SP = DOSEMU_LMHEAP_OFFS_OF(rmstack) + RMSTACK_SIZE;
    _CS = BIOS_HLT_BLK_SEG;
    _IP = hlt_off;
    clear_IF();

    /* run our fake core */
    while (1)
        run_vm86();
    pthread_cleanup_pop(1);
#else
    while (1) {
        sem_wait(&vtmr_sem);
        vtmr_smi(NULL);
    }
#endif
    return NULL;
}

#if MULTICORE_EXAMPLE
static void vtmr_hlt(Bit16u idx, HLT_ARG(arg))
{
    sem_wait(&vtmr_sem);
    coopth_start(smi_tid, NULL);
}
#endif

void vtmr_init(void)
{
    emu_iodev_t io_dev = {};
#if MULTICORE_EXAMPLE
    emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
#endif
    int i;

    io_dev.write_portb = vtmr_io_write;
    io_dev.read_portb = vtmr_irr_read;
    io_dev.read_portw = vtmr_vpend_read;
    io_dev.start_addr = VTMR_FIRST_PORT;
    io_dev.end_addr = VTMR_FIRST_PORT + VTMR_TOTAL_PORTS - 1;
    io_dev.handler_name = "virtual timer";
    port_register_handler(io_dev, 0);

#if MULTICORE_EXAMPLE
    rmstack = lowmem_alloc(RMSTACK_SIZE);
    hlt_hdlr.name = "vtmr sleep";
    hlt_hdlr.func = vtmr_hlt;
    hlt_off = hlt_register_handler_vm86(hlt_hdlr);
#endif

    latch_tid = coopth_create("vtmr latch smi", vtmr_latch_smi);
    coopth_set_ctx_handlers(latch_tid, sig_ctx_prepare, sig_ctx_restore, NULL);

    sem_init(&vtmr_sem, 0, 0);
    for (i = 0; i < VTMR_MAX; i++) {
        pthread_mutex_init(&vth[i].done_mtx, NULL);
        pthread_cond_init(&vth[i].done_cnd, NULL);
        vth[i].done_pred = 1;
    }
    pthread_create(&vtmr_thr, NULL, vtmr_thread, NULL);
#if defined(HAVE_PTHREAD_SETNAME_NP) && defined(__GLIBC__)
    pthread_setname_np(vtmr_thr, "dosemu: vtmr");
#endif
}

void vtmr_done(void)
{
    int i;

    pthread_cancel(vtmr_thr);
    pthread_join(vtmr_thr, NULL);
    sem_destroy(&vtmr_sem);
    for (i = 0; i < VTMR_MAX; i++) {
        pthread_mutex_destroy(&vth[i].done_mtx);
        pthread_cond_destroy(&vth[i].done_cnd);
    }
#if MULTICORE_EXAMPLE
    lowmem_free(rmstack);
#endif
}

void vtmr_reset(void)
{
    int i;

    vtmr_irr = 0;
    vtmr_imr = 0;
    vtmr_pirr = 0;
    for (i = 0; i < VTMR_MAX; i++)
        pic_untrigger(vip[i].irq);
}

static int do_vtmr_raise(int timer)
{
    uint16_t pirr;
    uint16_t mask = 1 << timer;

    assert(timer < VTMR_MAX);
    h_printf("vtmr: raise timer %i\n", timer);
    pirr = __sync_fetch_and_or(&vtmr_pirr, mask);
    if (!(pirr & mask)) {
        h_printf("vtmr: posting timer event\n");
        sem_post(&vtmr_sem);
        return 1;
    }
    return 0;
}

void vtmr_raise(int timer)
{
    int rc;

    pthread_mutex_lock(&vth[timer].done_mtx);
    vth[timer].done_pred = 0;
    pthread_mutex_unlock(&vth[timer].done_mtx);

    rc = do_vtmr_raise(timer);
    if (!rc) {
        pthread_mutex_lock(&vth[timer].done_mtx);
        vth[timer].done_pred = 1;
        pthread_mutex_unlock(&vth[timer].done_mtx);
        pthread_cond_signal(&vth[timer].done_cnd);
    }
}

void vtmr_latch(int timer)
{
    if (in_dpmi_pm())
        fake_pm_int();
    coopth_start(latch_tid, (void *)(uintptr_t)timer);
}

void vtmr_sync(int timer)
{
    pthread_mutex_lock(&vth[timer].done_mtx);
    while (!vth[timer].done_pred)
        cond_wait(&vth[timer].done_cnd, &vth[timer].done_mtx);
    pthread_mutex_unlock(&vth[timer].done_mtx);
}

void vtmr_register(int timer, int (*handler)(int))
{
    struct vthandler *vt = &vth[timer];
    struct vint_presets *vp = &vip[timer];
    assert(timer < VTMR_MAX);
    vt->handler = handler;
    vt->vint = vint_register(ack_handler, mask_handler, vp->irq,
                             vp->orig_irq, vp->interrupt);
}

void vtmr_register_latch(int timer, int (*handler)(void))
{
    struct vthandler *vt = &vth[timer];
    assert(timer < VTMR_MAX);
    vt->latch = handler;
}

void vtmr_set_tweaked(int timer, int on, unsigned flags)
{
    vint_set_tweaked(vth[timer].vint, on, flags);
}

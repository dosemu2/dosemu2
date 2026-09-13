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
 * NDIS 2.0.1 MAC driver for dosemu2.
 *
 * NDIS protocol stacks (MS LAN Manager, MS Client, MS TCP/IP, WfW) talk
 * to a "MAC driver", which is a DOS character device that registers
 * itself with PROTMAN$ and then exchanges dispatch tables with the
 * protocols bound to it.  This implements such a MAC driver natively,
 * on top of dosemu2's networking backends, so that no DOS-side NIC
 * driver (and no emulated NIC) is needed.
 *
 * The DOS-resident part is the tiny commands/ndis.sys stub: it only
 * carries the device header, performs the PROTMAN$ IOCTLs that must be
 * issued from DOS, and reserves the DOS memory in which this code
 * builds the NDIS tables and the receive buffers.  Everything else -
 * the characteristics tables, the MAC upper dispatch entry points and
 * the indications to the protocol - lives here.
 *
 * The entry points of the MAC upper dispatch table point into dosemu's
 * HLT block, so that a far call from DOS lands in ndis_entry() below.
 * They use the MS C "far pascal" convention: arguments are pushed
 * left to right, the callee removes them, the result is returned in AX,
 * and DS, SI, DI, BP and the stack registers are preserved.
 *
 * The DOS driver of which this is a port was written by robert-j, see
 * https://github.com/robert-j/pktndis and dosemu2 issue #1479.
 *
 * Author: Stas Sergeev (dosemu2 integration)
 */

#include <string.h>
#include <strings.h>
#include <errno.h>
#include <stddef.h>
#include <assert.h>

#include "emu.h"
#include "memory.h"
#include "int.h"
#include "hlt.h"
#include "bios.h"
#include "cpu.h"
#include "coopth.h"
#include "virq.h"
#include "lowmem.h"
#include "ioselect.h"
#include "utilities.h"
#include "doshelpers.h"
#include "pktdrvr.h"
#include "libpacket.h"
#include "ndis.h"

/*
 * The tables are exchanged with 16-bit DOS code, so their layout has to
 * match what the DOS compilers produce, i.e. word alignment.
 */
static_assert(sizeof(CommonChar) == 0x40, "bad CommonChar");
static_assert(offsetof(CommonChar, CcSysReq) == 0x24, "bad CcSysReq");
static_assert(offsetof(CommonChar, CcLwrDisp) == 0x34, "bad CcLwrDisp");
static_assert(sizeof(MACSpecChar) == 96, "bad MACSpecChar");
static_assert(sizeof(MACSpecStat) == 100, "bad MACSpecStat");
static_assert(sizeof(MAC8023Stat) == 66, "bad MAC8023Stat");
static_assert(sizeof(MACUprDisp) == 28, "bad MACUprDisp");
static_assert(sizeof(ProtLwrDisp) == 32, "bad ProtLwrDisp");
static_assert(sizeof(TxDataBlock) == 8, "bad TxDataBlock");
static_assert(sizeof(TDDataBlock) == 8, "bad TDDataBlock");
static_assert(sizeof(RxDataBlock) == 6, "bad RxDataBlock");
static_assert(sizeof(ReqBlock) == 14, "bad ReqBlock");

#define NDIS_ETH_ALEN	6
#define NDIS_ETH_ZLEN	60
#define NDIS_FRAME_MAX	1514		/* incl. MAC header, excl. FCS */

/* number of receive buffers in DOS memory */
#define NDIS_RX_BUFS	6
/*
 * Below this many free buffers we stop giving them away with
 * ReceiveChain and switch to the synchronous ReceiveLookahead path,
 * which frees the buffer as soon as the indication returns.
 */
#define NDIS_RX_LOWATER	2
/* maximum number of frames indicated in one go */
#define NDIS_RX_BATCH	(NDIS_RX_BUFS * 2)

#define NDIS_MAJOR	0
#define NDIS_MINOR	1

/* the DOS-resident data of the driver, built by ndis_setup() */
struct ndis_res {
    CommonChar cc;
    MACSpecChar msc;
    MACSpecStat mss;
    MAC8023Stat m83;
    MCastBuf mcb;
    MACUprDisp mud;
    RxBufDesc rxdesc;			/* passed to ReceiveChain */
    uint8_t indicate;			/* the "Indicate" flag byte */
    uint8_t pad;
    char vendor_desc[32];
    uint8_t rxbuf[NDIS_RX_BUFS][NDIS_FRAME_MAX];
} __attribute__((packed));

/* entry points of the MAC upper dispatch table, in HLT block order */
enum {
    NDIS_E_SYSREQ,
    NDIS_E_REQUEST,
    NDIS_E_XMITCHAIN,
    NDIS_E_XFERDATA,
    NDIS_E_RCVRELEASE,
    NDIS_E_INDON,
    NDIS_E_INDOFF,
    NDIS_E_MAX
};

enum { RXB_FREE, RXB_QUEUED, RXB_INDICATED, RXB_HELD };

static int ndis_fd = -1;
static int ndis_tid;
static uint16_t ndis_hlt_off;
static uint16_t ndis_seg;		/* segment of the resident area */
static struct ndis_res *res;		/* host alias of the resident area */
static int ndis_inited;			/* ndis.sys loaded and registered */
static int ndis_bound;			/* a protocol is bound to us */
static int ind_level;			/* indications disabled if non-zero */
static int int_requested;		/* InterruptRequest pending */
static int io_pending;			/* we owe an ioselect_complete() */
static int io_registered;		/* we listen on the network link */
static int cur_ind_buf = -1;		/* buffer TransferData works on */
static uint8_t my_mac[NDIS_ETH_ALEN];
static uint8_t txbuf[NDIS_FRAME_MAX];

/* the bound protocol */
static struct {
    uint16_t ds;
    FARPTR req_confirm;
    FARPTR xmit_confirm;
    FARPTR rcv_lkahead;
    FARPTR ind_complete;
    FARPTR rcv_chain;
    FARPTR stat_ind;
} prot;

static struct {
    uint16_t len;
    uint8_t state;
} rxb[NDIS_RX_BUFS];
static uint8_t rxq[NDIS_RX_BUFS];
static int rxq_head, rxq_cnt;

static const uint8_t bcast_addr[NDIS_ETH_ALEN] =
	{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/************************************************************************/

static dosaddr_t far2addr(FARPTR p)
{
    return SEGOFF2LINEAR(FARPTR_SEG(p), FARPTR_OFF(p));
}

static FARPTR res_ptr(unsigned off)
{
    return MK_FARPTR(ndis_seg, off);
}

static dosaddr_t rxbuf_addr(int idx)
{
    return SEGOFF2LINEAR(ndis_seg, offsetof(struct ndis_res, rxbuf[idx]));
}

/*
 * Reads the idx-th word of the argument list of the far pascal call
 * we are currently servicing. Arguments are counted left to right,
 * as they appear in the prototype; a far pointer occupies two words,
 * the segment first.
 */
static uint16_t arg16(int nwords, int idx)
{
    uint16_t sp = LWORD(esp) + 4 + 2 * (nwords - 1 - idx);
    return READ_WORD(SEGOFF2LINEAR(SREG(ss), sp));
}

static FARPTR argfp(int nwords, int idx)
{
    return MK_FARPTR(arg16(nwords, idx), arg16(nwords, idx + 1));
}

/* returns from a far pascal call, removing its arguments */
static void ndis_ret(int nwords, uint16_t val)
{
    fake_retf();
    LWORD(esp) += nwords * 2;
    LWORD(eax) = val;
}

/************************************************************************/
/* receive buffer pool                                                  */

static int rxb_alloc(void)
{
    int i;

    for (i = 0; i < NDIS_RX_BUFS; i++) {
	if (rxb[i].state == RXB_FREE) {
	    rxb[i].state = RXB_QUEUED;
	    return i;
	}
    }
    return -1;
}

static int rxb_free_cnt(void)
{
    int i, cnt = 0;

    for (i = 0; i < NDIS_RX_BUFS; i++) {
	if (rxb[i].state == RXB_FREE)
	    cnt++;
    }
    return cnt;
}

static void rxb_free(int idx)
{
    rxb[idx].state = RXB_FREE;
    rxb[idx].len = 0;
}

static void rxq_push(int idx)
{
    assert(rxq_cnt < NDIS_RX_BUFS);
    rxq[(rxq_head + rxq_cnt) % NDIS_RX_BUFS] = idx;
    rxq_cnt++;
}

static int rxq_pop(void)
{
    int idx;

    assert(rxq_cnt > 0);
    idx = rxq[rxq_head];
    rxq_head = (rxq_head + 1) % NDIS_RX_BUFS;
    rxq_cnt--;
    return idx;
}

static void rxq_flush(void)
{
    int i;

    while (rxq_cnt)
	rxb_free(rxq_pop());
    for (i = 0; i < NDIS_RX_BUFS; i++)
	rxb_free(i);
    cur_ind_buf = -1;
}

/************************************************************************/
/* upcalls into the bound protocol                                      */

/*
 * Performs a far pascal call into the protocol. The arguments are given
 * in the order in which they are pushed, i.e. left to right, a far
 * pointer as segment followed by offset.
 */
static uint16_t prot_call(FARPTR entry, const uint16_t *args, int nwords)
{
    unsigned ssp;
    uint16_t sp;
    int i;

    if (!entry) {
	error("NDIS: call to a NULL protocol entry point\n");
	return NDIS_ERR_GENERAL_FAILURE;
    }
    ssp = SEGOFF2LINEAR(SREG(ss), 0);
    sp = LWORD(esp);
    for (i = 0; i < nwords; i++)
	pushw(ssp, sp, args[i]);
    LWORD(esp) -= nwords * 2;
    do_call_back(FARPTR_SEG(entry), FARPTR_OFF(entry));
    /* the callee removed the arguments, so SP needs no adjustment */
    return LWORD(eax);
}

/*
 * Prepares the "Indicate" flag byte that the protocol clears if it
 * wants the indications to stay off after the handler returns.
 */
static void indicate_init(void)
{
    res->indicate = INDICATION_INIT;
}

static int indicate_wants_off(void)
{
    return res->indicate == 0;
}

/* ReceiveLookahead: the frame stays ours, the protocol copies it out
 * with TransferData before it returns. */
static int prot_lookahead(int idx)
{
    unsigned ind_off = offsetof(struct ndis_res, indicate);
    uint16_t args[8];
    uint16_t len = rxb[idx].len;

    args[0] = res->cc.CcModId;		/* MACID */
    args[1] = len;			/* FrameSize */
    args[2] = len;			/* BytesAvail */
    args[3] = ndis_seg;			/* Buffer */
    args[4] = offsetof(struct ndis_res, rxbuf[idx]);
    args[5] = ndis_seg;			/* Indicate */
    args[6] = ind_off;
    args[7] = prot.ds;			/* ProtDS */

    indicate_init();
    cur_ind_buf = idx;
    rxb[idx].state = RXB_INDICATED;
    prot_call(prot.rcv_lkahead, args, 8);
    cur_ind_buf = -1;
    rxb_free(idx);
    return indicate_wants_off();
}

/* ReceiveChain: the protocol may keep the buffer until ReceiveRelease */
static int prot_rcv_chain(int idx)
{
    unsigned ind_off = offsetof(struct ndis_res, indicate);
    uint16_t args[8];
    uint16_t len = rxb[idx].len;
    uint16_t ret;

    res->rxdesc.RxDataCount = 1;
    res->rxdesc.RxData[0].RxDataLen = len;
    res->rxdesc.RxData[0].RxDataPtr =
	    res_ptr(offsetof(struct ndis_res, rxbuf[idx]));

    args[0] = res->cc.CcModId;		/* MACID */
    args[1] = len;			/* FrameSize */
    args[2] = idx;			/* ReqHandle */
    args[3] = ndis_seg;			/* BufDesc */
    args[4] = offsetof(struct ndis_res, rxdesc);
    args[5] = ndis_seg;			/* Indicate */
    args[6] = ind_off;
    args[7] = prot.ds;			/* ProtDS */

    indicate_init();
    rxb[idx].state = RXB_INDICATED;
    ret = prot_call(prot.rcv_chain, args, 8);
    if (ret == NDIS_ERR_WAIT_FOR_RELEASE)
	rxb[idx].state = RXB_HELD;
    else
	rxb_free(idx);
    return indicate_wants_off();
}

static int prot_status_ind(uint16_t opcode)
{
    uint16_t args[6];

    if (!prot.stat_ind)
	return 0;
    args[0] = res->cc.CcModId;		/* MACID */
    args[1] = 0;			/* Param */
    args[2] = ndis_seg;			/* Indicate */
    args[3] = offsetof(struct ndis_res, indicate);
    args[4] = opcode;
    args[5] = prot.ds;

    indicate_init();
    prot_call(prot.stat_ind, args, 6);
    return indicate_wants_off();
}

static void prot_ind_complete(void)
{
    uint16_t args[2];

    if (!prot.ind_complete)
	return;
    args[0] = res->cc.CcModId;
    args[1] = prot.ds;
    prot_call(prot.ind_complete, args, 2);
}

/************************************************************************/
/* receive path                                                         */

static int ndis_accept(const uint8_t *buf)
{
    uint16_t filter = res->mss.MssFilter;
    int i;

    if (!filter)			/* reception disabled */
	return 0;
    if (filter & FILTER_PROMISCUOUS)
	return 1;
    if (memcmp(buf, bcast_addr, NDIS_ETH_ALEN) == 0)
	return !!(filter & FILTER_BROADCAST);
    if (buf[0] & 1) {			/* multicast */
	if (!(filter & FILTER_DIRECTED))
	    return 0;
	for (i = 0; i < res->mcb.McbCnt; i++) {
	    if (memcmp(buf, res->mcb.McbAddrs[i].mAddr, NDIS_ETH_ALEN) == 0)
		return 1;
	}
	return 0;
    }
    if (memcmp(buf, my_mac, NDIS_ETH_ALEN) == 0)
	return !!(filter & FILTER_DIRECTED);
    return 0;
}

/* reads one frame from the network into a receive buffer.
 * Returns 1 if something was read, 0 if the socket ran dry. */
static int ndis_do_receive(void)
{
    uint8_t buf[NDIS_FRAME_MAX];
    int idx, size;

    size = pkt_read(ndis_fd, buf, sizeof(buf));
    if (size <= 0)
	return 0;
    /* while nobody is bound, the frames are read and discarded */
    if (!ndis_bound)
	return 1;
    if (size < NDIS_ETH_ALEN * 2) {
	res->mss.MssRFMin++;
	return 1;
    }
    if (!ndis_accept(buf))
	return 1;
    /*
     * The TAP device can hand us frames shorter than the 60 bytes that
     * came off the wire, as the padding is not transmitted. DOS stacks
     * tend to dislike that, so pad the frame back. Same as the built-in
     * packet driver does.
     */
    if (size < NDIS_ETH_ZLEN) {
	memset(buf + size, 0, NDIS_ETH_ZLEN - size);
	size = NDIS_ETH_ZLEN;
    }
    idx = rxb_alloc();
    if (idx == -1) {
	/* should not happen, the caller checks for a free buffer */
	res->mss.MssRFLack++;
	return 1;
    }
    MEMCPY_2DOS(rxbuf_addr(idx), buf, size);
    rxb[idx].len = size;
    rxq_push(idx);
    res->mss.MssFR++;
    res->mss.MssFRByt += size;
    pd_printf("NDIS: received frame of %i bytes into buffer %i\n", size, idx);
    return 1;
}

static enum VirqHwRet ndis_virq_receive(void *arg)
{
    if (ndis_bound && rxb_free_cnt() == 0) {
	/*
	 * Nothing we can do right now. The fd stays masked and we get
	 * re-raised when the buffers are given back to us.
	 */
	return VIRQ_HWRET_DONE;
    }
    if (ndis_do_receive())
	return VIRQ_HWRET_CONT;
    /* the socket ran dry, let it wake us up again */
    if (io_pending) {
	io_pending = 0;
	ioselect_complete(ndis_fd);
    }
    return VIRQ_HWRET_DONE;
}

static void ndis_indicate_thr(void *arg)
{
    int wants_off = 0;
    int indicated = 0;
    int cnt;

    /*
     * Indications are disabled while we are indicating, so that the
     * frames arriving in the meantime are queued rather than delivered
     * from within a protocol's handler.
     */
    ind_level++;
    rm_stack_enter();
    /*
     * Frames keep arriving while we are calling into DOS, so limit the
     * batch: what is left is picked up by the next round.
     */
    for (cnt = 0; rxq_cnt && cnt < NDIS_RX_BATCH; cnt++) {
	int idx = rxq_pop();

	if (prot.rcv_chain && rxb_free_cnt() >= NDIS_RX_LOWATER)
	    wants_off |= prot_rcv_chain(idx);
	else
	    wants_off |= prot_lookahead(idx);
	indicated = 1;
    }
    if (int_requested) {
	int_requested = 0;
	wants_off |= prot_status_ind(SI_INTERRUPT_STATUS);
	indicated = 1;
    }
    /* the spec requires IndicationComplete after a batch of indications */
    if (indicated)
	prot_ind_complete();
    rm_stack_leave();

    /*
     * If the protocol cleared the indicate byte, it wants the
     * indications to stay off and will call IndicationOn itself.
     */
    if (!wants_off && ind_level)
	ind_level--;
    if (!ind_level && (rxq_cnt || io_pending))
	virq_raise(VIRQ_NDIS);
}

static enum VirqSwRet ndis_indicate(void *arg)
{
    if (!ndis_bound || ind_level || (!rxq_cnt && !int_requested))
	return VIRQ_SWRET_DONE;
    coopth_start(ndis_tid, NULL);
    return VIRQ_SWRET_BH;
}

static void ndis_receive_req_async(int fd, void *arg)
{
    io_pending = 1;
    virq_raise(VIRQ_NDIS);
}

/************************************************************************/
/* MAC upper dispatch table                                             */

/*
 * SystemRequest, called by PROTMAN to bind a protocol to us.
 *
 * USHORT SystemRequest(CommonChar far *ProtCC, CommonChar far * far *MacCC,
 *                      USHORT Param3, USHORT Opcode, USHORT MACDS);
 */
static void ndis_sysreq(void)
{
    const int nw = 7;
    FARPTR protcc = argfp(nw, 0);
    FARPTR maccc_p = argfp(nw, 2);
    uint16_t opcode = arg16(nw, 5);
    dosaddr_t cc, ld;

    if (opcode != SR_BIND) {
	pd_printf("NDIS: unsupported system request %i\n", opcode);
	ndis_ret(nw, NDIS_ERR_INVALID_FUNCTION);
	return;
    }
    if (ndis_bound) {
	error("NDIS: second bind attempt\n");
	ndis_ret(nw, NDIS_ERR_ALREADY_STARTED);
	return;
    }
    if (!protcc) {
	ndis_ret(nw, NDIS_ERR_INVALID_PARAMETER);
	return;
    }
    cc = far2addr(protcc);
    ld = far2addr(READ_DWORD(cc + offsetof(CommonChar, CcLwrDisp)));
    if (!ld) {
	error("NDIS: protocol has no lower dispatch table\n");
	ndis_ret(nw, NDIS_ERR_INCOMPATIBLE_MAC);
	return;
    }
    prot.ds = READ_WORD(cc + offsetof(CommonChar, CcModDS));
    prot.req_confirm = READ_DWORD(ld + offsetof(ProtLwrDisp, PldRequestConfirm));
    prot.xmit_confirm = READ_DWORD(ld + offsetof(ProtLwrDisp, PldTransmitConfirm));
    prot.rcv_lkahead = READ_DWORD(ld + offsetof(ProtLwrDisp, PldRcvLkAhead));
    prot.ind_complete = READ_DWORD(ld + offsetof(ProtLwrDisp, PldIndComplete));
    prot.rcv_chain = READ_DWORD(ld + offsetof(ProtLwrDisp, PldRcvChain));
    prot.stat_ind = READ_DWORD(ld + offsetof(ProtLwrDisp, PldStatInd));
    if (!prot.rcv_lkahead) {
	error("NDIS: protocol has no ReceiveLookahead handler\n");
	ndis_ret(nw, NDIS_ERR_INCOMPATIBLE_MAC);
	return;
    }

    /* hand our characteristics table to the protocol */
    WRITE_DWORD(far2addr(maccc_p), res_ptr(offsetof(struct ndis_res, cc)));
    res->mss.MssStatus |= MAC_STATUS_BOUND;
    /* the spec wants the reception to start disabled */
    res->mss.MssFilter = 0;
    ind_level = 0;
    ndis_bound = 1;
    pd_printf("NDIS: bound to protocol '%.16s', DS=%#x\n",
	    (char *)LINEAR2UNIX(cc + offsetof(CommonChar, CcName)), prot.ds);
    if (io_pending)
	virq_raise(VIRQ_NDIS);
    ndis_ret(nw, NDIS_SUCCESS);
}

/*
 * USHORT Request(USHORT ProtID, USHORT ReqHandle, USHORT Param1,
 *                void far *Param2, USHORT Opcode, USHORT MACDS);
 */
static void ndis_request(void)
{
    const int nw = 7;
    uint16_t param1 = arg16(nw, 2);
    FARPTR param2 = argfp(nw, 3);
    uint16_t opcode = arg16(nw, 5);
    uint16_t ret = NDIS_SUCCESS;
    uint8_t addr[NDIS_ETH_ALEN];
    int i;

    switch (opcode) {
    case GR_SET_PACKET_FILTER:
	pd_printf("NDIS: set packet filter to %#x\n", param1);
	res->mss.MssFilter = param1;
	break;

    case GR_ADD_MULTICAST_ADDRESS:
	if (!param2) {
	    ret = NDIS_ERR_INVALID_PARAMETER;
	    break;
	}
	MEMCPY_2UNIX(addr, far2addr(param2), sizeof(addr));
	for (i = 0; i < res->mcb.McbCnt; i++) {
	    if (memcmp(res->mcb.McbAddrs[i].mAddr, addr, sizeof(addr)) == 0)
		break;
	}
	if (i == res->mcb.McbCnt) {
	    if (res->mcb.McbCnt >= res->mcb.McbMax) {
		ret = NDIS_ERR_OUT_OF_RESOURCE;
		break;
	    }
	    memcpy(res->mcb.McbAddrs[res->mcb.McbCnt].mAddr, addr,
		    sizeof(addr));
	    res->mcb.McbCnt++;
	}
	break;

    case GR_DELETE_MULTICAST_ADDRESS:
	if (!param2) {
	    ret = NDIS_ERR_INVALID_PARAMETER;
	    break;
	}
	MEMCPY_2UNIX(addr, far2addr(param2), sizeof(addr));
	for (i = 0; i < res->mcb.McbCnt; i++) {
	    if (memcmp(res->mcb.McbAddrs[i].mAddr, addr, sizeof(addr)) == 0) {
		memmove(&res->mcb.McbAddrs[i], &res->mcb.McbAddrs[i + 1],
			(res->mcb.McbCnt - i - 1) * sizeof(MCastAddr));
		res->mcb.McbCnt--;
		break;
	    }
	}
	break;

    case GR_INTERRUPT_REQUEST:
	/*
	 * Ask for a callback from "interrupt context". We have no
	 * interrupt of our own to trigger, so let the virtual IRQ
	 * deliver the status indication.
	 */
	int_requested = 1;
	virq_raise(VIRQ_NDIS);
	break;

    case GR_SET_LOOKAHEAD:
	/* we always indicate the entire frame */
	break;

    case GR_UPDATE_STATISTICS:
    case GR_CLEAR_STATISTICS:
	/* the statistics we maintain are always current */
	break;

    default:
	pd_printf("NDIS: unsupported general request %i\n", opcode);
	ret = NDIS_ERR_INVALID_FUNCTION;
	break;
    }
    ndis_ret(nw, ret);
}

/*
 * USHORT TransmitChain(USHORT ProtID, USHORT ReqHandle,
 *                      TxBufDesc far *BufDesc, USHORT MACDS);
 */
static void ndis_transmit_chain(void)
{
    const int nw = 5;
    FARPTR bdp = argfp(nw, 2);
    dosaddr_t bd;
    uint16_t immed_len, cnt, total = 0;
    int i;

    if (!bdp) {
	ndis_ret(nw, NDIS_ERR_INVALID_PARAMETER);
	return;
    }
    bd = far2addr(bdp);
    immed_len = READ_WORD(bd + offsetof(TxBufDesc, TxImmedLen));
    cnt = READ_WORD(bd + offsetof(TxBufDesc, TxDataCount));
    if (immed_len > MAX_IMMED_LEN || cnt > MAX_TX_DATABLK) {
	ndis_ret(nw, NDIS_ERR_INVALID_PARAMETER);
	return;
    }
    if (immed_len) {
	FARPTR p = READ_DWORD(bd + offsetof(TxBufDesc, TxImmedPtr));

	MEMCPY_2UNIX(txbuf, far2addr(p), immed_len);
	total = immed_len;
    }
    for (i = 0; i < cnt; i++) {
	dosaddr_t blk = bd + offsetof(TxBufDesc, TxData) +
		i * sizeof(TxDataBlock);
	uint16_t len = READ_WORD(blk + offsetof(TxDataBlock, TxDataLen));
	FARPTR p = READ_DWORD(blk + offsetof(TxDataBlock, TxDataPtr));

	if (!len)			/* allowed by the spec */
	    continue;
	if (total + len > sizeof(txbuf)) {
	    error("NDIS: frame of %i bytes is too large\n", total + len);
	    ndis_ret(nw, NDIS_ERR_OUT_OF_RESOURCE);
	    return;
	}
	MEMCPY_2UNIX(txbuf + total, far2addr(p), len);
	total += len;
    }
    if (total < NDIS_ETH_ALEN * 2) {
	ndis_ret(nw, NDIS_ERR_INVALID_PARAMETER);
	return;
    }
    /* the hardware would pad a runt frame, so do it here */
    if (total < NDIS_ETH_ZLEN) {
	memset(txbuf + total, 0, NDIS_ETH_ZLEN - total);
	total = NDIS_ETH_ZLEN;
    }
    pd_printf("NDIS: transmitting frame of %i bytes\n", total);
    if (pkt_write(ndis_fd, txbuf, total) < 0) {
	error("NDIS: write to network failed: %s\n", strerror(errno));
	res->mss.MssSFHW++;
	ndis_ret(nw, NDIS_ERR_TRANSMIT_ERROR);
	return;
    }
    res->mss.MssFS++;
    res->mss.MssFSByt += total;
    /* we complete synchronously, so no TransmitConfirm is needed */
    ndis_ret(nw, NDIS_SUCCESS);
}

/*
 * USHORT TransferData(USHORT far *BytesCopied, USHORT FrameOfs,
 *                     TDBufDesc far *BufDesc, USHORT MACDS);
 */
static void ndis_transfer_data(void)
{
    const int nw = 6;
    FARPTR bcp = argfp(nw, 0);
    uint16_t frame_ofs = arg16(nw, 2);
    FARPTR bdp = argfp(nw, 3);
    dosaddr_t bd, src;
    uint16_t cnt, total = 0, len;
    int i;

    if (cur_ind_buf == -1 || !bdp) {
	if (bcp)
	    WRITE_WORD(far2addr(bcp), 0);
	ndis_ret(nw, NDIS_ERR_INVALID_PARAMETER);
	return;
    }
    len = rxb[cur_ind_buf].len;
    src = rxbuf_addr(cur_ind_buf);
    bd = far2addr(bdp);
    cnt = READ_WORD(bd + offsetof(TDBufDesc, TDDataCount));
    if (cnt > MAX_TD_DATABLK)
	cnt = MAX_TD_DATABLK;
    for (i = 0; i < cnt && frame_ofs < len; i++) {
	dosaddr_t blk = bd + offsetof(TDBufDesc, TDData) +
		i * sizeof(TDDataBlock);
	uint16_t blen = READ_WORD(blk + offsetof(TDDataBlock, TDDataLen));
	FARPTR p = READ_DWORD(blk + offsetof(TDDataBlock, TDDataPtr));

	if (!blen)			/* allowed by the spec */
	    continue;
	if (frame_ofs + blen > len)
	    blen = len - frame_ofs;
	MEMCPY_DOS2DOS(far2addr(p), src + frame_ofs, blen);
	frame_ofs += blen;
	total += blen;
    }
    if (bcp)
	WRITE_WORD(far2addr(bcp), total);
    ndis_ret(nw, NDIS_SUCCESS);
}

/*
 * USHORT ReceiveRelease(USHORT ReqHandle, USHORT MACDS);
 */
static void ndis_receive_release(void)
{
    const int nw = 2;
    uint16_t handle = arg16(nw, 0);

    if (handle >= NDIS_RX_BUFS || rxb[handle].state != RXB_HELD) {
	error("NDIS: bad ReceiveRelease handle %i\n", handle);
	ndis_ret(nw, NDIS_ERR_INVALID_PARAMETER);
	return;
    }
    rxb_free(handle);
    if (!ind_level && io_pending)
	virq_raise(VIRQ_NDIS);
    ndis_ret(nw, NDIS_SUCCESS);
}

/*
 * USHORT IndicationOn(USHORT MACDS);
 *
 * The spec asks us to return with the interrupts disabled. As our
 * receive path is driven by a virtual IRQ that only re-enters when
 * the indications are enabled, there is nothing to protect against
 * here and the callers interrupt flag is left alone.
 */
static void ndis_indication_on(void)
{
    if (ind_level)
	ind_level--;
    if (!ind_level && (rxq_cnt || io_pending || int_requested))
	virq_raise(VIRQ_NDIS);
    ndis_ret(1, NDIS_SUCCESS);
}

/*
 * USHORT IndicationOff(USHORT MACDS);
 */
static void ndis_indication_off(void)
{
    ind_level++;
    ndis_ret(1, NDIS_SUCCESS);
}

static void ndis_entry(Bit16u idx, HLT_ARG(arg))
{
    if (!ndis_inited) {
	error("NDIS: entry %i called before initialization\n", idx);
	fake_retf();
	LWORD(eax) = NDIS_ERR_DRIVER_NOT_INITIALIZED;
	return;
    }
    switch (idx) {
    case NDIS_E_SYSREQ:
	ndis_sysreq();
	break;
    case NDIS_E_REQUEST:
	ndis_request();
	break;
    case NDIS_E_XMITCHAIN:
	ndis_transmit_chain();
	break;
    case NDIS_E_XFERDATA:
	ndis_transfer_data();
	break;
    case NDIS_E_RCVRELEASE:
	ndis_receive_release();
	break;
    case NDIS_E_INDON:
	ndis_indication_on();
	break;
    case NDIS_E_INDOFF:
	ndis_indication_off();
	break;
    default:
	error("NDIS: unknown entry point %i\n", idx);
	fake_retf();
	LWORD(eax) = NDIS_ERR_GENERAL_FAILURE;
	break;
    }
}

/************************************************************************/
/* PROTOCOL.INI parsing                                                 */

/*
 * The configuration memory image handed out by PROTMAN$ is a linked
 * list of module (section) configurations, each holding a linked list
 * of keyword entries with typed parameters.
 */
static int cfg_get_str(dosaddr_t kw, char *buf, int size)
{
    dosaddr_t par = kw + offsetof(KeywordEntry, Params);

    if (!READ_WORD(kw + offsetof(KeywordEntry, NumParams)))
	return -1;
    if (READ_WORD(par + offsetof(Param, ParamType)) != PARAM_TYPE_STRING)
	return -1;
    snprintf(buf, size, "%s",
	    (char *)LINEAR2UNIX(par + offsetof(Param, ParamVal)));
    return 0;
}

static dosaddr_t cfg_find_module(dosaddr_t cfg, const char *drv_name)
{
    while (cfg) {
	dosaddr_t kw = cfg + offsetof(ModuleConfig, KE);

	while (kw) {
	    char keyword[NAME_LEN + 1];
	    char value[64];

	    memcpy(keyword, LINEAR2UNIX(kw + offsetof(KeywordEntry, KeyWord)),
		    NAME_LEN);
	    keyword[NAME_LEN] = '\0';
	    if (strcasecmp(keyword, "DRIVERNAME") == 0 &&
		    cfg_get_str(kw, value, sizeof(value)) == 0) {
		pd_printf("NDIS: PROTOCOL.INI: DriverName=%s\n", value);
		if (strcasecmp(value, drv_name) == 0)
		    return cfg;
	    }
	    kw = far2addr(READ_DWORD(kw +
		    offsetof(KeywordEntry, NextKeywordEntry)));
	}
	cfg = far2addr(READ_DWORD(cfg + offsetof(ModuleConfig, NextModule)));
    }
    return 0;
}

/************************************************************************/
/* initialization                                                       */

static void ndis_build_tables(const char *name)
{
    unsigned mcb_off = offsetof(struct ndis_res, mcb);

    memset(res, 0, sizeof(*res));

    res->cc.CcSize = sizeof(CommonChar);
    res->cc.CcNdisMajor = NDIS_VERSION_MAJOR;
    res->cc.CcNdisMinor = NDIS_VERSION_MINOR;
    res->cc.CcModMajor = NDIS_MAJOR;
    res->cc.CcModMinor = NDIS_MINOR;
    res->cc.CcModFunc = MFF_BIND_UPPER;
    snprintf((char *)res->cc.CcName, NAME_LEN, "%s", name);
    res->cc.CcUprLevel = UL_MAC;
    res->cc.CcUprType = UI_MAC;
    res->cc.CcModDS = ndis_seg;
    res->cc.CcSysReq = MK_FARPTR(BIOS_HLT_BLK_SEG,
	    ndis_hlt_off + NDIS_E_SYSREQ);
    res->cc.CcSChar = res_ptr(offsetof(struct ndis_res, msc));
    res->cc.CcSStat = res_ptr(offsetof(struct ndis_res, mss));
    res->cc.CcUprDisp = res_ptr(offsetof(struct ndis_res, mud));

    snprintf((char *)res->msc.MscType, NAME_LEN, "DIX+802.3");
    res->msc.MscSize = sizeof(MACSpecChar);
    res->msc.MscStnAdrSz = NDIS_ETH_ALEN;
    memcpy(res->msc.MscPermStnAdr, my_mac, NDIS_ETH_ALEN);
    memcpy(res->msc.MscCurrStnAdr, my_mac, NDIS_ETH_ALEN);
    res->msc.MscMCp = res_ptr(mcb_off);
    res->msc.MscLinkSpd = 100000000;	/* 100 Mbit/s */
    res->msc.MscService = SF_BROADCAST | SF_MULTICAST | SF_PROMISCUOUS |
	    SF_CURRENT_STATS | SF_RECEIVE_CHAIN | SF_INTERRUPT_REQUEST |
	    SF_MULTIPLE_XFER;
    res->msc.MscMaxFrame = NDIS_FRAME_MAX;
    res->msc.MscTBufCap = NDIS_FRAME_MAX;
    res->msc.MscTBlkSz = NDIS_FRAME_MAX;
    res->msc.MscRBufCap = NDIS_RX_BUFS * NDIS_FRAME_MAX;
    res->msc.MscRBlkSz = NDIS_FRAME_MAX;
    memset(res->msc.MscVenCode, 0xff, sizeof(res->msc.MscVenCode));
    snprintf(res->vendor_desc, sizeof(res->vendor_desc), "dosemu2 NDIS adapter");
    res->msc.MscVenAdaptDesc = res_ptr(offsetof(struct ndis_res, vendor_desc));
    res->msc.MscTxQDepth = 1;
    res->msc.MscMaxDataBlks = MAX_TX_DATABLK;

    res->mss.MssSize = sizeof(MACSpecStat);
    res->mss.MssDiagDT = NDIS_NOT_MAINTAINED;
    res->mss.MssClearDT = NDIS_NOT_MAINTAINED;
    res->mss.MssStatus = MAC_STATUS_OK | MAC_STATUS_OPENED;
    res->mss.MssFilter = 0;
    res->mss.MssMediaStat = res_ptr(offsetof(struct ndis_res, m83));
    res->mss.MssFRMC = NDIS_NOT_MAINTAINED;
    res->mss.MssFRBC = NDIS_NOT_MAINTAINED;
    res->mss.MssFRMCByt = NDIS_NOT_MAINTAINED;
    res->mss.MssFRBCByt = NDIS_NOT_MAINTAINED;
    res->mss.MssRFCRC = NDIS_NOT_MAINTAINED;
    res->mss.MssRFMax = NDIS_NOT_MAINTAINED;
    res->mss.MssRFHW = NDIS_NOT_MAINTAINED;
    res->mss.MssFSMC = NDIS_NOT_MAINTAINED;
    res->mss.MssFSBC = NDIS_NOT_MAINTAINED;
    res->mss.MssFSBCByt = NDIS_NOT_MAINTAINED;
    res->mss.MssFSMCByt = NDIS_NOT_MAINTAINED;
    res->mss.MssSFTime = NDIS_NOT_MAINTAINED;

    res->m83.M83sSize = sizeof(MAC8023Stat);
    res->m83.M83sVer = 1;

    res->mcb.McbMax = NUM_MCADDRS;

    res->mud.MudCCp = res_ptr(offsetof(struct ndis_res, cc));
    res->mud.MudRequest = MK_FARPTR(BIOS_HLT_BLK_SEG,
	    ndis_hlt_off + NDIS_E_REQUEST);
    res->mud.MudTransmitChain = MK_FARPTR(BIOS_HLT_BLK_SEG,
	    ndis_hlt_off + NDIS_E_XMITCHAIN);
    res->mud.MudTransferData = MK_FARPTR(BIOS_HLT_BLK_SEG,
	    ndis_hlt_off + NDIS_E_XFERDATA);
    res->mud.MudReceiveRelease = MK_FARPTR(BIOS_HLT_BLK_SEG,
	    ndis_hlt_off + NDIS_E_RCVRELEASE);
    res->mud.MudIndicationOn = MK_FARPTR(BIOS_HLT_BLK_SEG,
	    ndis_hlt_off + NDIS_E_INDON);
    res->mud.MudIndicationOff = MK_FARPTR(BIOS_HLT_BLK_SEG,
	    ndis_hlt_off + NDIS_E_INDOFF);
}

/*
 * Called by ndis.sys after it got the configuration memory image from
 * PROTMAN$. Builds all the tables and prepares the request block for
 * the RegisterModule call that ndis.sys issues next.
 */
static int ndis_setup(dosaddr_t rb, uint16_t seg)
{
    dosaddr_t cfg, mod;
    char name[NAME_LEN + 1];

    if (READ_WORD(rb + offsetof(ReqBlock, Status)) != NDIS_SUCCESS) {
	error("NDIS: PROTMAN$ GetProtocolManagerInfo failed with %#x\n",
		READ_WORD(rb + offsetof(ReqBlock, Status)));
	return NDIS_ERROR_NOCONFIG;
    }
    cfg = far2addr(READ_DWORD(rb + offsetof(ReqBlock, Pointer1)));
    if (!cfg) {
	error("NDIS: PROTMAN$ returned no configuration\n");
	return NDIS_ERROR_NOCONFIG;
    }
    mod = cfg_find_module(cfg, NDIS_DEVICE_NAME);
    if (!mod) {
	error("NDIS: no PROTOCOL.INI section with DriverName=%s\n",
		NDIS_DEVICE_NAME);
	return NDIS_ERROR_NOCONFIG;
    }
    memcpy(name, LINEAR2UNIX(mod + offsetof(ModuleConfig, ModName)), NAME_LEN);
    name[NAME_LEN] = '\0';

    ndis_seg = seg;
    res = LINEAR2UNIX(SEGOFF2LINEAR(seg, 0));
    if (GetDeviceHardwareAddress(my_mac) < 0)
	pkt_get_fake_mac(my_mac);
    ndis_build_tables(name);
    rxq_flush();

    /* prepare the RegisterModule request for ndis.sys */
    WRITE_WORD(rb + offsetof(ReqBlock, Opcode), PM_REGISTER_MODULE);
    WRITE_WORD(rb + offsetof(ReqBlock, Status), 0);
    WRITE_DWORD(rb + offsetof(ReqBlock, Pointer1),
	    res_ptr(offsetof(struct ndis_res, cc)));
    WRITE_DWORD(rb + offsetof(ReqBlock, Pointer2), 0);
    WRITE_WORD(rb + offsetof(ReqBlock, Word1), 0);

    pd_printf("NDIS: module '%s' at %#x:%#x, MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
	    name, ndis_seg, (unsigned)offsetof(struct ndis_res, cc),
	    my_mac[0], my_mac[1], my_mac[2], my_mac[3], my_mac[4], my_mac[5]);
    return 0;
}

void ndis_helper(struct vm86_regs *regs)
{
    int err;

    NOCARRY;
    switch (HI_BYTE_d(regs->eax)) {
    case NDIS_SUBHELPER_INIT:
	if (!config.ndis || ndis_fd == -1) {
	    LO_BYTE_d(regs->ebx) = NDIS_ERROR_DISABLED;
	    CARRY;
	    break;
	}
	if (ndis_inited) {
	    LO_BYTE_d(regs->ebx) = NDIS_ERROR_ALREADY;
	    CARRY;
	    break;
	}
	/* paragraphs of DOS memory we need to be reserved for us */
	regs->ecx = (sizeof(struct ndis_res) + 15) >> 4;
	break;

    case NDIS_SUBHELPER_CONFIG:
	err = ndis_setup(SEGOFF2LINEAR(regs->es, LO_WORD(regs->ebx)),
		LO_WORD(regs->edx));
	if (err) {
	    LO_BYTE_d(regs->ebx) = err;
	    CARRY;
	}
	break;

    case NDIS_SUBHELPER_DONE:
	ndis_inited = 1;
	if (!io_registered) {
	    /*
	     * Start listening only now: an enabled but unused NDIS
	     * driver should not take the frames away from the other
	     * users of the same network link.
	     */
	    add_to_io_select_masked(ndis_fd, ndis_receive_req_async, NULL);
	    io_registered = 1;
	}
	pd_printf("NDIS: driver registered with PROTMAN$\n");
	break;

    default:
	error("NDIS: unknown helper %#x\n", HI_BYTE_d(regs->eax));
	CARRY;
	break;
    }
}

static void ndis_register_net_fd(int fd, int mode)
{
    ndis_fd = fd;
}

void ndis_init(void)
{
    emu_hlt_t hlt_hdlr = HLT_INITIALIZER;

    if (!config.ndis)
	return;
    if (OpenNetworkLink(ndis_register_net_fd) < 0) {
	error("NDIS: unable to open the network link\n");
	if (config.pktdrv)
	    error("NDIS: most networking back-ends provide a single link only, "
		    "so $_pktdriver = (off) may be needed to use NDIS\n");
	config.ndis = 0;
	return;
    }

    hlt_hdlr.name = "ndis";
    hlt_hdlr.func = ndis_entry;
    hlt_hdlr.len = NDIS_E_MAX;
    ndis_hlt_off = hlt_register_handler_vm86(hlt_hdlr);

    ndis_tid = coopth_create("NDIS indications", ndis_indicate_thr);
    virq_register(VIRQ_NDIS, ndis_virq_receive, ndis_indicate, NULL);
    pd_printf("NDIS: driver initialized, device %s\n", NDIS_DEVICE_NAME);
}

/* stops listening on the network link, giving it back to its other users */
static void ndis_unlisten(void)
{
    if (!io_registered)
	return;
    if (io_pending) {
	io_pending = 0;
	ioselect_complete(ndis_fd);
    }
    remove_from_io_select(ndis_fd);
    io_registered = 0;
}

void ndis_reset(void)
{
    if (!config.ndis)
	return;
    /* the DOS-resident part is gone after a reboot */
    ndis_unlisten();
    ndis_inited = 0;
    ndis_bound = 0;
    ind_level = 0;
    int_requested = 0;
    rxq_head = rxq_cnt = 0;
    cur_ind_buf = -1;
    memset(rxb, 0, sizeof(rxb));
    memset(&prot, 0, sizeof(prot));
    res = NULL;
    ndis_seg = 0;
}

void ndis_term(void)
{
    if (!config.ndis)
	return;
    if (ndis_fd != -1) {
	ndis_unlisten();
	CloseNetworkLink(ndis_fd);
	ndis_fd = -1;
    }
}

/*
 *
 *     Packet driver emulation for the Linux DOS emulator.
 *
 *     Written almost from scratch by Rob Janssen, PE1CHL
 *     (pe1chl@rabo.nl, rob@cmgit.uucp, PE1CHL@PI8UTR.#UTR.NLD.EU, pe1chl@ko23)
 *
 *     Now it is entirely contained in dosemu, no driver needs to
 *     be loaded in DOS anymore.
 *     (it looks like the PC's BIOS supports the packet driver interface)
 *
 *     Also added support for multiple handles, and checking the
 *     actual type passed to access_type.  This means other protocols
 *     than Novell 802.3 are now supported.  Don't try to run TCP/IP
 *     over this packet driver, unless you understand what you are
 *     doing, though...
 */

#include "pktdrvr.h"
#include "cpu.h"
#include "emu.h"
#include "dosio.h"
#include "memory.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <linux/if_ether.h>

#define min(a,b)	((a) < (b)? (a) : (b))

extern int OpenNetworkType(u_short);
extern int GetDeviceHardwareAddress(char *, char *);
extern int WriteToNetwork(int, const char *, const char *, int);
extern int ReadFromNetwork(int, char *, char *, int);

/* global data which is put at PKTDRV_SEG:PKTDRV_OFF */
/* (not everything needs to be accessible from DOS space, but it is */
/* often convenient to be able to look at it with DEBUG...) */

struct pkt_globs
{
    unsigned char signature[20];	/* entry point + signature */
    unsigned char classes[4];		/* supported classes */
    unsigned char hw_address[8];	/* our hardware address */
    struct pkt_param param;		/* parameters */
    struct pkt_statistics stats;	/* counters */
    char driver_name[8];		/* fixed string: driver name */
    short type;				/* our type */
    short size;				/* current packet's size */
    long receiver;			/* current receive handler */
    int helpvec;			/* vector number for helper */
    unsigned char helper[56];		/* upcall helper */

    struct per_handle
    {
	char in_use;			/* this handle in use? */
	char class;			/* class it was access_type'd with */
	short packet_type_len;		/* length of packet type */
	int sock;			/* fd for the socket */
	long receiver;			/* receive handler */
	char packet_type[20];		/* packet type for this handle */
    } handle[MAX_HANDLE];

    char buf[ETH_FRAME_LEN + 32];	/* packet buffer */
};

static char devname[] = "eth0";		/* linux device name */

static struct pkt_globs *pg = NULL;

/* initialize the packet driver interface (called at startup) */
void
pkt_init (int vec)

{
    register unsigned char *ptr;

    /* initialize the globals */

    pg = (struct pkt_globs *) PKTDRV_ADD;

    memset(pg,0,sizeof(struct pkt_globs));

    /* init the interrupt vector and signature */

    SETIVEC(vec,PKTDRV_SEG,PKTDRV_OFF + offsetof(struct pkt_globs,signature));
    ptr = pg->signature;

    *ptr++ = 0xeb;			/* JMP $+10, NOP, PKT DRVR */
    *ptr++ = 0x0a;
    *ptr++ = 0x90;
    strcpy(ptr,"PKT DRVR");		/* signature as defined by driver spec */
    ptr += 9;
    *ptr++ = 0xcd;			/* 0xcd = INT */
    *ptr++ = vec;	
    *ptr++ = 0xca;			/* 0xca = RETF xx */
    *ptr++ = 2;

    /* fill other global data */

    strcpy(pg->driver_name,"Linux$");
    GetDeviceHardwareAddress(devname,pg->hw_address);
    pg->classes[0] = ETHER_CLASS;
    pg->classes[1] = IEEE_CLASS;
    pg->type = 12;			/* dummy type (3c503) */

    pg->param.major_rev = 9;		/* pkt driver spec */
    pg->param.minor_rev = 1;
    pg->param.length = sizeof(struct pkt_param);
    pg->param.addr_len = ETH_ALEN;
    pg->param.mtu = GetDeviceMTU(devname);
    pg->param.rcv_bufs = 8 - 1;		/* a guess */
    pg->param.xmt_bufs = 2 - 1;

    /* helper routine to transfer received packets and call receiver */

    pg->helpvec = vec + 1;		/* use this for helper vec */
    SETIVEC(pg->helpvec,PKTDRV_SEG,PKTDRV_OFF + offsetof(struct pkt_globs,helper));
    ptr = pg->helper;

    *ptr++ = 0x60;			/* pusha */
    *ptr++ = 0x0e;			/* push cs */
    *ptr++ = 0x1f;			/* pop ds */
    *ptr++ = 0x8b; *ptr++ = 0x0e;	/* mov cx,[pg->size] */
    *ptr++ = (PKTDRV_OFF + offsetof(struct pkt_globs,size)) & 0xff;
    *ptr++ = (PKTDRV_OFF + offsetof(struct pkt_globs,size)) >> 8;
    *ptr++ = 0xe3; *ptr++ = 0x2a;	/* jcxz nothing */
    *ptr++ = 0x31; *ptr++ = 0xc0;	/* xor ax,ax */
    *ptr++ = 0x51;			/* push cx */
    *ptr++ = 0xff; *ptr++ = 0x1e;	/* call [pg->receiver] */
    *ptr++ = (PKTDRV_OFF + offsetof(struct pkt_globs,receiver)) & 0xff;
    *ptr++ = (PKTDRV_OFF + offsetof(struct pkt_globs,receiver)) >> 8;
    *ptr++ = 0x59;			/* pop cx */
    *ptr++ = 0x8c; *ptr++ = 0xc0;	/* mov ax,es */
    *ptr++ = 0x09; *ptr++ = 0xf8;	/* or ax,di */
    *ptr++ = 0x74; *ptr++ = 0x16;	/* jz norecv */
    *ptr++ = 0xbe;			/* mov si,pg->buf */
    *ptr++ = (PKTDRV_OFF + offsetof(struct pkt_globs,buf)) & 0xff;
    *ptr++ = (PKTDRV_OFF + offsetof(struct pkt_globs,buf)) >> 8;
    *ptr++ = 0x57;			/* push di */
    *ptr++ = 0x51;			/* push cx */
    *ptr++ = 0xfc;			/* cld */
    *ptr++ = 0xf3; *ptr++ = 0xa4;	/* rep movsb */
    *ptr++ = 0x59;			/* pop cx */
    *ptr++ = 0x5e;			/* pop si */
    *ptr++ = 0x1e;			/* push ds */
    *ptr++ = 0x06;			/* push es */
    *ptr++ = 0x1f;			/* pop ds */
    *ptr++ = 0xb8;			/* mov ax,1 */
    *ptr++ = 0x01; *ptr++ = 0x00;
    *ptr++ = 0x2e; *ptr++ = 0xff; *ptr++ = 0x1e; /* call cs:[pg->receiver] */
    *ptr++ = (PKTDRV_OFF + offsetof(struct pkt_globs,receiver)) & 0xff;
    *ptr++ = (PKTDRV_OFF + offsetof(struct pkt_globs,receiver)) >> 8;
    *ptr++ = 0x1f;			/* pop ds */
    *ptr++ = 0xc7; *ptr++ = 0x06;	/* mov [pg->size],0 */
    *ptr++ = (PKTDRV_OFF + offsetof(struct pkt_globs,size)) & 0xff;
    *ptr++ = (PKTDRV_OFF + offsetof(struct pkt_globs,size)) >> 8;
    *ptr++ = 0x00; *ptr++ = 0x00;
    *ptr++ = 0x61;			/* popa */
    *ptr   = 0xcf;			/* iret */
}

/* this is the handler for INT calls from DOS to the packet driver */
int
pkt_int ()

{
    struct per_handle *hdlp;

#if 0
    printf("NPKT: AX=%04x BX=%04x CX=%04x DX=%04x FLAGS=%08x\n",
		LWORD(eax),LWORD(ebx),LWORD(ecx),LWORD(edx),REG(eflags));
    printf("      SI=%04x DI=%04x BP=%04x SP=%04x CS=%04x DS=%04x ES=%04x SS=%04x\n",
		LWORD(esi),LWORD(edi),LWORD(ebp),LWORD(esp),
		LWORD(cs),LWORD(ds),LWORD(es),LWORD(ss));
#endif

    /* first clear some flags that we may later set (e.g. on error) */

    REG(eflags) &= ~(CF|PF|AF|ZF|SF);

    /* when BX seems like a valid handle, set the handle info pointer */

    if (LWORD(ebx) < MAX_HANDLE)
	hdlp = &pg->handle[LWORD(ebx)];
    else
	hdlp = NULL;

    /* switch on the packet driver function code */

    switch (HI(ax))
    {
    case F_DRIVER_INFO:
	REG(eax) = 2;				/* basic+extended functions */
	REG(ebx) = 1;				/* version */
	if (hdlp != NULL && hdlp->in_use)
	    REG(ecx) = (hdlp->class << 8) + 1;	/* class, number */
	else
	    REG(ecx) = (pg->classes[0] << 8) + 1;
	REG(edx) = pg->type;			/* type (dummy) */
	REG(ds) = PKTDRV_SEG;			/* driver name */
	REG(esi) = PKTDRV_OFF + offsetof(struct pkt_globs,driver_name);
	return 1;

    case F_ACCESS_TYPE:
	if (strchr(pg->classes,LO(ax)) == NULL) { /* check if_class */
	    REG(edx) = E_NO_CLASS << 8;
	    break;
	}
	if (LWORD(ebx) != 0xffff && LWORD(ebx) != pg->type) { /* check if_type */
	    REG(edx) = E_NO_TYPE << 8;
	    break;
	}
	if (LO(dx) != 0 && LO(dx) != 1) {	/* check if_number */
	    REG(edx) = E_NO_NUMBER << 8;
	    break;
	}
	if (LWORD(ecx) > sizeof(pg->handle[0].packet_type)) { /* check len */
	    REG(edx) = E_BAD_TYPE << 8;
	    break;
	}
	/* now check if the type is not already in use, and find a free */
	/* handle to use for this request */
	{
	    int free_handle = -1;
	    int handle;
	    unsigned short type;

	    if (LWORD(ecx) == 0)		/* pass-all type? */
		handle = MAX_HANDLE - 1;	/* always put it last */
	    else
		handle = 0;			/* else search from 1st */

	    for (; handle < MAX_HANDLE; handle++) {
		hdlp = &pg->handle[handle];

		if (hdlp->in_use) {
		    if (hdlp->class == LO(ax) && /* same class? */
			!memcmp(hdlp->packet_type, /* same type? (prefix) */
				SEG_ADR((char *),ds,si),
				min(LWORD(ecx),hdlp->packet_type_len)))
		    {
			REG(edx) = E_TYPE_INUSE << 8;
			REG(eflags) |= CF;
			return 1;
		    }
		} else {
		    if (free_handle == -1)
			free_handle = handle;	/* remember free handle */
		}
	    }

	    /* now see if we found a free handle, and allocate it */
	    if (free_handle == -1) {
		REG(edx) = E_NO_SPACE << 8;
		break;
	    }

	    hdlp = &pg->handle[free_handle];
	    hdlp->in_use = 1;
	    hdlp->receiver = (LWORD(es) << 16) | LWORD(edi);
	    hdlp->packet_type_len = LWORD(ecx);
	    memcpy(hdlp->packet_type,SEG_ADR((char *),ds,si),LWORD(ecx));
	    hdlp->class = LO(ax);

	    if (hdlp->class == IEEE_CLASS)
		type = ETH_P_802_3;
	    else {
                if (hdlp->packet_type_len < 2)
                   type = ETH_P_ALL;           /* get all packet types */
                else {
	  	    type = (hdlp->packet_type[0] << 8) | hdlp->packet_type[1];

		    if (type == 0)			/* sometimes this bogus type */
		        type = ETH_P_802_3;		/* is used for status calls.. */
	        }
	    }

	    hdlp->sock = OpenNetworkType(type);	/* open the socket */

	    if (hdlp->sock < 0) {
		hdlp->in_use = 0;		/* forget about this handle */
		REG(edx) = E_BAD_TYPE << 8;
		break;
	    }

	    REG(eax) = free_handle;		/* return the handle */
	}
	return 1;

    case F_RELEASE_TYPE:
	if (hdlp == NULL) {
	    REG(edx) = E_BAD_HANDLE << 8;
	    break;
	}
	if (hdlp->in_use)
	    CloseNetworkLink(hdlp->sock);	/* close the socket */
	hdlp->in_use = 0;			/* no longer in use */
	return 1;

    case F_SEND_PKT:
    	pg->stats.packets_out++;
    	pg->stats.bytes_out += LWORD(ecx);

    	/* unfortunately, a handle is not passed as a parameter for the */
	/* SEND_PKT call.  so, we will have to scan the handle table to */
	/* find a handle which is in use, and use its socket for the send */
	{
	    int handle;

	    for (handle = 0; handle < MAX_HANDLE; handle++) {
		hdlp = &pg->handle[handle];

		if (hdlp->in_use) {
		    if (WriteToNetwork(hdlp->sock,devname,
				SEG_ADR((char *),ds,si),LWORD(ecx)) >= 0)
		    {
			/* send was okay, check for a reply to speedup */
			/* things (background poll is quite slow) */
			for (handle = 0; handle < 100; handle++)
			    if (pkt_check_receive())
				break;

			return 1;
		    } else
			break;
		}
	    }

	    pg->stats.errors_out++;
	    REG(edx) = E_CANT_SEND << 8;
	}
	break;

    case F_TERMINATE:
	if (hdlp == NULL || !hdlp->in_use)
	    REG(edx) = E_BAD_HANDLE << 8;
	else
	    REG(edx) = E_CANT_TERM << 8;

	break;

    case F_GET_ADDRESS:
    	if (LWORD(ecx) < ETH_ALEN) {
	    REG(edx) = E_NO_SPACE << 8;
	    break;
        }
    	REG(ecx) = ETH_ALEN;
    	memcpy(SEG_ADR((char *),es,di),pg->hw_address,ETH_ALEN);
    	return 1;

    case F_RESET_IFACE:
	if (hdlp == NULL || !hdlp->in_use)
	    REG(edx) = E_BAD_HANDLE << 8;
	else
	    REG(edx) = E_CANT_RESET << 8;

	break;

    case F_GET_PARAMS:
	REG(es) = PKTDRV_SEG;
	REG(edi) = PKTDRV_OFF + offsetof(struct pkt_globs,param);
	return 1;

    case F_SET_RCV_MODE:
	if (hdlp == NULL || !hdlp->in_use) {
	    REG(edx) = E_BAD_HANDLE << 8;
	    break;
	}
	if (LWORD(ecx) != 3) {			/* only mode 3 supported */
	    REG(edx) = E_BAD_MODE << 8;
	    break;
	}
	return 1;

    case F_GET_RCV_MODE:
	if (hdlp == NULL || !hdlp->in_use) {
	    REG(edx) = E_BAD_HANDLE << 8;
	    break;
	}
	REG(eax) = 3;
	return 1;

    case F_GET_STATS:
	if (hdlp == NULL || !hdlp->in_use) {
	    REG(edx) = E_BAD_HANDLE << 8;
	    break;
	}
	REG(ds) = PKTDRV_SEG;
	REG(esi) = PKTDRV_OFF + offsetof(struct pkt_globs,stats);
	return 1;

    default:
	/* unhandled function, indicate an error */

	REG(edx) = E_BAD_COMMAND << 8;
	break;
    }

    /* fell through switch, indicate an error (DH set above) */
    REG(eflags) |= CF;

#if 0
    printf("ERR:  AX=%04x BX=%04x CX=%04x DX=%04x FLAGS=%08x\n",
		LWORD(eax),LWORD(ebx),LWORD(ecx),LWORD(edx),REG(eflags));
    printf("      SI=%04x DI=%04x BP=%04x SP=%04x CS=%04x DS=%04x ES=%04x SS=%04x\n",
		LWORD(esi),LWORD(edi),LWORD(ebp),LWORD(esp),
		LWORD(cs),LWORD(ds),LWORD(es),LWORD(ss));
#endif

    return 1;
}

int
pkt_check_receive()

{
    int size,handle;
    struct per_handle *hdlp;
    char *p;
    char device[32];

    if (pg == NULL)				/* packet driver intialized? */
	return -1;

    if (pg->size != 0)				/* transfer area busy? */
	return 1;

    for (handle = 0; handle < MAX_HANDLE; handle++) {
	hdlp = &pg->handle[handle];

	if (hdlp->in_use) {
	    /* attempt to read from this handle's socket */
	    /* it is in NODELAY mode so the read will return immediately */
	    /* when no data is pending */

	    size = ReadFromNetwork(hdlp->sock,device,pg->buf,sizeof(pg->buf));

	    /* check if something has been received */
	    /* there was a check for "not multicast" here, but I think */
	    /* it should not be there... */

	    if (size > 1) {
		/* check if the packet's type matches the specified type */
		/* in the ACCESS_TYPE call.  the position depends on the
		/* driver class! */

		if (hdlp->class == ETHER_CLASS)
		    p = pg->buf + 2 * ETH_ALEN;		/* Ethernet-II */
		else
		    p = pg->buf + 2 * ETH_ALEN + 2;	/* IEEE 802 */

		if (!memcmp(p,hdlp->packet_type,hdlp->packet_type_len)) {
		    pg->stats.packets_in++;
		    pg->stats.bytes_in += size;

		    /* stuff things in global vars and queue a hardware */
		    /* interrupt which will perform the upcall */
		    pg->size = size;
		    pg->receiver = hdlp->receiver;
		    do_hard_int(pg->helpvec);
		    return 1;
		} else
		    pg->stats.errors_in++;	/* not really an error */
	    }
	}
    }

    return 0;
}

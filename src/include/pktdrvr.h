#ifndef PKTDRVR_H
#define PKTDRVR_H
/*
 *
 *     Packet driver emulation for the Linux DOS emulator.
 *
 *
 *     Some modifications by Jason Gorden gorden@jegnixa.hsc.missouri.edu
 *
 *     Additional functions added by Rob Janssen
 *
 */

enum {
	VNET_TYPE_AUTO,
	VNET_TYPE_ETH,
	VNET_TYPE_TAP,
	VNET_TYPE_VDE,
	VNET_TYPE_MAX,
};

#define F_DRIVER_INFO	1
#define F_ACCESS_TYPE	2
#define F_RELEASE_TYPE	3
#define F_SEND_PKT	4
#define F_TERMINATE	5
#define F_GET_ADDRESS	6
#define F_RESET_IFACE	7
#define F_GET_PARAMS	10
#define F_AS_SEND_PKT	11
#define F_SET_RCV_MODE	20
#define F_GET_RCV_MODE	21
#define F_GET_STATS	24
#define F_SET_ADDRESS	25
#define F_RECV_PKT	27

#define E_BAD_HANDLE	1
#define E_NO_CLASS	2
#define E_NO_TYPE	3
#define E_NO_NUMBER	4
#define E_BAD_TYPE	5
#define E_NO_MULTICAST	6
#define E_CANT_TERM	7
#define E_BAD_MODE	8
#define E_NO_SPACE	9
#define E_TYPE_INUSE	10
#define E_BAD_COMMAND	11
#define E_CANT_SEND	12
#define E_CANT_SET	13
#define E_BAD_ADDRESS	14
#define E_CANT_RESET	15

#define ETHER_CLASS	1
#define IEEE_CLASS	11

#define MAX_HANDLE	50

/* return structure for GET_PARAMS */

struct pkt_param {
    unsigned char   major_rev;      /* Revision of Packet Driver spec */
    unsigned char   minor_rev;      /*  this driver conforms to. */
    unsigned char   length;         /* Length of structure in bytes */
    unsigned char   addr_len;       /* Length of a MAC-layer address */
    unsigned short  mtu;            /* MTU, including MAC headers */
    unsigned short  multicast_aval; /* Buffer size for multicast addr */
    unsigned short  rcv_bufs;       /* (# of back-to-back MTU rcvs) - 1 */
    unsigned short  xmt_bufs;       /* (# of successive xmits) - 1 */
    unsigned short  int_num;        /* Interrupt # to hook for post-EOI
				       processing, 0 == none */
};

/* return structure for GET_STATS */

struct pkt_statistics {
    unsigned long   packets_in;     /* Totals across all handles */
    unsigned long   packets_out;
    unsigned long   bytes_in;       /* Including MAC headers */
    unsigned long   bytes_out;
    unsigned long   errors_in;      /* Totals across all error types */
    unsigned long   errors_out;
    unsigned long   packets_lost;   /* No buffer from receiver(), card */
				    /*  out of resources, etc. */
};

extern void pkt_priv_init (void);
extern void pkt_init (void);
extern void pkt_reset (void);
extern void pkt_term (void);

struct pkt_ops {
    int id;
    int (*open)(char *name);
    void (*close)(int);
    int (*get_hw_addr)(unsigned char *addr);
    int (*get_MTU)(void);
    ssize_t (*pkt_read)(int fd, void *buf, size_t count);
    ssize_t (*pkt_write)(int fd, const void *buf, size_t count);
};

extern int pkt_register_backend(struct pkt_ops *o);
extern void pkt_get_fake_mac(unsigned char *addr);

#define PKT_FLG_QUIET 1
void pkt_set_flags(int flags);
void pkt_clear_flags(int flags);
int pkt_get_flags(void);

extern unsigned short receive_mode;

#endif				/* PKTDRVR_H */

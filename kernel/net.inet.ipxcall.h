/*
 *	The following information is in its entirety obtained from:
 *
 *	Novell 'IPX Router Specification' Version 1.10 
 *		Part No. 107-000029-001
 *
 *	Which is available from ftp.novell.com
 */

#ifndef _IPX_H
#define _IPX_H

#include <linux/ddi.h>

typedef struct
{
	unsigned long net;
	unsigned char  node[6]; 
	unsigned short sock;
} ipx_address;

#define ipx_broadcast_node	"\377\377\377\377\377\377"

typedef struct ipx_packet
{
	unsigned short	ipx_checksum;
#define IPX_NO_CHECKSUM	0xFFFF
	unsigned short  ipx_pktsize;
	unsigned char   ipx_tctrl;
	unsigned char   ipx_type;
#define IPX_TYPE_UNKNOWN	0x00
#define IPX_TYPE_RIP		0x01	/* may also be 0 */
#define IPX_TYPE_SAP		0x04	/* may also be 0 */
#define IPX_TYPE_SPX		0x05	/* coming soon... */
#define IPX_TYPE_NCP		0x11	/* $30,000 for docs on this (SPIT) */
#define IPX_TYPE_PPROP		0x14	/* complicated flood fill brdcast */
	ipx_address	ipx_dest __attribute__ ((packed));
	ipx_address	ipx_source __attribute__ ((packed));
} ipx_packet;


typedef struct ipx_route
{
	unsigned long net;
	unsigned char router_node[6];
	unsigned long router_net;
	unsigned short flags;
	struct device *dev;
#define IPX_RT_ROUTED	1		/* This isn't a direct route. Send via this if to node router_node */
#define IPX_RT_BLUEBOOK	2		/* Use DIX 8137 frames not IEE802.3 */
	struct ipx_route *next;
} ipx_route;


typedef struct sock ipx_socket;


extern int ipx_e2_rcv(struct sk_buff *skb, struct device *dev, struct packet_type *pt);
extern int ipx_8023_rcv(struct sk_buff *skb, struct device *dev, struct packet_type *pt);
extern int ipx_8022_rcv(struct sk_buff *skb, struct device *dev, struct packet_type *pt);
extern void ipx_proto_init(struct ddi_proto *pro);


#define AF_IPX_MAJOR 31

#endif


/*
 *	Implements an IPX socket layer (badly - but I'm working on it).
 *
 *	This code is derived from work by
 *		Ross Biro	: 	Writing the original IP stack
 *		Fred Van Kempen :	Tidying up the TCP/IP
 *
 *	Many thanks go to Keith Baker, Institute For Industrial Information
 *	Technology Ltd, Swansea University for allowing me to work on this
 *	in my own time even though it was in some ways related to commercial
 *	work I am currently employed to do there.
 *
 *	All the material in this file is subject to the Gnu license version 2.
 *	Neither Alan Cox nor the Swansea University Computer Society admit liability
 *	nor provide warranty for any of this software. This material is provided 
 *	as is and at no charge.		
 *
 */
 
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/ipx.h>
#include "inet.h"
#include "dev.h"
#include "skbuff.h"
#include "sock.h"
#include <asm/segment.h>
#include <asm/system.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/interrupt.h>

#ifdef CONFIG_IPX
/***********************************************************************************************************************\
*															*
*						Handlers for the socket list.						*
*															*
\***********************************************************************************************************************/

static ipx_socket *volatile ipx_socket_list=NULL;

/*
 *	Note: Sockets may not be removed _during_ an interrupt or inet_bh
 *	handler using this technique. They can be added although we do not
 *	use this facility.
 */
 
static void ipx_remove_socket(ipx_socket *sk)
{
	ipx_socket *s;
	
	cli();
	s=ipx_socket_list;
	if(s==sk)
	{
		ipx_socket_list=s->next;
		sti();
		return;
	}
	while(s && s->next)
	{
		if(s->next==sk)
		{
			s->next=sk->next;
			sti();
			return;
		}
		s=s->next;
	}
	sti();
}

static void ipx_insert_socket(ipx_socket *sk)
{
	cli();
	sk->next=ipx_socket_list;
	ipx_socket_list=sk;
	sti();
}

static ipx_socket *ipx_find_socket(int port)
{
	ipx_socket *s;
	s=ipx_socket_list;
	while(s)
	{
		if(s->ipx_source_addr.sock==port)
		{
			return(s);
		}
		s=s->next;
	}
	return(NULL);
}

/*
 *	This is only called from user mode. Thus it protects itself against
 *	interrupt users but doesn't worry about being called during work.
 *	Once it is removed from the queue no interrupt or bottom half will
 *	touch it and we are (fairly 8-) ) safe.
 */
 
static void ipx_destroy_socket(ipx_socket *sk)
{
	struct sk_buff *skb;
	ipx_remove_socket(sk);
	
	while((skb=skb_dequeue(&sk->rqueue))!=NULL)
	{
		kfree_skb(skb,FREE_READ);
	}
	
	kfree_s(sk,sizeof(*sk));
}

/*******************************************************************************************************************\
*													            *
*	            			Routing tables for the IPX socket layer				            *
*														    *
\*******************************************************************************************************************/


static ipx_route *ipx_router_list=NULL;
static ipx_route *ipx_local_net=NULL;

static ipx_route *ipxrtr_get_dev(long net)
{
	ipx_route *r;
	unsigned long flags;
	save_flags(flags);
	cli();
	r=ipx_router_list;
	while(r!=NULL)
	{
		if(r->net==net)
		{
			restore_flags(flags);
			return r;
		}
		r=r->next;
	}
	restore_flags(flags);
	return NULL;
}

static int ipxrtr_create(struct ipx_route_def *r)
{
	ipx_route *new;
	struct device *dev;
	if(ipxrtr_get_dev(r->ipx_network))
		return -EEXIST;
	if(r->ipx_router_network!=0)
	{
		/* Adding an indirect route */
		ipx_route *rt=ipxrtr_get_dev(r->ipx_router_network);
		if(rt==NULL)
			return -ENETUNREACH;
		if(rt->flags&IPX_RT_ROUTED)
			return -EMULTIHOP;
		new=(ipx_route *)kmalloc(sizeof(ipx_route),GFP_ATOMIC);	/* Because we are brave and don't lock the table! */
		if(new==NULL)
			return -EAGAIN;
		new->net=r->ipx_network;
		new->router_net=r->ipx_router_network;
		memcpy(new->router_node,r->ipx_router_node,sizeof(new->router_node));
		new->flags=(rt->flags&IPX_RT_BLUEBOOK)|IPX_RT_ROUTED;
		new->flags |= (rt->flags&IPX_RT_8022);
		new->dev=rt->dev;
		new->next=ipx_router_list;
		ipx_router_list=new;
		return 0;
	}

	/* Add a direct route */
	if (ipx_local_net != NULL)
		return -EEXIST;
	dev=dev_get(r->ipx_device);
	if(dev==NULL)
		return -ENODEV;
	/* Check addresses are suitable */
	if(dev->addr_len>6)
		return -EINVAL;
	if(dev->addr_len<2)
		return -EINVAL;
	/* Ok now create */
	new=(ipx_route *)kmalloc(sizeof(ipx_route),GFP_KERNEL);
	if(new==NULL)
		return -ENOMEM;

	new->router_net=0;
	new->dev=dev;
	new->net=r->ipx_network;
	new->flags=r->ipx_flags&IPX_RT_BLUEBOOK;
	new->flags |= (r->ipx_flags&IPX_RT_8022);
	cli();
	new->next=ipx_router_list;
	ipx_router_list=new;
	ipx_local_net=new;
	sti();
	return 0;
}

static int ipxrtr_delete(long net)
{
	ipx_route *r=ipx_router_list;
	if(r->net==net)
	{
		ipx_router_list=r->next;
		return 0;
	}
	while(r->next!=NULL)
	{
		if(r->next->net==net)
		{
			ipx_route *d=r->next;
			r->next=d->next;
			if (d == ipx_local_net)
				ipx_local_net = NULL;
			kfree_s(d,sizeof(ipx_route));
			return 0;
		}
		r=r->next;
	}
	return -ENOENT;
}

static int ipxrtr_ioctl(unsigned int cmd, void *arg)
{
	int err;
	switch(cmd)
	{
		case SIOCDELRT:
			err=verify_area(VERIFY_READ,arg,sizeof(long));
			if(err)
				return err;
			return ipxrtr_delete(get_fs_long(arg));
		case SIOCADDRT:
		{
			struct ipx_route_def f;
			err=verify_area(VERIFY_READ,arg,sizeof(f));
			if(err)
				return err;
			memcpy_fromfs(&f,arg,sizeof(f));
			return ipxrtr_create(&f);
		}
		default:
			return -EINVAL;
	}
}

/*******************************************************************************************************************\
*													            *
*	      Handling for system calls applied via the various interfaces to an IPX socket object		    *
*														    *
\*******************************************************************************************************************/
 
static int ipx_fcntl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	ipx_socket *sk;
	
	sk=(ipx_socket *)sock->data;
	
	if(sk==NULL)
	{
		printk("IPX:fcntl:passed sock->data=NULL\n");
		return(0);
	}
	
	switch(cmd)
	{
		default:
			return(-EINVAL);
	}
}

static int ipx_setsockopt(struct socket *sock, int level, int optname,
	char *optval, int optlen)
{
	ipx_socket *sk;
	int err,opt;
	
	sk=(ipx_socket *)sock->data;
	
	if(sk==NULL)
	{
		printk("IPX:setsockopt:passed sock->data=NULL\n");
		return 0;
	}
	
	if(level!=SOL_SOCKET)
		return(-EOPNOTSUPP);
	if(optval==NULL)
		return(-EINVAL);
	err=verify_area(VERIFY_READ,optval,sizeof(int));
	if(err)
		return err;
	opt=get_fs_long((unsigned long *)optval);
	
	switch(optname)
	{
		case SO_TYPE:
			sk->ipx_type=(unsigned char)opt;
			return(0);
		case SO_ERROR:
			return -ENOPROTOOPT;
		case SO_DEBUG:
			sk->debug=(opt==0)?0:1;
			return 0;
		case SO_DONTROUTE:
			return -ENOPROTOOPT;
		case SO_BROADCAST:
			sk->broadcast=(opt==0)?0:1;
			return(0);
		case SO_SNDBUF:
			if((opt!=0&& opt<sizeof(ipx_packet)+64)||opt>8192)
				return -EINVAL;
			sk->sndbuf=opt;
			if(opt==0)
				sk->sndbuf=IPX_MTU;
			return 0;
		case SO_RCVBUF:
			if((opt!=0 && opt<sizeof(ipx_packet)+64)||opt>8192)
				return -EINVAL;
			sk->rcvbuf=opt;
			if(opt==0)
				sk->rcvbuf=IPX_MTU;
			return 0;
		default:
			return(-ENOPROTOOPT);
	}
}

static int ipx_getsockopt(struct socket *sock, int level, int optname,
	char *optval, int *optlen)
{
	ipx_socket *sk;
	int val=0;
	int err;
	
	if(level!=SOL_SOCKET)
		return(-EOPNOTSUPP);
	sk=(ipx_socket *)sock->data;
	if(sk==NULL)
	{
		printk("IPX:getsockopt:passed NULL sock->data.\n");
		return 0;
	}
	
	switch(optname)
	{
		case SO_DEBUG:
			val=sk->debug;
		case SO_DONTROUTE:
			break;
		case SO_BROADCAST:
			val=sk->broadcast;
			break;
		case SO_SNDBUF:
			val=sk->sndbuf;
		case SO_RCVBUF:
			val=sk->rcvbuf;
			break;
		case SO_TYPE:
			val=sk->ipx_type;
			break;
		case SO_ERROR:
			val=sk->err;
			break;
		default:
			return(-ENOPROTOOPT);
	}
	err=verify_area(VERIFY_WRITE,optlen,sizeof(int));
	if(err)
		return err;
	put_fs_long(sizeof(int),(unsigned long *)optlen);
	err=verify_area(VERIFY_WRITE,optval,sizeof(int));
	put_fs_long(val,(unsigned long *)optval);
	return(0);
}

static int ipx_listen(struct socket *sock, int backlog)
{
	return -EOPNOTSUPP;
}

static int ipx_create(struct socket *sock, int protocol)
{
	ipx_socket *sk;
	sk=(ipx_socket *)kmalloc(sizeof(*sk),GFP_KERNEL);
	if(sk==NULL)
		return(-ENOMEM);
	switch(sock->type)
	{
		case SOCK_DGRAM:
			break;
		default:
			kfree_s((void *)sk,sizeof(*sk));
			return(-ESOCKTNOSUPPORT);
	}
	sk->rmem_alloc=0;
	sk->dead=0;
	sk->next=NULL;
	sk->broadcast=0;
	sk->rcvbuf=SK_RMEM_MAX;
	sk->sndbuf=SK_WMEM_MAX;
	sk->wmem_alloc=0;
	sk->rmem_alloc=0;
	sk->inuse=0;
	sk->dead=0;
	sk->prot=NULL;	/* So we use default free mechanisms */
	sk->broadcast=0;
	sk->shutdown=0;
	sk->err=0;
	sk->rqueue=NULL;
	sk->wback=NULL;
	sk->wfront=NULL;
	sk->send_head=NULL;
	sk->back_log=NULL;
	sk->state=TCP_CLOSE;
	sk->ipx_type=0;
	
	memset(&sk->ipx_dest_addr,'\0',sizeof(sk->ipx_dest_addr));
	memset(&sk->ipx_source_addr,'\0',sizeof(sk->ipx_source_addr));
	sk->mtu=IPX_MTU;
	sock->data=(void *)sk;
	sk->sleep=sock->wait;
	sk->zapped=1;
	sk->socket = sock;
	return(0);
}

static int ipx_dup(struct socket *newsock,struct socket *oldsock)
{
	return(ipx_create(newsock,SOCK_DGRAM));
}

static int ipx_release(struct socket *sock, struct socket *peer)
{
	ipx_socket *sk=(ipx_socket *)sock->data;
	if(sk==NULL)
		return(0);
	wake_up(sk->sleep);
	sk->dead=1;
	sock->data=NULL;
	ipx_destroy_socket(sk);
	return(0);
}
		
static unsigned short	socketNumber = 0x4000;

static int ipx_bind(struct socket *sock, struct sockaddr *uaddr,int addr_len)
{
	ipx_socket *sk;
	int err;
	struct sockaddr_ipx addr;
	struct ipx_route *rt;
	
	sk=(ipx_socket *)sock->data;
	if(sk==NULL)
	{
		printk("IPX:bind:sock->data=NULL\n");
		return 0;
	}
	
	if(sk->zapped==0)
		return(-EIO);
		
	err=verify_area(VERIFY_READ,uaddr,addr_len);
	if(err)
		return err;
	if(addr_len!=sizeof(addr))
		return -EINVAL;
	memcpy_fromfs(&addr,uaddr,addr_len);
	
/*
	if(ntohs(addr.sipx_port)<0x4000 && !suser())
		return(-EPERM);
*/
	
	/* Source addresses are easy. It must be our network:node pair for
	   an interface routed to IPX with the ipx routing ioctl() */

	if (addr.sipx_port == 0) {
		while (socketNumber < 0x8000 && ipx_find_socket(htons(socketNumber)) != NULL) {
			socketNumber++;
		}
		if (socketNumber == 0x8000) {
			printk("IPX: Out of sockets\n");
			return -EINVAL;
		}
		addr.sipx_port = htons(socketNumber);
		if(sk->debug)
			printk("IPX: socket number is %x\n", socketNumber);
		socketNumber++;
		if (socketNumber > 0x7ffc) {
			/* Wrap around */
			socketNumber = 0x4000;
		}
	}


	if(ipx_find_socket(addr.sipx_port)!=NULL)
	{
		if(sk->debug)
			printk("IPX: bind failed because port %X in use.\n",
				(int)addr.sipx_port);
		return(-EADDRINUSE);	   
	}
	sk->ipx_source_addr.sock=addr.sipx_port;
	memcpy(sk->ipx_source_addr.node,addr.sipx_node,sizeof(sk->ipx_source_addr.node));
	if (addr.sipx_network == 0L) {
		if (ipx_local_net == NULL)
			return -EADDRNOTAVAIL;

		addr.sipx_network = ipx_local_net->net;
	}
	sk->ipx_source_addr.net=addr.sipx_network;
	if((rt=ipxrtr_get_dev(sk->ipx_source_addr.net))==NULL)
	{
		if(sk->debug)
			printk("IPX: bind failed (no device for net %X)\n",
				sk->ipx_source_addr.net);
		return(-EADDRNOTAVAIL);
	}
	memset(sk->ipx_source_addr.node,'\0',6);
	memcpy(sk->ipx_source_addr.node,rt->dev->dev_addr,rt->dev->addr_len);
	ipx_insert_socket(sk);
	sk->zapped=0;
	if(sk->debug)
		printk("IPX: socket is bound.\n");
	return(0);
}

static int ipx_connect(struct socket *sock, struct sockaddr *uaddr,
	int addr_len, int flags)
{
	ipx_socket *sk=(ipx_socket *)sock->data;
	struct sockaddr_ipx addr;
	int err;
	
	if(sk==NULL)
	{
		printk("IPX:connect:sock->data=NULL!\n");
		return 0;
	}

	sk->state = TCP_CLOSE;	
	sock->state = SS_UNCONNECTED;
	
	if(addr_len!=sizeof(addr))
		return(-EINVAL);
	err=verify_area(VERIFY_READ,uaddr,addr_len);
	if(err)
		return err;
	memcpy_fromfs(&addr,uaddr,sizeof(addr));
	
	if(ntohs(addr.sipx_port)<0x4000 && !suser())
		return -EPERM;
	if(sk->ipx_source_addr.net==0)	/* Must bind first - no autobinding in this */
		return -EINVAL;
		
	
	sk->ipx_dest_addr.net=addr.sipx_network;
	sk->ipx_dest_addr.sock=addr.sipx_port;
	memcpy(sk->ipx_dest_addr.node,addr.sipx_node,sizeof(sk->ipx_source_addr.node));
	if(ipxrtr_get_dev(sk->ipx_dest_addr.net)==NULL)
		return -ENETUNREACH;
	sock->state = SS_CONNECTED;
	sk->state=TCP_ESTABLISHED;
	return(0);
}

static int ipx_socketpair(struct socket *sock1, struct socket *sock2)
{
	return(-EOPNOTSUPP);
}

static int ipx_accept(struct socket *sock, struct socket *newsock, int flags)
{
	if(newsock->data)
		kfree_s(newsock->data,sizeof(ipx_socket));
	return -EOPNOTSUPP;
}

static int ipx_getname(struct socket *sock, struct sockaddr *uaddr,
	int *uaddr_len, int peer)
{
	ipx_address *addr;
	struct sockaddr_ipx sipx;
	ipx_socket *sk;
	int len;
	int err;
	
	sk=(ipx_socket *)sock->data;
	
	err = verify_area(VERIFY_WRITE,uaddr_len,sizeof(long));
	if(err)
		return err;
		
	len = get_fs_long(uaddr_len);
	
	err = verify_area(VERIFY_WRITE, uaddr, len);
	if(err)
		return err;
	
	if(len<sizeof(struct sockaddr_ipx))
		return -EINVAL;
		
	if(peer)
	{
		if(sk->state!=TCP_ESTABLISHED)
			return -ENOTCONN;
		addr=&sk->ipx_dest_addr;
	}
	else
		addr=&sk->ipx_source_addr;
		
	sipx.sipx_family = AF_IPX;
	sipx.sipx_port = addr->sock;
	sipx.sipx_network = addr->net;
	memcpy(sipx.sipx_node,addr->node,sizeof(sipx.sipx_node));
	memcpy_tofs(uaddr,&sipx,sizeof(sipx));
	put_fs_long(len,uaddr_len);
	return(0);
}


int ipx_rcv(struct sk_buff *skb, struct device *dev, struct packet_type *pt)
{
	ipx_socket *sock;
	ipx_packet *ipx;
	ipx_route *rt;
	
	ipx = skb->h.ipx;
	
#if 0
printk("IPX got packet: ");
printk("CHECKSUM == %x\n", ipx->ipx_checksum);
printk("SIZE == %x\n", ntohs(ipx->ipx_pktsize));
printk("HOPS == %x\n", ipx->ipx_tctrl);
printk("DEST:\n");
printk("NET ==  %x\n", ipx->ipx_dest.net);
printk("NODE == ");
{
	int i;
	for (i = 0; i < 6; i++) {
		printk("%x", ipx->ipx_dest.node[i]);
	}
}
printk("\n");
printk("SOCKET == %x\n", ipx->ipx_dest.sock);
printk("SOURCE:\n");
printk("NET ==  %x\n", ipx->ipx_source.net);
printk("NODE == ");
{
	int i;
	for (i = 0; i < 6; i++) {
		printk("%x", ipx->ipx_source.node[i]);
	}
}
printk("\n");
printk("SOCKET == %x\n", ipx->ipx_source.sock);
#endif
		

	if(ipx->ipx_checksum!=IPX_NO_CHECKSUM)
	{
		printk("IPX frame received with user level checksum. Facility not supported.\n");
		kfree_skb(skb,FREE_READ);
		return(0);
	}
	
	if(ntohs(ipx->ipx_pktsize)<sizeof(ipx_packet))
	{
		printk("IPX: Dropping unusually small packet %x\n", ipx->ipx_pktsize);
		kfree_skb(skb,FREE_READ);
		return(0);
	}
	
	if(ipx->ipx_tctrl>16)
	{
		printk("IPX: Too many hops\n");
		kfree_skb(skb,FREE_READ);
		return(0);
	}
	
	if(memcmp(dev->dev_addr,ipx->ipx_dest.node,dev->addr_len)!=0
	     && memcmp(ipx_broadcast_node,ipx->ipx_dest.node,dev->addr_len)!=0)
	{
		printk("IPX routing not yet supported.\n");
		kfree_skb(skb,FREE_READ);
		return(0);
	}
	
	rt=ipxrtr_get_dev(ipx->ipx_dest.net);
	
	if(ipx->ipx_dest.net && rt!=NULL && rt->dev!=dev)
	{
		printk("IPX network number mismatch: Check configuration.\n");
		kfree_skb(skb,FREE_READ);
		return(0);
	}
	
	if(ipx->ipx_dest.net && rt==NULL)
	{
		/* Apparently we don't know our local net; let's see */
		rt=ipxrtr_get_dev(0L);
		if (rt == NULL) {
printk("IPX: Not registered.\n");
			kfree_skb(skb,FREE_READ);
			return 0;
		}
		if (rt->router_net != 0L) {
		printk("IPX network number mismatch: Check configuration.\n");
			kfree_skb(skb,FREE_READ);
			return 0;
		}
		/* Now we know our local net */
printk("Setting local net to %x\n", ipx->ipx_dest.net);
		rt->net = ipx->ipx_dest.net;
	}
	
	/* Ok its for us ! */
	
	sock=ipx_find_socket(ipx->ipx_dest.sock);
	if(sock==NULL)	/* But not one of our sockets */
	{
		kfree_skb(skb,FREE_READ);
		return(0);
	}
	
#if 0
	if(sock->rmem_alloc>=sock->rcvbuf)
	{
		printk("Socket is full\n");
		kfree_skb(skb,FREE_READ);	/* Socket is full */
		return(0);
	}
#endif
	
	sock->rmem_alloc+=skb->mem_len;

	skb_queue_tail(&sock->rqueue,skb);
	if(!sock->dead) {
		wake_up(sock->sleep);
	}

	return(0);
}

int ipx_e2_rcv(struct sk_buff *skb, struct device *dev, 
			struct packet_type *pt)
{
	if ((ipx_local_net == NULL) || !(ipx_local_net->flags&IPX_RT_BLUEBOOK)) {
		kfree_skb(skb,FREE_READ);
		return 0;
	}

	return ipx_rcv(skb, dev, pt);
}

int ipx_8023_rcv(struct sk_buff *skb, struct device *dev, 
			struct packet_type *pt)
{
	if ((ipx_local_net == NULL) || (ipx_local_net->flags != 0)) {
		kfree_skb(skb,FREE_READ);
		return 0;
	}

	return ipx_rcv(skb, dev, pt);
}

int ipx_8022_rcv(struct sk_buff *skb, struct device *dev, 
			struct packet_type *pt)
{
	if ((ipx_local_net == NULL) || !(ipx_local_net->flags&IPX_RT_8022)) {
		kfree_skb(skb,FREE_READ);
		return 0;
	}

	return ipx_rcv(skb, dev, pt);
}

static int ipx_sendto(struct socket *sock, void *ubuf, int len, int noblock,
	unsigned flags, struct sockaddr *usip, int addr_len)
{
	ipx_socket *sk=(ipx_socket *)sock->data;
	struct sockaddr_ipx *usipx=(struct sockaddr_ipx *)usip;
	int err;
	struct sockaddr_ipx sipx;
	struct sk_buff *skb;
	struct device *dev;
	struct ipx_packet *ipx;
	int size;
	ipx_route *rt;
	struct ethhdr *eth;
	unsigned char *hp;

	/* FILL ME IN */
	if(flags)
		return -EINVAL;
	if(len<0)
		return -EINVAL;
	if(len == 0)
		return 0;
		
	if(usipx)
	{
		if(addr_len <sizeof(sipx))
			return(-EINVAL);
		err=verify_area(VERIFY_READ,usipx,sizeof(sipx));
		if(err)
			return(err);
		memcpy_fromfs(&sipx,usipx,sizeof(sipx));
		if(sipx.sipx_family != AF_IPX)
			return -EINVAL;
	}
	else
	{
		if(sk->state!=TCP_ESTABLISHED)
			return -ENOTCONN;
		sipx.sipx_family=AF_IPX;
		sipx.sipx_port=sk->ipx_dest_addr.sock;
		sipx.sipx_network=sk->ipx_dest_addr.net;
		memcpy(sipx.sipx_node,sk->ipx_dest_addr.node,sizeof(sipx.sipx_node));
	}
	
	if(sk->debug)
		printk("IPX: sendto: Addresses built.\n");
	if(!sk->broadcast && memcmp(&sipx.sipx_node,&ipx_broadcast_node,6)==0)
		return -ENETUNREACH;
	/* Build a packet */
	
	if(sk->debug)
		printk("IPX: sendto: building packet.\n");
	err=verify_area(VERIFY_READ,ubuf,len);
	if(err)
		return err;
		
	size=sizeof(struct sk_buff)+sizeof(ipx_packet)+len;	/* For mac headers */

	if (sipx.sipx_network == 0L) {
		/*
		 *	Set local network number in source address.
		 */
		if (ipx_local_net == NULL)
			return -ENETUNREACH;

		sipx.sipx_network = ipx_local_net->net;
	}

	rt=ipxrtr_get_dev(sipx.sipx_network);
	if(rt==NULL)
	{
		return -ENETUNREACH;
	}
	
	dev=rt->dev;
	size+=dev->hard_header_len;
		
	if (rt->flags&IPX_RT_8022) {
		size+=3;
	}

	if(size+sk->wmem_alloc>sk->sndbuf)
		return -EAGAIN;
		
	sk->wmem_alloc+=size;
	
	skb=alloc_skb(size,GFP_KERNEL);
	if(skb==NULL)
		return -ENOMEM;
		
	skb->mem_addr=skb;
	skb->mem_len=size;
	skb->sk=sk;
	skb->free=1;
	skb->arp=1;
	skb->next = skb->prev = NULL;
	skb->magic = 0;
	skb->len=size-sizeof(struct sk_buff);

	if(sk->debug)
		printk("Building MAC header.\n");		
	eth=(struct ethhdr *)(skb+1);
	skb->dev=rt->dev;
	if(rt->flags&IPX_RT_BLUEBOOK)
		eth->h_proto=ntohs(ETH_P_IPX);	
	else {
		int elen = len+sizeof(ipx_packet);
		if (rt->flags&IPX_RT_8022)
			elen += 3;
		eth->h_proto=htons(elen);
	}
		
	/* See if its for a router or for us */
	if(rt->flags&IPX_RT_ROUTED)
		memcpy(eth->h_dest,rt->router_node,sizeof(eth->h_dest));
	else
		memcpy(eth->h_dest,sipx.sipx_node,sizeof(eth->h_dest));
	memcpy(eth->h_source,rt->dev->dev_addr,rt->dev->addr_len);
	hp = (unsigned char *)(eth + 1);

	if (rt->flags&IPX_RT_8022) {
		*hp = *(hp+1) = 0xE0;
		*(hp+2) = 0x03;
		hp += 3;
	}
	
	/* Now the IPX */
	if(sk->debug)
		printk("Building IPX Header.\n");
	ipx=(ipx_packet *) hp;
	ipx->ipx_checksum=0xFFFF;
	ipx->ipx_pktsize=htons(len+sizeof(ipx_packet));
	ipx->ipx_tctrl=0;
	ipx->ipx_type=sk->ipx_type;
	/*
	 *	See if we have resolved our local net yet.
	 */
	if (sk->ipx_source_addr.net == 0L) {
		if (ipx_local_net != NULL)
			sk->ipx_source_addr.net = ipx_local_net->net;
	}
	memcpy(&ipx->ipx_source,&sk->ipx_source_addr,sizeof(ipx->ipx_source));
	
	ipx->ipx_dest.net=sipx.sipx_network;
	memcpy(ipx->ipx_dest.node,sipx.sipx_node,sizeof(ipx->ipx_dest.node));
	ipx->ipx_dest.sock=sipx.sipx_port;
	if(sk->debug)
		printk("IPX: Appending user data.\n");
	/* User data follows immediately after the IPX data */
	memcpy_fromfs((char *)(ipx+1),ubuf,len);
	if(sk->debug)
		printk("IPX: Transmitting buffer\n");
	dev->queue_xmit(skb,dev,SOPRI_NORMAL);
	return len;
}

static int ipx_send(struct socket *sock, void *ubuf, int size, int noblock, unsigned flags)
{
	return ipx_sendto(sock,ubuf,size,noblock,flags,NULL,0);
}

static int ipx_write(struct socket *sock, char *ubuf, int size, int noblock)
{
	return ipx_send(sock,ubuf,size,noblock,0);
}

static int ipx_recvfrom(struct socket *sock, void *ubuf, int size, int noblock,
		   unsigned flags, struct sockaddr *sip, int *addr_len)
{
	ipx_socket *sk=(ipx_socket *)sock->data;
	struct sockaddr_ipx *sipx=(struct sockaddr_ipx *)sip;
	/* FILL ME IN */
	int copied = 0;
	struct sk_buff *skb;
	int er;
	
	if(sk->err)
	{
		er= -sk->err;
		sk->err=0;
		return er;
	}
	
	if(size==0)
		return 0;
	if(size<0)
		return -EINVAL;
	if(addr_len)
	{
		er=verify_area(VERIFY_WRITE,addr_len,sizeof(*addr_len));
		if(er)
			return er;
		put_fs_long(sizeof(*sipx),addr_len);
	}
	if(sipx)
	{
		er=verify_area(VERIFY_WRITE,sipx,sizeof(*sipx));
		if(er)
			return er;
	}
	er=verify_area(VERIFY_WRITE,ubuf,size);
	if(er)
		return er;
	skb=skb_recv_datagram(sk,flags,noblock,&er);
	if(skb==NULL)
		return er;
	copied = ntohs(skb->h.ipx->ipx_pktsize) - sizeof(ipx_packet);
	if (size < copied)
		copied = size;
	skb_copy_datagram(skb,sizeof(struct ipx_packet),ubuf,copied);
	
	if(sipx)
	{
		struct sockaddr_ipx addr;
		
		addr.sipx_family=AF_IPX;
		addr.sipx_port=skb->h.ipx->ipx_source.sock;
		memcpy(addr.sipx_node,skb->h.ipx->ipx_source.node,sizeof(addr.sipx_node));
		addr.sipx_network=skb->h.ipx->ipx_source.net;
		memcpy_tofs(sipx,&addr,sizeof(*sipx));
	}
	skb_free_datagram(skb);
	return(copied);
}		

static int ipx_recv(struct socket *sock, void *ubuf, int size , int noblock,
	unsigned flags)
{
	ipx_socket *sk=(ipx_socket *)sock->data;
	if(sk->zapped)
		return -ENOTCONN;
	return ipx_recvfrom(sock,ubuf,size,noblock,flags,NULL, NULL);
}

static int ipx_read(struct socket *sock, char *ubuf, int size, int noblock)
{
	return ipx_recv(sock,ubuf,size,noblock,0);
}


static int ipx_shutdown(struct socket *sk,int how)
{
	return -EOPNOTSUPP;
}

static int ipx_select(struct socket *sock , int sel_type, select_table *wait)
{
	ipx_socket *sk=(ipx_socket *)sock->data;
	
	return datagram_select(sk,sel_type,wait);
}

static int ipx_ioctl(struct socket *sock,unsigned int cmd, unsigned long arg)
{
	
	switch(cmd)
	{
		case SIOCADDRT:
		case SIOCDELRT:
			if(!suser())
				return -EPERM;
			return(ipxrtr_ioctl(cmd,(void *)arg));
		default:
			return -EINVAL;
	}
	/*NOTREACHED*/
	return(0);
}

static int ipx_fioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int minor;
	minor=MINOR(inode->i_rdev);
	if(minor!=0)
		return(-ENODEV);
	return ipx_ioctl(NULL,cmd,arg);
}
	
static struct file_operations ipx_fops = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	ipx_fioctl,	/* We have ioctl */
	NULL,
	NULL,
	NULL
};


static struct proto_ops ipx_proto_ops = {
	AF_IPX,
	
	ipx_create,
	ipx_dup,
	ipx_release,
	ipx_bind,
	ipx_connect,
	ipx_socketpair,
	ipx_accept,
	ipx_getname,
	ipx_read,
	ipx_write,
	ipx_select,
	ipx_ioctl,
	ipx_listen,
	ipx_send,
	ipx_recv,
	ipx_sendto,
	ipx_recvfrom,
	ipx_shutdown,
	ipx_setsockopt,
	ipx_getsockopt,
	ipx_fcntl,
};


/* Called by ddi.c on kernel start up */

void ipx_proto_init(struct ddi_proto *pro)
{
	if(register_chrdev(AF_IPX_MAJOR,"af_ipx", &ipx_fops) < 0) {
		printk("%s: cannot register major device %d!\n",
			pro->name, AF_IPX_MAJOR);
		return;
	}
	
	(void) sock_register(ipx_proto_ops.family, &ipx_proto_ops);
	
	printk("Swansea University Computer Society IPX 0.10 ALPHA\n");
	
}


#endif

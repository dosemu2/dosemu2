/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Pseudo-driver for the dosnet interface.
 *
 * Authors:	Vinod G Kulkarni    vinod@cse.iitb.ernet.in
 *              Adapted from the skeleton device driver for linux in the
 *              kernel sources.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/socket.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/in.h>
#include <linux/if_ether.h>	/* For the statistics structure. */

#include <asm/system.h>
#include <asm/segment.h>
#include <asm/io.h>

#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#define MODULE
#ifdef MODULE
#include <linux/module.h>
#include "/usr/src/linux/tools/version.h"
#endif

#include "dosnet.h"

unsigned char *dsn_eth_address=DOSNET_FAKED_ETH_ADDRESS ;


/*
 *	Determine the packet's "special" protocol ID if the destination
 *      ethernet addresses start with  precisely chars "db". Protocol
 *      id is then given by the 3rd and 4th fields, and it should not match 
 *      any of existing one.
 *      If this condition is not satisfied, call original eth_type_trans.	
 */
unsigned short int dosnet_eth_type_trans(struct sk_buff *skb, struct device *dev)
{
	struct ethhdr *eth = (struct ethhdr *) skb->data;

        /* This is quite tricky. 
           See the notes in dosnet_xmit.  */

        /* cases 2,3,7,8: destination is dosnet */
        if( (eth->h_dest[0] == dsn_eth_address[0]) &&
                   (eth->h_dest[1] == dsn_eth_address[1]) ) {
              return( htons(*(unsigned short int *)&(eth->h_dest[2])) );
        }
        /* cases  {1,3,6,7} - {3,7} = {1,6} */
        else if ( (eth->h_source[0] == dsn_eth_address[0]) &&
                  (eth->h_source[1] == dsn_eth_address[1]) ) {
              return(eth_type_trans(skb, dev)); 	
        }
        /* cases 4 and 5 are ignored. */
        return( htons(DOSNET_INVALID_TYPE) );
}


static int
dosnet_xmit(struct sk_buff *skb, struct device *dev)
{
  struct sk_buff *new_skb, *new_skb2;
  struct enet_statistics *stats = (struct enet_statistics *)dev->priv;
  struct ethhdr *eth; 

  if (skb == NULL || dev == NULL) return(0);

  cli();
  if (dev->tbusy != 0) {
	sti();
	stats->tx_errors++;
	return(1);
  }
  dev->tbusy = 1;
  sti();
  
  /* Now the trickiest part. send this packet only if valid:
     There are many types of packets received: 
     Interfaces=linux and dosemu's. Distinguished using ethernet addresses.
     Source      Dest.   Type of packet      Action
  1  dosemu      linux   normal              Send with orig. type field      
  2  linux       dosemu  normal              Send with dosemu's type field      
  3  dosemu      dosemu  normal              Send with dosemu's type field
  4  linux       linux   normal              ignore
      Broadcast packets: dosemu -> linux : duplicate a packet. It gives rise
      to these cases.
  5  linux       linux   broadcast           ignore 
  6  dosemu	 linux   broadcast           (duplicated packet) orig type field. 
  7  dosemu	 dosemu  broadcast           send with dosemu's broadcast type field 
  8  linux       dosemu  broadcast           same as 7. (no duplication.)

     How to do this now??? 
     OK, here we only need to check linux-linux ones and skip them.
     But But But ... these don't come often so let us handle all cases at 
     type_trans  only !
  */

  new_skb = alloc_skb(skb->len, GFP_ATOMIC);
  if (new_skb == NULL) {
	printk("%s: Memory squeeze, dropping packet.\n", dev->name);
	stats->rx_dropped++;
        dev->tbusy = 0;
        return(1); 
  }
  new_skb->len = skb->len;
  new_skb->dev = skb->dev;
 
  memcpy(new_skb->data, skb->data, skb->len);

  dev_kfree_skb(skb, FREE_WRITE);
  stats->tx_packets++;
  dev->tbusy = 0;

  /* When it is a broadcast packet, we need to duplicate this packet under 
     one special case - case 6. When dosemu broadcasts, it should be sent 
     to all other dosemu's AND  linux. Duplicated packet -> no change in
     dest address. Original broadcast packet: change dest. address.
  */
  eth = (struct ethhdr *) new_skb->data;
  if (  (eth->h_dest[0] == 0xff) && (eth->h_dest[1] == 0xff) ) {
        if( (eth->h_source[0] == dsn_eth_address[0]) &&
            (eth->h_source[1] == dsn_eth_address[1]) ) {
                  /* Broadcast packet by dosemu. Duplicate and send to 
                     linux, and send to other dosemu's.
                  */
		  new_skb2 = alloc_skb(new_skb->len, GFP_ATOMIC);
		  if (new_skb2 != NULL) {
		     new_skb2->len = new_skb->len;
		     new_skb2->dev = new_skb->dev;
                  }
            memcpy(new_skb2->data, new_skb->data, new_skb->len);
            /* It is broadcast packet with source=dosemu, so retain it as is. */
            netif_rx(new_skb2);
        }
        memcpy(eth->h_dest, DOSNET_BROADCAST_ADDRESS, 6 );
  }
  netif_rx(new_skb);

  stats->rx_packets++;

  return (0);
}

/* Promiscuous mode: we are always in promiscus mode, so do nothing. ...! */
static void set_multicast_list(struct device *dev, int num_addrs, void *addrs)
{
}

static struct enet_statistics *
get_stats(struct device *dev)
{
    return (struct enet_statistics *)dev->priv;
}

static int dosnet_open(struct device *dev)
{
/*	dev->flags|=IFF_LOOPBACK; */
  int i;
  unsigned char dummyethaddr[6]= { 0x00, 0x00, 'U', 'M', 'M', 'Y' };
  for(i=0; i<6; i++) {
    dev->dev_addr[i]=dummyethaddr[i];
  }
#ifdef MODULE
  MOD_INC_USE_COUNT;
#endif       
	return 0;
}
static int dosnet_close(struct device *dev)
{
#ifdef MODULE
    MOD_DEC_USE_COUNT;
#endif    
        return 0;
}



/* Initialize the rest of the DOSNET device. */
int
dosnet_init(struct device *dev)
{
  int i;
  unsigned char broadcast[6]= { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

  dev->mtu		= 1500;			/* MTU			*/
  dev->tbusy		= 0;
  dev->hard_start_xmit	= dosnet_xmit;
  dev->open		= dosnet_open;
  for(i=0; i<6; i++) {
    dev->broadcast[i]=broadcast[i];
  }
  ether_setup(dev);  

  dev->hard_header	= eth_header;
  dev->hard_header_len	= ETH_HLEN;		/* 14			*/
  dev->addr_len		= ETH_ALEN;		/* 6			*/
  dev->type		= ARPHRD_ETHER;		/* 0x0001		*/
  dev->type_trans	= dosnet_eth_type_trans;
  dev->rebuild_header	= eth_rebuild_header;
  dev->set_multicast_list = set_multicast_list ;
  dev->open		= dosnet_open;
  dev->stop		= dosnet_close;

  /* New-style flags. */
  dev->flags		= IFF_BROADCAST;
  dev->family		= AF_INET;
#ifdef CONFIG_INET    
  dev->pa_addr		= 0; 
  dev->pa_brdaddr	= 0;
  dev->pa_mask		= 0;
  dev->pa_alen		= 0;
#endif  
  dev->priv = kmalloc(sizeof(struct enet_statistics), GFP_KERNEL);
  memset(dev->priv, 0, sizeof(struct enet_statistics));
  dev->get_stats = get_stats;

  return(0);
};




/*
 * Local variables:
 *  compile-command: "gcc -D__KERNEL__ -Wall -Wstrict-prototypes -O6 -fomit-frame-pointer  -m486 -c -o 3c501.o 3c501.c"
 *  kept-new-versions: 5
 * End:
 */

#ifdef MODULE
char kernel_version[] = UTS_RELEASE;
static char *devicename="dsn0";
static struct device *dev_dosnet = NULL;
/*  {	"      " , 
		0, 0, 0, 0,
	 	0x280, 5,
	 	0, 0, 0, NULL, &dosnet_init };  */
	
int
init_module(void)
{
	dev_dosnet = (struct device *)kmalloc(sizeof(struct device), GFP_KERNEL);
        memset(dev_dosnet, 0, sizeof(struct device));
        dev_dosnet->name=devicename;
        dev_dosnet->init=&dosnet_init;
        dev_dosnet->next=(struct device *)NULL;


	if (register_netdev(dev_dosnet) != 0)
		return -EIO;
	return 0;
}

void
cleanup_module(void)
{
	if (MOD_IN_USE)
		printk("dosnet: device busy, remove delayed\n");
	else
	{
		unregister_netdev(dev_dosnet);
	}
}
#endif /* MODULE */

/*
 *	Perform IPX Get Local Target - resolve a network address to
 *              its immediate address (and add the route to the local
 *              route table if necessary)
 *
 *
 */

#include "ipx.h"
#ifdef IPX
#include <features.h>
#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <net/route.h>
#if defined (__GLIBC__) && __GLIBC__ >= 2
  #include<netipx/ipx.h>
#else
  #include <linux/sockios.h>
  #include <linux/ipx.h>
#endif
#include <netinet/in.h>
#include <errno.h>

#include "emu.h"

#define FALSE   0
#define TRUE    1
#define TIMEOUT 250000

typedef struct
{
	short Operation __attribute__ ((packed));
	unsigned long Network __attribute__ ((packed));
        short Hops __attribute__ ((packed));
        short Ticks __attribute__ ((packed));
} RipPacket_t;
                                   
/* targetNet and network should be passed in in network order */
int AddRoute( unsigned long targetNet, unsigned network,
        unsigned char node[] )
{
	PRIV_SAVE_AREA
	struct rtentry rt;
	struct sockaddr_ipx	*st = (struct sockaddr_ipx *)&rt.rt_dst;
	struct sockaddr_ipx	*sr = (struct sockaddr_ipx *)&rt.rt_gateway;
	int sock;
	int i;
	
	rt.rt_flags = RTF_GATEWAY;
	st->sipx_network = targetNet;
        sr->sipx_network = network;
	for( i=0; i<6; i++ ) {
		sr->sipx_node[i] = node[i];
	}	
	sr->sipx_family = st->sipx_family = AF_IPX;
	
	enter_priv_on();
	sock=socket(AF_IPX,SOCK_DGRAM,PF_IPX);
	leave_priv_setting();
	if(sock==-1) {
		return( -1 );
	}
	
	enter_priv_on();
	if(ioctl(sock,SIOCADDRT,(void *)&rt) < 0) {
                if( errno != EEXIST ) {
			leave_priv_setting();
                        close( sock );
                        return( -2 );
                }
	}
        close( sock );
	leave_priv_setting();
        return(0);
}

/* network must be passed in in network order (that is, high-low) */
/* returns 0 on success, less than 0 on failure */
int IPXGetLocalTarget( unsigned long network, int *hops, int *ticks )
{
	PRIV_SAVE_AREA
 	int sock;
	struct sockaddr_ipx ipxs;
	int opt=1;
	static RipPacket_t RipRequest;
	static RipPacket_t RipResponse;
        int size, sz;
        struct timeval timeout;
        int retries;
        fd_set  fds;
        int done, retCode, selrtn;

        retCode = -1;	
	enter_priv_on();
	sock=socket(AF_IPX,SOCK_DGRAM,PF_IPX);
	leave_priv_setting();
	if(sock==-1)
	{
		goto GLTExit;
	}
	
	/* Socket debugging */
	enter_priv_on();
	if(setsockopt(sock,SOL_SOCKET,SO_DEBUG,&opt,sizeof(opt))==-1)
	{
		leave_priv_setting();
		goto CloseGLTExit;
	}
	
	/* Permit broadcast output */
	if(setsockopt(sock,SOL_SOCKET,SO_BROADCAST, &opt,sizeof(opt))==-1)
	{
		leave_priv_setting();
		goto CloseGLTExit;
	}
	
	/* This is a special for IPX sockets that allows the superuser
	   to change the IPX type field of a socket */
	   
	opt=4;		/* Remember no htons! - its a byte */
	
	if(setsockopt(sock,SOL_SOCKET,IPX_TYPE,&opt,sizeof(opt))==-1)
	{
		leave_priv_setting();
		goto CloseGLTExit;
	}
	
	ipxs.sipx_family=AF_IPX;
	ipxs.sipx_network=htonl(0);
	ipxs.sipx_port=htons(0);
	
	if(bind(sock,(struct sockaddr *)&ipxs,sizeof(ipxs))==-1)
	{
		leave_priv_setting();
		goto CloseGLTExit;
	}
	leave_priv_setting();

        /* prepare destination for send, broadcast to local net */
	ipxs.sipx_port=htons(0x453);
	memset(ipxs.sipx_node,0xff,6);
	ipxs.sipx_type = 4;
        
        /* prepare packet data for RIP request */        
        RipRequest.Operation = htons(1);        /* RIP Request */
        RipRequest.Network = network;
        RipRequest.Hops = -1;
        RipRequest.Ticks = -1;
	
        /* get ready for multiple sends and receives */
        /* we will use a select to wait for the response */
        retries = 5;
        done = FALSE;
        retCode = -1;
        
        /* loop here sending RIP requests and trying to get a RIP response */
        while( !done ) {
		enter_priv_on();
        	if(sendto(sock,(void *)&RipRequest,sizeof(RipRequest),0,
                        (struct sockaddr *)&ipxs,sizeof(ipxs))==-1)
	        {
			leave_priv_setting();
                        retCode = -2;
        		goto CloseGLTExit;
	        }
		leave_priv_setting();
                
                timeout.tv_sec = 0;
                timeout.tv_usec = TIMEOUT;
                FD_ZERO( &fds );
                FD_SET( sock, &fds);
RepeatSelect:
                selrtn = select( 255, &fds, NULL, NULL, &timeout );
                if( selrtn == -1 ) {
                        if( errno != EINTR ) {
                                done = TRUE;
                                retCode = -3;
                                break;
                        }
                        goto RepeatSelect;
                }
                if( selrtn==0 ) {
                        retries--;
                        if( retries==0 ) {
                                done = TRUE;
                                retCode = -4;
                        }
                        continue;
                }
		if (FD_ISSET(sock, &fds)) {
			sz = sizeof(ipxs);
			enter_priv_on();
        		size=recvfrom(sock,(char *)&RipResponse,
                                sizeof(RipResponse),0,
                                (struct sockaddr *)&ipxs,&sz);
			leave_priv_setting();
        		if(size==-1 || size < sizeof (RipResponse) ||
                                RipResponse.Operation != htons(2) ) {
                                done = TRUE;
                                retCode = -5;
	        	} else {
                                done = TRUE;
                                retCode = 0;
                        }
                }
        }
        
        if( retCode==0 ) {
                n_printf("IPX: Received RIP information for network %08lx\n",
                        (unsigned long int)htonl(network));
        
                *hops = htons(RipResponse.Hops);
                *ticks = htons(RipResponse.Ticks);
                retCode = AddRoute( network, ipxs.sipx_network,
                        ipxs.sipx_node );
                if( retCode < 0 ) {
                        n_printf("IPX: Failure %d adding route <%08lx through %08lx:%02x%02x%02x%02x%02x%02x>\n",
                                retCode,
                                (unsigned long int)htonl(network), (unsigned long int)htonl(ipxs.sipx_network),
                                ipxs.sipx_node[0],ipxs.sipx_node[1],ipxs.sipx_node[2],
                                ipxs.sipx_node[3],ipxs.sipx_node[4],ipxs.sipx_node[5] );
                } else {
                        n_printf("IPX: Success adding route <%08lx through %08lx:%02x%02x%02x%02x%02x%02x>\n",
                                (unsigned long int)htonl(network), (unsigned long int)htonl(ipxs.sipx_network),
                                ipxs.sipx_node[0],ipxs.sipx_node[1],ipxs.sipx_node[2],
                                ipxs.sipx_node[3],ipxs.sipx_node[4],ipxs.sipx_node[5] );
                }
        } else {
                printf("IPX: Error %d in GetLocalTarget main packet send/receive loop\n", retCode);
        }
CloseGLTExit:
	enter_priv_on();
        close( sock );
	leave_priv_setting();
GLTExit:
        return( retCode );
}
                                   

#endif


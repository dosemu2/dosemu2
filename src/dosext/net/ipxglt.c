/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 *	Perform IPX Get Local Target - resolve a network address to
 *              its immediate address (and add the route to the local
 *              route table if necessary)
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <net/route.h>
#include "ipx_wrp.h"
#include <netinet/in.h>
#include <errno.h>

#include "emu.h"
#include "utilities.h"
#include "ipx.h"
#ifdef IPX

#define FALSE   0
#define TRUE    1
#define TIMEOUT 250000
#define TRUST_RTABLE 1

typedef struct
{
	short Operation;
	unsigned long Network;
        short Hops;
        short Ticks;
} __attribute__((packed)) RipPacket_t;

/* targetNet and network should be passed in in network order */
static int AddRoute( unsigned long targetNet, unsigned network,
                     unsigned char node[] )
{
	struct rtentry rt;
	struct sockaddr_ipx st, sr;
	int sock;

	rt.rt_flags = RTF_GATEWAY;
	st.sipx_network = targetNet;
        sr.sipx_network = network;
	memcpy(sr.sipx_node, node, IPX_NODE_LEN);
	sr.sipx_family = st.sipx_family = AF_IPX;
	memcpy(&rt.rt_dst, &st, sizeof(st));
	memcpy(&rt.rt_gateway, &sr, sizeof(sr));

	sock=socket(AF_IPX,SOCK_DGRAM,PF_IPX);
	if(sock==-1) {
		return( -1 );
	}

	if(ioctl(sock,SIOCADDRT,(void *)&rt) < 0) {
                if( errno != EEXIST ) {
                        close( sock );
                        return( -2 );
                }
	}
        close( sock );
        return(0);
}

static int CheckRouteExist(unsigned long targetNet
#if !TRUST_RTABLE
	, unsigned network,
	  unsigned char node[]
#endif
	)
{
	char buf_targ[9], proc_net[9], proc_node[13], *proc_str;
#if !TRUST_RTABLE
	char buf_net[9], buf_node[13];
#endif
	sprintf(buf_targ, "%08lX", (unsigned long)htonl(targetNet));
#if !TRUST_RTABLE
	sprintf(buf_net, "%08lX", (unsigned long)htonl(network));
	sprintf(buf_node, "%02X%02X%02X%02X%02X%02X", node[0], node[1],
                     node[2], node[3], node[4], node[5]);
#endif

	if(access("/proc/net/ipx/route",R_OK) == 0)
		open_proc_scan("/proc/net/ipx/route");
	else if(access("/proc/net/ipx_route",R_OK) == 0)
		open_proc_scan("/proc/net/ipx_route");
	else
		return 0;
	proc_str = get_proc_string_by_key(buf_targ);

	if (!proc_str) {
		close_proc_scan();
		return 0;
	}

	sscanf(proc_str, "%s %s", proc_net, proc_node);
	close_proc_scan();

#if !TRUST_RTABLE
	if (strcmp(buf_net, proc_net) || strcmp(buf_node, proc_node)) {
		if(access("/proc/net/ipx/interface",R_OK) == 0)
			open_proc_scan("/proc/net/ipx/interface");
		else if(access("/proc/net/ipx_interface",R_OK) == 0)
			open_proc_scan("/proc/net/ipx_interface");
		else
			return 0;
		proc_str = get_proc_string_by_key(buf_net);
		if (!proc_str) {
			close_proc_scan();
			return 0;
		}
		if (!strstr(proc_str, "Internal")) {
			close_proc_scan();
			return 0;
		}
		close_proc_scan();
		/* fall through */
	}
#endif

	return 1;
}

/* network must be passed in in network order (that is, high-low) */
/* returns 0 on success, less than 0 on failure */
int IPXGetLocalTarget( unsigned long network, int *hops, int *ticks )
{
 	int sock;
	struct sockaddr_ipx ipxs;
	int opt;
	static RipPacket_t RipRequest;
	static RipPacket_t RipResponse;
        int size;
	socklen_t sz;
        struct timeval timeout;
        int retries;
        fd_set  fds;
        int done, retCode, selrtn;

#if TRUST_RTABLE
	if (CheckRouteExist(network)) {
	    n_printf("IPX: Route <%08lx> already exists\n",
		(unsigned long int)htonl(network));
	    return 0;
	}
#endif

        retCode = -1;
	sock=socket(AF_IPX,SOCK_DGRAM,PF_IPX);
	if(sock==-1)
	{
		n_printf("IPX: could not open IPX socket: %s.\n", strerror(errno));
		goto GLTExit;
	}

	/* Permit broadcast output */
	opt = 1;		/* enable */
	if(setsockopt(sock,SOL_SOCKET,SO_BROADCAST, &opt,sizeof(opt))==-1)
	{
		n_printf("IPX: could not set socket option for broadcast: %s.\n", strerror(errno));
		goto CloseGLTExit;
	}

	opt = 1;		/* RIP type */
	if (setsockopt(sock, SOL_IPX, IPX_TYPE, &opt, sizeof(opt)) == -1)
	{
		n_printf("IPX: could not set socket option for type: %s.\n", strerror(errno));
		goto CloseGLTExit;
	}

	memset(&ipxs, 0, sizeof(ipxs));
	ipxs.sipx_family=AF_IPX;
	ipxs.sipx_network=htonl(0);	/* Primary Interface */
	ipxs.sipx_port=htons(0);
	ipxs.sipx_type = 1;		/* RIP */

	if(bind(sock,(struct sockaddr*)&ipxs,sizeof(ipxs))==-1)
	{
		n_printf("IPX: could not bind socket to address: %s\n", strerror(errno));
		goto CloseGLTExit;
	}

        /* prepare destination for send, broadcast to local net */
	ipxs.sipx_port=htons(0x453);	/* RIP port */
	memset(ipxs.sipx_node,0xff,IPX_NODE_LEN);

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
		n_printf("IPX: sending RIP request\n");
        	if(sendto(sock,(void *)&RipRequest,sizeof(RipRequest),0,
                        (struct sockaddr*)&ipxs,sizeof(ipxs))==-1)
	        {
                        retCode = -2;
			n_printf("IPX: sendto() failed: %s\n", strerror(errno));
        		goto CloseGLTExit;
	        }

                timeout.tv_sec = 0;
                timeout.tv_usec = TIMEOUT;
RepeatSelect:
                FD_ZERO( &fds );
                FD_SET( sock, &fds);
                n_printf("IPX: waiting for RIP response, retries=%i\n", retries);
                selrtn = select( sock + 1, &fds, NULL, NULL, &timeout );

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
        		size=recvfrom(sock,(char *)&RipResponse,
                                sizeof(RipResponse),0,
                                (struct sockaddr*)&ipxs,&sz);
			if(size > 0 && ((ipxs.sipx_type != 1 &&
				ipxs.sipx_port != htons(0x453)) ||
				RipResponse.Operation != htons(2))) {
                    		retries--;
                    		if( retries==0 ) {
                            		done = TRUE;
                            		retCode = -5;
                    		}
				n_printf("IPX: Ignoring packet size=%i type=%x port=%x operation=%x retries=%i\n",
					size, ipxs.sipx_type, ntohs(ipxs.sipx_port), ntohs(RipResponse.Operation), retries);
                    		continue;
			}
        		if(size < sizeof (RipResponse)) {
                                done = TRUE;
                                retCode = -6;
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
#if !TRUST_RTABLE
		if (CheckRouteExist(network, ipxs.sipx_network, ipxs.sipx_node)) {
		   n_printf("IPX: Route <%08lx through %08lx:%02x%02x%02x%02x%02x%02x> already exists\n",
                     (unsigned long int)htonl(network), (unsigned long int)htonl(ipxs.sipx_network),
                     ipxs.sipx_node[0],ipxs.sipx_node[1],ipxs.sipx_node[2],
                     ipxs.sipx_node[3],ipxs.sipx_node[4],ipxs.sipx_node[5] );
		   goto CloseGLTExit;
		}
#endif
                retCode = AddRoute( network, ipxs.sipx_network,
                        ipxs.sipx_node );
                if( retCode < 0 ) {
                        error("IPX: Failure adding route <%08lx through %08lx:%02x%02x%02x%02x%02x%02x>\n",
                                (unsigned long int)htonl(network), (unsigned long int)htonl(ipxs.sipx_network),
                                ipxs.sipx_node[0],ipxs.sipx_node[1],ipxs.sipx_node[2],
                                ipxs.sipx_node[3],ipxs.sipx_node[4],ipxs.sipx_node[5] );
			error("IPX: Try manually add this route with ipx_route command\nor use RIP daemon like ipxripd\n");
                } else {
                        n_printf("IPX: Success adding route <%08lx through %08lx:%02x%02x%02x%02x%02x%02x>\n",
                                (unsigned long int)htonl(network), (unsigned long int)htonl(ipxs.sipx_network),
                                ipxs.sipx_node[0],ipxs.sipx_node[1],ipxs.sipx_node[2],
                                ipxs.sipx_node[3],ipxs.sipx_node[4],ipxs.sipx_node[5] );
                }
        } else {
                error("IPX: Error %d in GetLocalTarget main packet send/receive loop\n", retCode);
        }
CloseGLTExit:
        close( sock );
GLTExit:
        return( retCode );
}
#endif

/*
 *	Bind an interface to an IPX network.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h> 
#include <sys/socket.h>
#include <linux/sockios.h>
#include <linux/ipx.h>

void main(int argc,char *argv[])
{
	struct ipx_route_def rt;
	int sock;
	
	if(argc!=4)
	{
		fprintf(stderr,"%s <driver> <network> <bluebook|iee802.3|8022>\n",argv[0]);
		exit(1);
	}
	
	if(sscanf(argv[2],"%lx",&rt.ipx_network)==0)
	{
		fprintf(stderr,"Unable to parse network number '%s'.\n",argv[2]);
		exit(1);
	}

	rt.ipx_network=htonl(rt.ipx_network);	
	rt.ipx_router_network=IPX_ROUTE_NO_ROUTER;
	strncpy(rt.ipx_device,argv[1],16);
	rt.ipx_flags=0;
	if (strcmp(argv[3], "8022") == 0)
		rt.ipx_flags=IPX_RT_8022;
	else if(strcmp(argv[3],"bluebook")==0)
		rt.ipx_flags=IPX_RT_BLUEBOOK;
	else if(strcmp(argv[3],"iee802.3"))
	{
		fprintf(stderr,"Unknown ipx link layer '%s'.\n",argv[3]);
		exit(1);
	}
	
	sock=socket(AF_IPX,SOCK_DGRAM,PF_IPX);
	if(sock==-1)
	{
		perror("socket");
		exit(1);
	}
	
	if(ioctl(sock,SIOCADDRT,(void *)&rt)==-1)
	{
		perror("add route");
		exit(1);
	}
	printf("Network %lx added.\n",rt.ipx_network);
}

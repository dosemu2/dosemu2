/*
     IPX bridging on Linux. 
     Doesn't do intelligent bridging.  
     (Sometimes, it may be faster than intelligent bridging!)

     Authors: Amitay Issac , project engr, Dept. of Aerospace Engg
              Vinod Kulkarni, research scholar, Dept. of Comp. Sc.
                              IIT Bombay.

     This version is placed in Public Domain.
*/


#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>

#define BRIDGE_ONLY

#if !defined(ETH0_ETH1) && !defined(DEVICE0) && !defined(DEVICE1)
#define ETH0_ETH1 
#define DEVICE0		"eth0"
#define DEVICE1		"eth1"
#define  CHANGED_POS        3
#endif

#ifndef   DEVICE0 
#define  DEVICE0		"eth0"
#endif
#ifndef   DEVICE1 
#define  DEVICE1		"eth1"
#endif

#ifdef ETH0_ETH1
#ifndef   CHANGED_POS 
#define   CHANGED_POS 3 
#endif
#endif


unsigned short len_device0, len_device1;

#define MAX_PACK_LEN   20000  /* Sufficient for ethernet packets. */
#define ETHER_SIZE     14
struct ether_hdr
{
  u_char dst_mac[6];
  u_char src_mac[6];
  u_char pkt_type[2];  
};

#define	max(i,j)	(((i)>(j)) ? (i) : (j))

int promisc_mode(char *, int);
void close_all(int);

union {
  u_char full_packet[MAX_PACK_LEN];
  struct ether_hdr ether_header;
  struct eth_address_index  {
       char da, db, dc, dd;   /* Destination address */
       unsigned short int di;  /* size of short int = 4. */
       char sa, sb, sc, sd;   /* Source address */
       unsigned short int si;  /* size of short int = 4. */
  } index;
} a ;

union {
     unsigned short int i;
     u_char b[2];      /* If i=0xab12, b[0]=12 and b[1]=ab. */
} src_index, dst_index;

int packet_length;
int max_fd, fd, fd1, fd2, fd3, fd4;


void main(int argc, char *argv[])
{
fd_set fdset;
struct sockaddr from, to;
int  from_len;
unsigned char from_device_ch, to_device_ch;

#ifdef ETH0_ETH1
static unsigned char ascii_set[256];
#endif

	/* Set the exception handling */
	signal(SIGINT, close_all);
	signal(SIGKILL, close_all);


        len_device0=strlen(DEVICE0);
        len_device1=strlen(DEVICE1);

#ifdef ETH0_ETH1
        ascii_set[ (u_char)DEVICE1[CHANGED_POS] ] = DEVICE0[CHANGED_POS]; 
        ascii_set[ (u_char)DEVICE0[CHANGED_POS] ] = DEVICE1[CHANGED_POS]; 
#endif
        fprintf(stderr,"Opening sockets ...\n");
	/* Open a Socket for reading *all* IPX packets */
	fd1 = socket(AF_INET, SOCK_PACKET, htons(ETH_P_IPX));
	if(fd1 < 0) {
		fprintf(stderr, "Error in opening generic packet socket.\n");
		exit(0);
	}
	fd2 = socket(AF_INET, SOCK_PACKET, htons(ETH_P_802_3));
	if(fd2 < 0) {
		fprintf(stderr, "Error in opening generic packet socket.\n");
		exit(0);
	}
	fd3 = socket(AF_INET, SOCK_PACKET, htons(ETH_P_802_2));
	if(fd3 < 0) {
		fprintf(stderr, "Error in opening generic packet socket.\n");
		exit(0);
	}
	fd4 = socket(AF_INET, SOCK_PACKET, htons(ETH_P_SNAP));
	if(fd4 < 0) {
		fprintf(stderr, "Error in opening generic packet socket.\n");
		exit(0);
	}

        fprintf(stderr,"Setting promiscuos mode ...\n");
	if(promisc_mode(DEVICE0, 1) < 0) {
		fprintf(stderr, "Can't set device 0 to promiscuous mode.\n");
		exit(0);
	}

	if(promisc_mode(DEVICE1, 1) < 0) {
		fprintf(stderr, "Can't set device 1 to promiscuous mode.\n");
		exit(0);
	}

	max_fd = max(fd1, fd2);
	max_fd = max(fd3, max_fd);
	max_fd = max(fd4, max_fd) + 1;

	from.sa_family = AF_INET;  
	to.sa_family = AF_INET;  

#ifdef ETH0_ETH1
        strcpy(from.sa_data , DEVICE0) ;  
        strcpy(to.sa_data , DEVICE1) ; 
        /* This is Required because we only change particular characters 
           from these array variables. 
        */
#endif
        fprintf(stderr, "Now running as daemon.\n");
	if(fork() != 0) exit(0); 

	for(;;) {
		FD_ZERO(&fdset);
		FD_SET(fd1, &fdset);
		FD_SET(fd2, &fdset);
		FD_SET(fd3, &fdset);
		FD_SET(fd4, &fdset);

		select(max_fd, &fdset, (fd_set *)NULL, (fd_set *)NULL, 
				(struct timeval *)NULL);
		
		if(FD_ISSET(fd1, &fdset) == 1) fd = fd1;
		else if(FD_ISSET(fd2, &fdset) == 1) fd = fd2;
		else if(FD_ISSET(fd3, &fdset) == 1) fd = fd3;
		else if(FD_ISSET(fd4, &fdset) == 1) fd = fd4;
		else { 
                       fprintf(stderr, "*** Unknown fd in select ***\n");
                       continue;
                }

		packet_length = recvfrom(fd, a.full_packet, MAX_PACK_LEN, 0, &from, &from_len);
		if(packet_length <= 0) continue;

#ifdef LDEBUG
		fprintf(stderr, "device = %s, ether_len = %.4x, type = %4.4x\n", 
				from.sa_data, packet_length, a.ether_header.pkt_type);
#endif

#ifdef ETH0_ETH1
                from_device_ch = from.sa_data[ CHANGED_POS ];
#else
                from_device_ch = 
                      (memcmp(&from.sa_data[0], DEVICE0, len_device0))? '1':'0';
#endif

#ifdef ETH0_ETH1
                to.sa_data[CHANGED_POS] = ascii_set[ from_device_ch ];
#else
                strcpy(to.sa_data, (from_device_ch=='0')? DEVICE1: DEVICE0 );
#endif                
		fprintf(stderr, "to-device = %s\n", to.sa_data);
	        if(sendto(fd, a.full_packet, packet_length, 0, &to, sizeof(to)) < 0) 
		          perror("sendto");

	} /* for loop. */
}


int promisc_mode(char *device, int flag)
{
struct ifreq if_data;

	strcpy(if_data.ifr_name, device);
	if(ioctl(fd1, SIOCGIFFLAGS, &if_data) < 0) {
		fprintf(stderr, "Cannot get the flags for %s\n", device);
		return -1;
	}

	if(flag) {
		if_data.ifr_flags |= IFF_PROMISC;
	}
	else {
		if_data.ifr_flags &= IFF_PROMISC;
		if_data.ifr_flags ^= IFF_PROMISC;
	}

	if(ioctl(fd1, SIOCSIFFLAGS, &if_data) < 0) {
		fprintf(stderr, "Cannot set promiscous mode for %s\n", device);
		return -1;
	}

	return 1;
}

#ifdef DEBUG
void debug(void)
{
int set;
int ipx_len;
u_char *pkt_type;
unsigned int type, frame_type;
u_char *ptr;
               /* This is a long DEBUG code to see if particular packets are getting
                          accepted. */
		pkt_type = a.ether_header.pkt_type;
		type = pkt_type[0] << 8 | pkt_type[1];

		ptr = (u_char *) (a.full_packet + ETHER_SIZE); 
		/* Here we check the frame type and get the IPX related packets */
		if(type >= 1536) frame_type = type;
		else {
			if(ptr[0] == 0xFF && ptr[1] == 0xFF) frame_type = ETH_P_802_3;
			if(ptr[0] == 0xAA && ptr[1] == 0xAA) frame_type = ETH_P_SNAP;
			else frame_type = ETH_P_802_2;
		}
		ipx_len = ptr[2] << 8 | ptr[3];
		set = 0;
		switch(frame_type) {
			case ETH_P_IPX :
				fprintf(stderr, "Got IPX over DIX packet\n"); 
				set = 1;
				break;
			case ETH_P_802_2 :
				fprintf(stderr, "Got Bluebook packet\n"); 
				set = 1;
				break;
			case ETH_P_802_3 :
				fprintf(stderr, "Got IPX over DIX\n"); 
				set = 1;
				break;
			case ETH_P_SNAP :
				fprintf(stderr, "Got Etherenet II SNAP\n"); 
				set = 1;
				break;
			default :
				break;
		}
		if(! set) return;
		fprintf(stderr, "device = %s, ether_len = %.4x, ipx_len = %.4x, type = %.4x\n", 
				a.full_packet, packet_length, ipx_len, type);

    /* That was a long DEBUG code to see if particular packets are getting
       accepted. */
}
#endif

void close_all(int a)
{
	close(fd1);
	close(fd2);
}

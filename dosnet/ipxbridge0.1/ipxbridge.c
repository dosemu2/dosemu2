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
#include "a.h"

#define DEVICE1		"eth0"
#define DEVICE2		"dsn0"

#define	max(i,j)	(((i)>(j)) ? (i) : (j))

int promisc_mode(char *, int);
void close_all(int);

u_char *full_packet;
int new_len;
int max_fd, fd, fd1, fd2, fd3, fd4;

void main(int argc, char *argv[])
{
fd_set fdset;
struct sockaddr from, to;
int len, from_len;
u_char *pkt_type;
struct ether_hdr *ether_header;
int ipx_len;
u_char *ptr;
unsigned int type, frame_type;
int set;

	/* Set the exception handling */
	signal(SIGINT, close_all);
	signal(SIGKILL, close_all);

	/* I am running as a daemon */
	if(fork() != 0) exit(0); 

	/* Open a Socket for reading *all* OR *IPX* packets */
	fd1 = socket(AF_INET, SOCK_PACKET, htons(ETH_P_802_2));
	if(fd1 < 0) {
		fprintf(stderr, "Error in opening generic packet socket.\n");
		exit(0);
	}
	fd2 = socket(AF_INET, SOCK_PACKET, htons(ETH_P_IPX));
	if(fd2 < 0) {
		fprintf(stderr, "Error in opening generic packet socket.\n");
		exit(0);
	}
	fd3 = socket(AF_INET, SOCK_PACKET, htons(ETH_P_802_3));
	if(fd3 < 0) {
		fprintf(stderr, "Error in opening generic packet socket.\n");
		exit(0);
	}
	fd4 = socket(AF_INET, SOCK_PACKET, htons(ETH_P_SNAP));
	if(fd4 < 0) {
		fprintf(stderr, "Error in opening generic packet socket.\n");
		exit(0);
	}

	if(promisc_mode(DEVICE1, 1) < 0) {
		exit(0);
	}

	if(promisc_mode(DEVICE2, 1) < 0) {
		exit(0);
	}

	if((full_packet = (char *)malloc(MAX_PACK_LEN)) == NULL) {
		fprintf(stderr, "Cannot allocate memory.\n");
		exit(0);
	}

	max_fd = max(fd1, fd2);
	max_fd = max(fd3, max_fd);
	max_fd = max(fd4, max_fd) + 1;

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
		else fprintf(stderr, "*** Unknown fd here ***\n");

		len = recvfrom(fd, full_packet, MAX_PACK_LEN, 0, &from, &from_len);
		if(len <= 0) continue;

		/* Get the ethernet header */
		ether_header = (struct ether_hdr *) full_packet;
		pkt_type = ether_header->pkt_type;
		type = pkt_type[0] << 8 | pkt_type[1];

		/* Here we check the frame type and get the IPX related packets */
		if(type >= 1536) frame_type = type;
		else {
			ptr = (u_char *) (full_packet + ETHER_SIZE);
			if(ptr[0] == 0xFF && ptr[1] == 0xFF) frame_type = ETH_P_802_3;
			if(ptr[0] == 0xAA && ptr[1] == 0xAA) frame_type = ETH_P_SNAP;
			else frame_type = ETH_P_802_2;
		}

		ipx_len = ptr[2] << 8 | ptr[3];

		set = 0;

		switch(frame_type) {
			case ETH_P_IPX :
#ifdef DEBUG
				fprintf(stderr, "Got IPX over DIX packet\n"); 
#endif
				set = 1;
				break;
			case ETH_P_802_2 :
#ifdef DEBUG
				fprintf(stderr, "Got Bluebook packet\n"); 
#endif
				set = 1;
				break;
			case ETH_P_802_3 :
#ifdef DEBUG
				fprintf(stderr, "Got IPX over DIX\n"); 
#endif
				set = 1;
				break;
			case ETH_P_SNAP :
#ifdef DEBUG
				fprintf(stderr, "Got Etherenet II SNAP\n"); 
#endif
				set = 1;
				break;
			default :
				break;
		}
			
		if(! set) continue;

#ifdef DEBUG
		fprintf(stderr, "device = %s, ether_len = %.4x, ipx_len = %.4x, type = %.4x\n", 
				from.sa_data, len, ipx_len, type);
#endif

		if(strncmp(from.sa_data, DEVICE1, 4) == 0) 
			strcpy(to.sa_data, DEVICE2);
		else if(strncmp(from.sa_data, DEVICE2, 4) == 0) 
			strcpy(to.sa_data, DEVICE1);
		else
			continue;

		to.sa_family = AF_INET;

	/* sendto function sends the packet *as is* to the network interface
	   Only have to check if it adds the any of the MAC components to 
	   the packet or not */

		if(sendto(fd, full_packet, len, 0, &to, sizeof(to)) < 0) 
		   perror("sendto");

	/* After sending the packet we are waiting for some time before we
	   start polling again. */
	}
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

void close_all(int a)
{
	free(full_packet);
	close(fd1);
	close(fd2);
}


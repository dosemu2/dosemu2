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

#define LDEBUG

#define DEVICE1		"eth0"
#define DEVICE2		"dsn0"

#define MAX_PACK_LEN   66560
#define ETHER_SIZE     14
struct ether_hdr
{
  u_char dst_mac[6];
  u_char src_mac[6];
  u_char pkt_type[2];  /* 0x8137 */
};

#define	max(i,j)	(((i)>(j)) ? (i) : (j))

int promisc_mode(char *, int);
void close_all(int);

u_char *full_packet;
int new_len;
int max_fd, fd, fd1, fd2, fd3, fd4;

/* Hash table:  Ethernet address is aa:bb:cc:dd:ee:ff. 

   Hash table is of 64K. It is addressed using only 13 bits. So each addressed
   location has total of 6 bytes for its usage. Of these, we store
   the ethernet address in 5 bytes. (1 byte [+3bits] is matched when 
   looking up). If it doesn't match, flood. 
   Lookup Address = ee:(ff << 3).

   But this memory should not be swapped out any time! This can be ensured
   by setting t bit (I think). 

   After every n seconds of time, it should set all elements of hash table 
   to zero so as to dynamically "learn" . 

   Broadcast address: LSB of 2nd byte of destination should equal 1.
   First 3 bytes are meant to identify manufacturer ID.

*/
char hash_table[ 0xFFFF ] ;  /* Can't we avoid it? */
  /* Assumption: Linux initialises all variables to zero ... 
     We don't want to calloc. Helps the compiler to optimise.
   */
struct eth_address_index  {
       char da, db, dc, dd;   /* Destination address */
       unsigned short int di;  /* size of short int = 4. */
       char sa, sb, sc, sd;   /* Source address */
       unsigned short int si;  /* size of short int = 4. */
} ;
union {
  unsigned short int i;
  u_char a, b;
} src_index, dst_index;

void main(int argc, char *argv[])
{
fd_set fdset;
struct sockaddr from, to;
int len, from_len;
unsigned short dummy123;  /* for pkt_type! don't use it ..*/
u_char *pkt_type;
u_char *ptr;
struct ether_hdr *ether_header;
int ipx_len;
unsigned int type, frame_type;
int set;
static char ascii01[2]= {'1','0'};

	/* Set the exception handling */
	signal(SIGINT, close_all);
	signal(SIGKILL, close_all);


	/* I am running as a daemon */
	/* if(fork() != 0) exit(0);  */


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


	from.sa_family = AF_INET;  
        strcpy(from.sa_data , "eth0") ; 
	to.sa_family = AF_INET;  
        strcpy(to.sa_data , "eth0") ; 

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
#ifdef LDEBUG
		fprintf(stderr, "device = %s, ether_len = %.4x, ipx_len = %.4x, type = %.4x\n", 
				from.sa_data, len, ipx_len, type);
#endif

                /* Intellingent bridging now ... */

                /*=============Insert source address in hash table=========*/
                /* Use its source address to index the hash table and
                   store its interface in hash table. */
                src_index.i = ((struct eth_address_index *)ether_header)->si;  
                        /* Last two bytes are in this variable now. */
                src_index.b <<= 3; /* Shift left last byte by 3 bits. */  
                /* Long int is 4 bytes. This statement should move first 4
                   bytes of source ethernet address into hash table. */
                (long int)hash_table[ src_index.i ] =  
                       *(long int *)((char *)ether_header + 6) ;
                /* Next, last byte of destination address should be moved into
                   the hash table. */
                (long int)hash_table[ src_index.i + 4 ] =  *(char *)((char *)ether_header + 11) ;
                /* Next, move the interface number into the next position. 
                   Note: this is ascii '0' or '1'. 
                */
                hash_table[ src_index.i + 5 ] = from.sa_data[3] ;
                /*=======================================================*/


                /*=============Handle broadcast addresses==================*/
                /* Check if it is broadcast destination. */
                if ( (((struct eth_address_index *)ether_header)->db & 0x01) == 0x01 )
                { 
                  /* Make eth0 -> eth1, and eth1 -> eth0. */
                  /* ASSUMPTION: Only eth0 and eth1 are participating! */
                  to.sa_data[3] = ascii01[ from.sa_data[3] - '0' ];
                
                  /* Now broadcast it on other segment. */
		  if(sendto(fd, full_packet, len, 0, &to, sizeof(to)) < 0) 
		          perror("sendto");
                  continue;
                }
                /*=========================================================*/

                /*=============Handle non-broadcast addresses==============*/
                /* No it is not broadcast. Get the destination interface 
                   from the hash tables. */ 

                /* Use its destination address to index the hash table */ 
                dst_index.i = ((struct eth_address_index *)ether_header)->di;  
                       /* Last two bytes are in this variable now. */
                dst_index.b <<= 3; /* Shift left last byte by 3 bits. */  
                /* Long int is 4 bytes. This statement should compare first 4
                   bytes of destination ethernet address with the same of
                   entry in hash table. -- followed by  comparing the last 
                   byte of destination address  */ 
                if (  ( (long int)hash_table[ dst_index.i ] == 
                                *(long int *)((char *)ether_header) ) &&
                      ( (long int)hash_table[ dst_index.i + 4 ] ==  
                                *(char *)((char *)ether_header + 5) ) ) {
                     to.sa_data[3] = hash_table[ src_index.i + 5 ] ;
                     /* This packet need not be sent if both interfaces 
                        are same */
                     if (to.sa_data[3] == from.sa_data[3])  {
#ifdef LDEBUG
                            fprintf(stderr,"Hey! a match at last! \n");
#endif
                            continue;
                     }
                } 
                else{ 
                     to.sa_data[3] = ascii01[ from.sa_data[3] - '0' ];
                }
#ifdef LDEBUG
                fprintf(stderr,"No match: sending on interface %s\n",to.sa_data);
#endif
		if(sendto(fd, full_packet, len, 0, &to, sizeof(to)) < 0) 
		          perror("sendto");

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

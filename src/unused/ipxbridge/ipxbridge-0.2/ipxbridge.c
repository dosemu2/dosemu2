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

int new_len;
int max_fd, fd, fd1, fd2, fd3, fd4;

/* Hash table:  Ethernet address is aa:bb:cc:dd:ee:ff. 

   Hash table is of 64K. So each byte of it can be addressed by 16-bit
   number.  However, we address using only 15 bits, with last bit 
   set to zero. So an addressible  "block" would be of 2 bytes.
   We however store the interface no. and 5 bytes of the ethernet address
   in 6 bytes. (It could be written over by next address in sequence, but
   we guarantee that if there is a match between 5 bytes, then 6th one
   will be proper interface.)
   Given ethernet address of aa:bb:cc:dd:ee:ff, We can use any two
   bytes for hashing. Requirement: Most random but pairs of them shouldn't
   occur.  Usually ee:ff are most random since people buy cards in bulk
   from same manufacturer. Further, we use index= ff * 256 + ee and
   shift left it by 1. i.e. ee gets shifted left. 
   So addresses of the form ee:ff,(ee+1):ff , (ee+2):ff destroy each
   others' ethernet addresses.  

   But this memory should not be swapped out any time! This can be ensured
   by setting 't' bit using chmod. (I think). 

   After every n seconds of time, it should set all elements of hash table 
   to zero so as to dynamically "learn" . 

   Broadcast address: LSB of 2nd byte of destination should equal 1.
   First 3 bytes are meant to identify manufacturer ID.

*/
u_char hash_table[ (unsigned short int)0xFFFF ] ;  /* Can't we avoid it? */
  /* Assumption: Linux initialises all variables to zero ... 
     We don't want to calloc. Helps the compiler to optimise.
   */

void main(int argc, char *argv[])
{
fd_set fdset;
struct sockaddr from, to;
int len, from_len;
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
		fprintf(stderr, "Can't set device 1 to promiscuous mode.\n");
		exit(0);
	}

	if(promisc_mode(DEVICE1, 1) < 0) {
		fprintf(stderr, "Can't set device 1 to promiscuous mode.\n");
		exit(0);
	}

        fprintf(stderr,"Setting hash memory to all zero ...\n");
        memset(hash_table, '\0', 0xffff ); 

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

		len = recvfrom(fd, a.full_packet, MAX_PACK_LEN, 0, &from, &from_len);
		if(len <= 0) continue;


#ifdef ETH0_ETH1
                from_device_ch = from.sa_data[ CHANGED_POS ];
#else
                from_device_ch = 
                      (memcmp(&from.sa_data[0], DEVICE0, len_device0))? '1':'0';
#endif

                /* check if it is broadcast packet. */
                if (  (a.index.db & 0x01) == 0x01 )
                     to_device_ch=0;
                else {
                     /* No it is not broadcast. Get the destination interface 
                        from the hash tables. Use the destination address to 
                        index the hash table */ 

                     dst_index.i = a.index.di;  
                           /* Last two bytes are in this variable now. */
                     dst_index.b[0] <<= 1; /* Shift left LSB by 1 bit. */  

                     /* This may be invalid if next text fails! ... */
                     to_device_ch = hash_table[ dst_index.i ];
                }
#ifdef LDEBUG
                fprintf(stderr,"From=%c  To:%c \n", from_device_ch, to_device_ch );
#endif
                /* Determine if it can be filtered. */ 
                /* Long int is 4 bytes. This statement should compare first 4
                   bytes of destination ethernet address with the same of
                   entry in hash table. -- followed by  comparing the last 
                   byte of destination address  
                   If the to_device doesn't match, no need to go further
                   in the test! Or else, compare 5 bytes ...
                */ 
                if (    (from_device_ch == to_device_ch)  
                     && !memcmp(&hash_table[ dst_index.i + 1 ], &a.ether_header.dst_mac[0], 5)) 
                { 
#ifdef LDEBUG
                        fprintf(stderr,"filtered\n");
#endif
                        continue;
                }

                /* if it comes here, it couldn't be filtered.
                   So choose the other interface. */  
#ifdef ETH0_ETH1
                to.sa_data[CHANGED_POS] = ascii_set[ from_device_ch ];
#else
                strcpy(to.sa_data, (from_device_ch=='0')? DEVICE1: DEVICE0 );
#endif                
	        if(sendto(fd, a.full_packet, len, 0, &to, sizeof(to)) < 0) 
		          perror("sendto");

                /*=============Insert source address in hash table=========*/

                /* Use last two bytes of source address to index the hash 
                   table and store its interface in hash table. */
                src_index.i = a.index.si;  

                /* i[0]=5th_byte=b[0], i[1]=6th_byte=b[1] of ethernet address. 
                   0th byte of integer is its LSB 8 bits. 
                   So index= 6th_byte * 256 + 5th_byte.
                */

                src_index.b[0] <<= 1; /* Shift left LSB byte by 1 bit. */  

                /* Check if it is already in hash table. For this do only
                   single char compare of 0th byte, which gives the interface. 
                   If this byte is 0 (null), it is empty location. If some
                   other address has occupied it from other interface, overwrite
                   it.
                */
                if( hash_table[ src_index.i ] == '\0' )
                {  
                     memcpy( &hash_table[ src_index.i + 1 ], &a.ether_header.src_mac[0], 5);
                     /* Moved first 5 bytes of source ethernet address into hash 
                        table. 6th byte was used 'as is' to access the hash table,
                        so it neednot be stored.
                      */
                      /* Next, move the interface number into 0'th position. 
                         We use ascii '0' or '1' if the names don't differ
                         in same position. 
                      */
                      hash_table[ src_index.i ] = from_device_ch;
                }  
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
		pkt_type = a.ether_header->pkt_type;
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
		if(! set) continue;
		fprintf(stderr, "device = %s, ether_len = %.4x, ipx_len = %.4x, type = %.4x\n", 
				from.sa_data, len, ipx_len, type);

    /* That was a long DEBUG code to see if particular packets are getting
       accepted. */
}
#endif

void close_all(int a)
{
	close(fd1);
	close(fd2);
}

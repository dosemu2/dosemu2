#include <linux/ipx.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

struct ifreq	id;
char	*progname;

void
usage(void)
{
	fprintf(stderr, "Usage: %s add [-p] device frame_type [net_number]\n\
Usage: %s del device frame_type\n\
Usage: %s check device frame_type\n", progname, progname, progname);
	exit(-1);
}

struct frame_type {
	char	*ft_name;
	unsigned char	ft_val;
}	frame_types[] = {
	{"802.2",	IPX_FRAME_8022},
	{"802.3",	IPX_FRAME_8023},
	{"SNAP",	IPX_FRAME_SNAP},
	{"EtherII",	IPX_FRAME_ETHERII}
};

#define NFTYPES	(sizeof(frame_types)/sizeof(struct frame_type))

int
lookup_frame_type(char *frame)
{
	int	j;

	for (j = 0; (j < NFTYPES) && 
		(strcasecmp(frame_types[j].ft_name, frame)); 
		j++)
		;

	if (j != NFTYPES)
		return j;

	fprintf(stderr, "%s: Frame type must be", progname);
	for (j = 0; j < NFTYPES; j++) {
		fprintf(stderr, "%s%s", 
			(j == NFTYPES-1) ? " or " : " ",
			frame_types[j].ft_name);
	}
	fprintf(stderr, ".\n");
	return -1;
}

int
ipx_add_interface(int argc, char **argv)
{
	struct sockaddr_ipx	*sipx = (struct sockaddr_ipx *)&id.ifr_addr;
	int	s;
	int	result;
	unsigned long	netnum;
	char	errmsg[80];
	int	i, fti;
	char	c;

	sipx->sipx_special = IPX_SPECIAL_NONE;
	sipx->sipx_network = 0L;
	sipx->sipx_type = IPX_FRAME_NONE;
	while ((c = getopt(argc, argv, "p")) > 0) {
		switch (c) {
		case 'p': sipx->sipx_special = IPX_PRIMARY; break;
		}
	}

	if (((i = (argc - optind)) < 2) || (i > 3)) {
		usage();
	}

	for (i = optind; i < argc; i++) {
		switch (i-optind) {
		case 0:	/* Physical Device - Required */
			strcpy(id.ifr_name, argv[i]);
			break;
		case 1: /* Frame Type - Required */
			fti = lookup_frame_type(argv[i]);
			if (fti < 0)
				exit(-1);
			sipx->sipx_type = frame_types[fti].ft_val;
			break;

		case 2: /* Network Number - Optional */
			netnum = strtoul(argv[i], (char **)NULL, 16);
			if (netnum == 0xffffffffL) {
				fprintf(stderr, 
					"%s: Inappropriate network number %08X\n", 
					progname, netnum);
				exit(-1);
			}
			sipx->sipx_network = htonl(netnum);
			break;
		}
	}
			
	s = socket(AF_IPX, SOCK_DGRAM, AF_IPX);
	if (s < 0) {
		sprintf(errmsg, "%s: socket", progname);
		perror(errmsg);
		exit(-1);
	}

	i = 0;
	sipx->sipx_family = AF_IPX;
	sipx->sipx_action = IPX_CRTITF;
	do {
		result = ioctl(s, SIOCSIFADDR, &id);
		i++;
	}	while ((i < 5) && (result < 0) && (errno == EAGAIN));

	if (result == 0) {
		fprintf(stdout, "Interface added.\n");
		exit(0);
	}
		
	switch (errno) {
	case EEXIST:
		fprintf(stderr, "%s: Primary network already selected.\n",
			progname);
		break;
	case EADDRINUSE:
		fprintf(stderr, "%s: Network number (%08X) already in use.\n",
			progname, htonl(sipx->sipx_network));
		break;
	case EPROTONOSUPPORT:
		fprintf(stderr, "%s: Invalid frame type (%s).\n",
			progname, frame_types[fti].ft_name);
		break;
	case ENODEV:
		fprintf(stderr, "%s: No such device (%s).\n", progname,
			id.ifr_name);
		break;
	case ENETDOWN:
		fprintf(stderr, "%s: Requested device (%s) is down.\n", progname,
			id.ifr_name);
		break;
	case EINVAL:
		fprintf(stderr, "%s: Invalid device (%s).\n", progname,
			id.ifr_name);
		break;
	case EAGAIN:
		fprintf(stderr, 
			"%s: Insufficient memory to create interface.\n",
			progname);
		break;
	default:
		sprintf(errmsg, "%s: ioctl", progname);
		perror(errmsg);
		break;
	}
	exit(-1);
}

int
ipx_del_interface(int argc, char **argv)
{
	struct sockaddr_ipx	*sipx = (struct sockaddr_ipx *)&id.ifr_addr;
	int	s;
	int	result;
	char	errmsg[80];
	int	fti;
	
	if (argc != 3) {
		usage();
	}

	sipx->sipx_network = 0L;
	strcpy(id.ifr_name, argv[1]);
	fti = lookup_frame_type(argv[2]);
	if (fti < 0)
		exit(-1);
	sipx->sipx_type = frame_types[fti].ft_val;

	s = socket(AF_IPX, SOCK_DGRAM, AF_IPX);
	if (s < 0) {
		sprintf(errmsg, "%s: socket", progname);
		perror(errmsg);
		exit(-1);
	}
	sipx->sipx_action = IPX_DLTITF;
	sipx->sipx_family = AF_IPX;
	result = ioctl(s, SIOCSIFADDR, &id);
	if (result == 0) {
		fprintf(stdout, "Interface deleted.\n");
		exit(0);
	}

	switch (errno) {
	case EPROTONOSUPPORT:
		fprintf(stderr, "%s: Invalid frame type (%s).\n",
			progname, frame_types[fti].ft_name);
		break;
	case ENODEV:
		fprintf(stderr, "%s: No such device (%s).\n", progname,
			id.ifr_name);
		break;
	case EINVAL:
		fprintf(stderr, "%s: No such IPX interface %s %s.\n", progname,
			id.ifr_name, frame_types[fti].ft_name);
		break;
	default:
		sprintf(errmsg, "%s: ioctl", progname);
		perror(errmsg);
		break;
	}
	exit(-1);
}

int
ipx_check_interface(int argc, char **argv)
{
	struct sockaddr_ipx	*sipx = (struct sockaddr_ipx *)&id.ifr_addr;
	int	s;
	int	result;
	char	errmsg[80];
	int	fti;
	
	if (argc != 3) {
		usage();
	}

	sipx->sipx_network = 0L;
	strcpy(id.ifr_name, argv[1]);
	fti = lookup_frame_type(argv[2]);
	if (fti < 0)
		exit(-1);
	sipx->sipx_type = frame_types[fti].ft_val;

	s = socket(AF_IPX, SOCK_DGRAM, AF_IPX);
	if (s < 0) {
		sprintf(errmsg, "%s: socket", progname);
		perror(errmsg);
		exit(-1);
	}
	sipx->sipx_family = AF_IPX;
	result = ioctl(s, SIOCGIFADDR, &id);
	if (result == 0) {
		printf("IPX Address for (%s, %s) is %08X:%02X%02X%02X%02X%02X%02X.\n",
			argv[1], frame_types[fti].ft_name,
			htonl(sipx->sipx_network), sipx->sipx_node[0],
			sipx->sipx_node[1], sipx->sipx_node[2], 
			sipx->sipx_node[3], sipx->sipx_node[4], 
			sipx->sipx_node[5]);
		exit(0);
	}

	switch (errno) {
	case EPROTONOSUPPORT:
		fprintf(stderr, "%s: Invalid frame type (%s).\n",
			progname, frame_types[fti].ft_name);
		break;
	case ENODEV:
		fprintf(stderr, "%s: No such device (%s).\n", progname,
			id.ifr_name);
		break;
	case EADDRNOTAVAIL:
		fprintf(stderr, "%s: No such IPX interface %s %s.\n", progname,
			id.ifr_name, frame_types[fti].ft_name);
		break;
	default:
		sprintf(errmsg, "%s: ioctl", progname);
		perror(errmsg);
		break;
	}
	exit(-1);
}

int
main(int argc, char **argv)
{
	int	i;

	progname = argv[0];
	if (argc < 2) {
		usage();
		exit(-1);
	}

	if (strncasecmp(argv[1], "add", 3) == 0) {
		for (i = 1; i < (argc-1); i++) 
			argv[i] = argv[i+1];
		ipx_add_interface(argc-1, argv);
	} else if (strncasecmp(argv[1], "del", 3) == 0) {
		for (i = 1; i < (argc-1); i++) 
			argv[i] = argv[i+1];
		ipx_del_interface(argc-1, argv);
	} else if (strncasecmp(argv[1], "check", 5) == 0) {
		for (i = 1; i < (argc-1); i++) 
			argv[i] = argv[i+1];
		ipx_check_interface(argc-1, argv);
	}
	usage();
}

#include <linux/ipx.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

struct ifreq	id;

char	*progname;

void
usage(void)
{
	fprintf(stderr, "Usage: %s add net_number(hex) node(hex)\n\
Usage: %s del\n", progname, progname);
	exit(-1);
}

int
map_char_to_val(char dig)
{
	char	digit = tolower(dig);
	if ((digit >= '0') && (digit <= '9')) {
		return digit - '0';
	} else if ((digit >= 'a') && (digit <= 'f')) {
		return (10 + (digit - 'a'));
	} else {
		return 0;
	}
}

int
ipx_add_internal_net(int argc, char **argv)
{
	struct sockaddr_ipx	*sipx = (struct sockaddr_ipx *)&id.ifr_addr;
	int	s;
	int	result;
	unsigned long	netnum;
	char	errmsg[80];
	int	nodelen;
	char	*node;
	char	tmpnode[13];
	unsigned char	*tout;
	char	*tin;
	int	i;
	
	if (argc != 3) {
		usage();
	}

	netnum = strtoul(argv[1], (char **)NULL, 16);
	if ((netnum == 0L) || (netnum == 0xffffffffL)) {
		fprintf(stderr, "%s: Inappropriate network number %08X\n", 
			progname, netnum);
		exit(-1);
	}

	node = argv[2];
	nodelen = strlen(node);
	if (nodelen > 12) {
		fprintf(stderr, "%s: Node length is too long (> 12).\n", progname);
		exit(-1);
	}
	
	for (i = 0; (i < nodelen) && isxdigit(node[i]); i++) 
		;

	if (i < nodelen) {
		fprintf(stderr, "%s: Invalid value in node, must be hex digits.\n",
			progname);
		exit(-1);
	}
		
	strcpy(tmpnode, "000000000000");
	memcpy(&(tmpnode[12-nodelen]), node, nodelen);
	for (tin = tmpnode, tout = sipx->sipx_node; *tin != '\0'; tin += 2, tout++)  {
		*tout = (unsigned char) map_char_to_val(*tin);
		*tout <<= 4;
		*tout |= (unsigned char) map_char_to_val(*(tin+1));
	}

	if ((memcmp(sipx->sipx_node, "\0\0\0\0\0\0\0\0", IPX_NODE_LEN) == 0) ||
		(memcmp(sipx->sipx_node, "\377\377\377\377\377\377", IPX_NODE_LEN) == 0)){
		fprintf(stderr, "%s: Node is invalid.\n", progname);
		exit(-1);
	}

	sipx->sipx_network = htonl(netnum);
	sipx->sipx_type = IPX_FRAME_NONE;
	sipx->sipx_special = IPX_INTERNAL;
	s = socket(AF_IPX, SOCK_DGRAM, AF_IPX);
	if (s < 0) {
		sprintf(errmsg, "%s: socket", progname);
		perror(errmsg);
		exit(-1);
	}

	sipx->sipx_family = AF_IPX;
	sipx->sipx_action = IPX_CRTITF;
	i = 0;
	do {
		result = ioctl(s, SIOCSIFADDR, &id);
		i++;
	}	while ((i < 5) && (result < 0) && (errno == EAGAIN));

	if (result == 0) {
		fprintf(stdout, "Internal network created.\n");
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
	case EAGAIN:
		fprintf(stderr, 
			"%s: Insufficient memory to create internal net.\n",
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
ipx_del_internal_net(int argc, char **argv)
{
	struct sockaddr_ipx	*sipx = (struct sockaddr_ipx *)&id.ifr_addr;
	int	s;
	int	result;
	char	errmsg[80];
	
	if (argc != 1) {
		usage();
	}

	sipx->sipx_network = 0L;
	sipx->sipx_special = IPX_INTERNAL;
	s = socket(AF_IPX, SOCK_DGRAM, AF_IPX);
	if (s < 0) {
		sprintf(errmsg, "%s: socket", progname);
		perror(errmsg);
		exit(-1);
	}
	sipx->sipx_family = AF_IPX;
	sipx->sipx_action = IPX_DLTITF;
	result = ioctl(s, SIOCSIFADDR, &id);
	if (result == 0) {
		fprintf(stdout, "Internal network deleted.\n");
		exit(0);
	}

	switch (errno) {
	case ENOENT:
		fprintf(stderr, "%s: No internal network configured.\n", progname);
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
		ipx_add_internal_net(argc-1, argv);
	} else if (strncasecmp(argv[1], "del", 3) == 0) {
		for (i = 1; i < (argc-1); i++) 
			argv[i] = argv[i+1];
		ipx_del_internal_net(argc-1, argv);
	}
	usage();
}


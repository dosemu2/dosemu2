#include <linux/ipx.h>
#include <linux/sockios.h>
#include <linux/socket.h>
#include <linux/route.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

struct rtentry	rd;
char	*progname;

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

void
usage(void)
{
	fprintf(stderr, 
	"Usage: %s add network(hex) router_network(hex) router_node(hex)\n\
Usage: %s del network(hex)\n", progname, progname);
	exit(-1);
}

int
ipx_add_route(int argc, char **argv)
{
	/* Router */
	struct sockaddr_ipx	*sr = (struct sockaddr_ipx *)&rd.rt_gateway;
	/* Target */
	struct sockaddr_ipx	*st = (struct sockaddr_ipx *)&rd.rt_dst;
	int	s;
	int	result;
	int	nodelen;
	int	i;
	unsigned long	netnum;
	char	errmsg[80];
	char	*node;
	char	*tin;
	char	tmpnode[13];
	unsigned char	*tout;
	
	if (argc != 4)
		usage();

	/* Network Number */
	netnum = strtoul(argv[1], (char **)NULL, 16);
	if ((netnum == 0xffffffffL) || (netnum == 0L)) {
		fprintf(stderr, "%s: Inappropriate network number %08X\n", 
			progname, netnum);
		exit(-1);
	}
	rd.rt_flags = RTF_GATEWAY;
	st->sipx_network = htonl(netnum);

	/* Router Network Number */
	netnum = strtoul(argv[2], (char **)NULL, 16);
	if ((netnum == 0xffffffffL) || (netnum == 0L)) {
		fprintf(stderr, "%s: Inappropriate network number %08X\n", 
			progname, netnum);
		exit(-1);
	}
	sr->sipx_network = htonl(netnum);
	
	/* Router Node */
	node = argv[3];
	nodelen = strlen(node);
	if (nodelen > 12) {
		fprintf(stderr, "%s: Node length is too long (> 12).\n", 
			progname);
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
	for (tin = tmpnode, tout = sr->sipx_node; *tin != '\0'; tin += 2, tout++)  {
		*tout = (unsigned char) map_char_to_val(*tin);
		*tout <<= 4;
		*tout |= (unsigned char) map_char_to_val(*(tin+1));
	}

	if ((memcmp(sr->sipx_node, "\0\0\0\0\0\0\0\0", IPX_NODE_LEN) == 0) ||
		(memcmp(sr->sipx_node, "\377\377\377\377\377\377", IPX_NODE_LEN) == 0)){
		fprintf(stderr, "%s: Node (%s) is invalid.\n", progname, tmpnode);
		exit(-1);
	}

	s = socket(AF_IPX, SOCK_DGRAM, AF_IPX);
	if (s < 0) {
		sprintf(errmsg, "%s: socket", progname);
		perror(errmsg);
		exit(-1);
	}

	sr->sipx_family = st->sipx_family = AF_IPX;
	i = 0;
	do {
		result = ioctl(s, SIOCADDRT, &rd);
		i++;
	}	while ((i < 5) && (result < 0) && (errno == EAGAIN));

	if (result == 0) {
		fprintf(stdout, "Route added.\n");
		exit(0);
	}

	switch (errno) {
	case ENETUNREACH:
		fprintf(stderr, "%s: Router network (%08X) not reachable.\n",
			progname, htonl(sr->sipx_network));
		break;
	default:
		sprintf(errmsg, "%s: ioctl", progname);
		perror(errmsg);
		break;
	}
	exit(-1);
}

int
ipx_del_route(int argc, char **argv)
{
	/* Router */
	struct sockaddr_ipx	*sr = (struct sockaddr_ipx *)&rd.rt_gateway;
	/* Target */
	struct sockaddr_ipx	*st = (struct sockaddr_ipx *)&rd.rt_dst;
	int	s;
	int	result;
	unsigned long	netnum;
	char	errmsg[80];
	
	if (argc != 2) {
		usage();
	}

	rd.rt_flags = RTF_GATEWAY;
	/* Network Number */
	netnum = strtoul(argv[1], (char **)NULL, 16);
	if ((netnum == 0xffffffffL) || (netnum == 0L)) {
		fprintf(stderr, "%s: Inappropriate network number %08X.\n", 
			progname, netnum);
		exit(-1);
	}
	st->sipx_network = htonl(netnum);

	st->sipx_family = sr->sipx_family = AF_IPX;
	s = socket(AF_IPX, SOCK_DGRAM, AF_IPX);
	if (s < 0) {
		sprintf(errmsg, "%s: socket", progname);
		perror(errmsg);
		exit(-1);
	}
	result = ioctl(s, SIOCDELRT, &rd);
	if (result == 0) {
		fprintf(stdout, "Route deleted.\n");
		exit(0);
	}

	switch (errno) {
	case ENOENT:
		fprintf(stderr, "%s: Route not found for network %08X.\n",
			progname, netnum);
		break;
	case EPERM:
		fprintf(stderr, "%s: Network %08X is directly connected.\n",
			progname, netnum);
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
		ipx_add_route(argc-1, argv);
	} else if (strncasecmp(argv[1], "del", 3) == 0) {
		for (i = 1; i < (argc-1); i++) 
			argv[i] = argv[i+1];
		ipx_del_route(argc-1, argv);
	}
	usage();
}

#include <linux/ipx.h>
#include <linux/sockios.h>
#include <linux/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <getopt.h>

struct option	options[] = {
	{ "auto_primary", required_argument, NULL, 1 },
	{ "auto_interface", required_argument, NULL, 2 },
	{ NULL, 0, NULL, 0 }
};

char	*progname;

void
usage(void)
{
	fprintf(stderr, 
	"Usage: %s [--auto_primary=[on|off]] [--auto_interface=[on|off]]\n",
	progname);
}

int
map_string_to_bool(char *optarg)
{
	if ((strcasecmp(optarg, "ON") == 0) ||
			(strcasecmp(optarg, "TRUE") == 0) || 
			(strcasecmp(optarg, "SET") == 0)) {
		return 1;
	} else if ((strcasecmp(optarg, "OFF") == 0) ||
			(strcasecmp(optarg, "FALSE") == 0) || 
			(strcasecmp(optarg, "CLEAR") == 0)) {
		return 0;
	} 

	return -1;
}

int
main(int argc, char **argv)
{
	int	s;
	int	result;
	char	errmsg[80];
	char	val;
	int	option_index = 0;
	int	got_auto_pri = 0;
	int	got_auto_itf = 0;
	ipx_config_data	data;

	progname = argv[0];
	
	s = socket(AF_IPX, SOCK_DGRAM, AF_IPX);
	if (s < 0) {
		sprintf(errmsg, "%s: socket", progname);
		perror(errmsg);
		exit(-1);
	}

	sprintf(errmsg, "%s: ioctl", progname);
	while ((result = getopt_long(argc, argv, "", options, &option_index)) != -1) {
		switch (result) {
		case 1: 
			if (got_auto_pri)
				break;
			got_auto_pri++;
				
			val = map_string_to_bool(optarg);
			if (val < 0) {
				usage();
				exit(-1);
			}

			result = ioctl(s, SIOCAIPXPRISLT, &val);
			if (result < 0) {
				perror(errmsg);
				exit(-1);
			}
			break;
		case 2:
			if (got_auto_itf) 
				break;
			got_auto_itf++;

			val = map_string_to_bool(optarg);
			if (val < 0) {
				usage();
				exit(-1);
			}

			result = ioctl(s, SIOCAIPXITFCRT, &val);
			if (result < 0) {
				perror(errmsg);
				exit(-1);
			}
			break;
		}
	}
	result = ioctl(s, SIOCIPXCFGDATA, &data);
	if (result < 0) {
		perror(errmsg);
		exit(-1);
	}
	fprintf(stdout, "Auto Primary Select is %s\n\
Auto Interface Create is %s\n", 
		(data.ipxcfg_auto_select_primary) ? "ON" : "OFF",
		(data.ipxcfg_auto_create_interfaces) ? "ON" : "OFF");
	exit(0);
}


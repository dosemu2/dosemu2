#define AF_IPX		4
#define PF_IPX		AF_IPX
#define AF_IPX_IK	5

struct sockaddr_ipx
{
	short sipx_family;
	unsigned long  sipx_network;
	unsigned char sipx_node[6];
	short sipx_port;
};

struct ipx_route_def
{
	unsigned long ipx_network;
	unsigned long ipx_router_network;
#define IPX_ROUTE_NO_ROUTER	0
	unsigned char ipx_router_node[6];
	unsigned char ipx_device[16];
	unsigned short ipx_flags;
#define IPX_RT_SNAP		8
#define IPX_RT_8022		4
#define IPX_RT_BLUEBOOK		2
#define IPX_RT_ROUTED		1
};

#define IPX_MTU		1024

typedef struct {
	int	(*async_func)();
	void	*async_arg;
}	ipx_async_reg;

#define FCNTL_ASYNC	100


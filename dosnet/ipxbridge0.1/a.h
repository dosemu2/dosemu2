#define MAX_PACK_LEN 66560  /* 65535 + sizeof ether_hdr + bit extra */

#define ETHER_SIZE 14

struct ether_hdr
{
  u_char dst_mac[6];
  u_char src_mac[6];
  u_char pkt_type[2];  /* 0x8137 */
};

struct ether_trail
{
  u_char length;
  u_char *trailer;
};

/*
 * IPX packet types - not to be trusted: use dst sock
 */
#define IPX_UNKNOWN   0x00
#define IPX_RIP       0x01
#define IPX_SAP       0x04
#define IPX_SPX       0x05
#define IPX_NCP       0x11
#define IPX_NETBIOS   0x14

/*
 * IPX socket numbers - low address: high address always 0x04 for these
 */
#define IPX_S_NCP     0x51
#define IPX_S_SAP     0x52
#define IPX_S_RIP     0x53
#define IPX_S_NETBIOS 0x55
#define IPX_S_DIAG    0x56

#define IPX_HDR_SIZE 30

struct ipx_hdr
{
  u_char chk_sum[2];    /* always 0xffff */
  u_char length[2];
  u_char transport_ctl;
  u_char pkt_type;
  u_char dst_net[4];
  u_char dst_node[6];
  u_char dst_sock[2];
  u_char src_net[4];
  u_char src_node[6];
  u_char src_sock[2];
};

/*
 * IPX operations for RIP and SAP - high address always 0
 */
#define IPX_REQ    1
#define IPX_RESP   2
#define IPX_REQ_NEAR 3
#define IPX_RESP_NEAR 4

struct rip_ent
{
  u_char net_num[4];
  u_char num_hops[2];
  u_char num_ticks[2];
};

struct rip_pkt
{
  u_char op[2];
  u_char num_ents;
  struct rip_ent *net_ent;
};

struct sap_ent
{
  u_char serv_type[2];
  u_char serv_name[48];
  u_char net_num[4];
  u_char net_node[6];
  u_char net_sock[2];
  u_char net_hops[2];
};

struct sap_pkt
{
  u_char op[2];
  u_char num_ents;
  struct sap_ent *serv_ent;
};

struct ipx_pkt
{
  u_char interface;
  struct ether_hdr ipx_ether_hdr;
  struct ipx_hdr  ipx_ipx_hdr;
  union
  {
    struct sap_pkt ipx_sap;
    struct rip_pkt ipx_rip;
    char *ipx_other;
  } packet;
  struct ether_trail ipx_ether_trail;
};

struct route_entry
{
  u_char net_num[4];
  u_char num_hops;
  u_char num_ticks[2];
  u_char interface;
  u_char router[6];
  u_char age;
  struct route_entry *next;
};

struct server_entry
{
  u_char serv_type[2];
  u_char serv_name[48];
  u_char net_num[4];
  u_char net_node[6];
  u_char net_sock[2];
  u_char num_hops;
  u_char interface;
  u_char age;
  struct server_entry *next;
};

/* 
 * Tim Bird, tbird@novell.com (original code)
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* ipx.c for the DOS emulator
 * 96/07/31 -	Add callback from ESR and procedure to call into 
 *		IPX from DOSEmu (bios.S ESRFarCall). JES
 *
 * 21.10.2004 -	Removed callback from ESR and procedure to call into 
 *		IPX from DOSEmu (bios.S ESRFarCall). stsp
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netipx/ipx.h>
#include <netinet/in.h>
#include <errno.h>

#include "emu.h"
#include "timers.h"
#include "cpu.h"
#include "int.h"
#include "dpmi.h"
#include "bios.h"
#include "ipx.h"
#ifdef IPX

#define MAX_PACKET_DATA		1500

#define ECBp ((ECB_t*)FARt_PTR(ECBPtr))
#define AESECBp ((AESECB_t*)FARt_PTR(ECBPtr))

/* declare some function prototypes */
static u_char IPXCancelEvent(far_t ECBPtr);
static void ipx_recv_esr_call(void);
static void ipx_aes_esr_call(void);

static ipx_socket_t *ipx_socket_list = NULL;
/* hopefully these static ECBs will not cause races... */
static far_t recvECB;
static far_t aesECB;

/* DANG_FIXTHIS - get a real value for my address !! */
static unsigned char MyAddress[10] =
{0x01, 0x01, 0x00, 0xe0,
 0x00, 0x00, 0x1b, 0x33, 0x2b, 0x13};

static int GetMyAddress( void )
{
  int sock;
  struct sockaddr_ipx ipxs;
  struct sockaddr_ipx ipxs2;
  int len;
  int i;
  
  sock=socket(AF_IPX,SOCK_DGRAM,PF_IPX);
  if(sock==-1)
  {
    n_printf("IPX: could not open socket in GetMyAddress: %s\n", strerror(errno));
    return(-1);
  }
#define DYNAMIC_PORT 1
#if DYNAMIC_PORT
  #define DEF_PORT 0
#else
  #define DEF_PORT 0x5000
#endif
  ipxs.sipx_family=AF_IPX;
  ipxs.sipx_network=htonl(config.ipx_net);
  ipxs.sipx_port=htons(DEF_PORT);
  
  /* bind this socket to network */  
  if(bind(sock,(struct sockaddr *)&ipxs,sizeof(ipxs))==-1)
  {
    n_printf("IPX: could not bind to network %#lx in GetMyAddress: %s\n",
      config.ipx_net, strerror(errno));
    close( sock );
    return(-1);
  }
  
  len = sizeof(ipxs2);
  if (getsockname(sock,(struct sockaddr *)&ipxs2,&len) < 0) {
    n_printf("IPX: could not get socket name in GetMyAddress: %s\n", strerror(errno));
    close( sock );
    return(-1);
  }

  *((uint32_t *)MyAddress) = ipxs2.sipx_network;
  for (i = 0; i < 6; i++) {
    MyAddress[4+i] = ipxs2.sipx_node[i];
  }
  n_printf("IPX: using network address %02X%02X%02X%02X\n",
    MyAddress[0], MyAddress[1], MyAddress[2], MyAddress[3] );
  n_printf("IPX: using node address of %02X%02X%02X%02X%02X%02X\n",
    MyAddress[4], MyAddress[5], MyAddress[6], MyAddress[7],
    MyAddress[8], MyAddress[9] );
#if DYNAMIC_PORT
  close( sock );
#endif
  return(0);
}

void ipx_init(void)
{
  int ccode;
  if (!config.ipxsup)
    return;
  ccode = GetMyAddress();
  if( ccode ) {
    error("IPX: cannot get IPX node address for network %#lx\n", config.ipx_net);
  }
  pic_seti(PIC_IPX, ipx_receive, 0, ipx_recv_esr_call);
  pic_seti(PIC_IPX_AES, IPXCheckForAESReady, 0, ipx_aes_esr_call);
}

/*************************
 * Check for IPX - int 2f, AH=7A
 * returns AL=FF
 * 	ES:DI points to helper routine which gets to FarCallHandler
 *************************/
int
IPXInt2FHandler(void)
{
  LO(ax) = 0xff;
  REG(es) = IPX_SEG;
  LWORD(edi) = IPX_OFF;
  n_printf("IPX: request for IPX far call handler address %x:%x\n",
	   IPX_SEG, IPX_OFF);
  return 1;
}

static void 
ipx_remove_socket(ipx_socket_t * sk)
{
  ipx_socket_t *s;

  s = ipx_socket_list;
  if (s == sk) {
    ipx_socket_list = s->next;
    free(s);
    return;
  }
  while (s && s->next) {
    if (s->next == sk) {
      s->next = sk->next;
      free(sk);
      return;
    }
    s = s->next;
  }
}

static void 
ipx_insert_socket(u_short socket, u_short PSP, int fd)
{
  ipx_socket_t *sk;

  sk = (ipx_socket_t *) malloc(sizeof(ipx_socket_t));

  sk->socket = socket;
  sk->listenList = (far_t){0, 0};
  sk->listenCount = 0;
  sk->AESList = (far_t){0, 0};
  sk->AESCount = 0;
  sk->PSP = PSP;
  sk->fd = fd;
  sk->next = ipx_socket_list;
  ipx_socket_list = sk;
}

static ipx_socket_t *
ipx_find_socket(int port)
{
  ipx_socket_t *s;

  s = ipx_socket_list;
  while (s) {
    if (s->socket == port) {
      return (s);
    }
    s = s->next;
  }
  return (NULL);
}

static void dumpBytes(u_char * memptr, int count)
{
  int i, linecounter;

  linecounter = 0;
  if (count > 64) {
    /* dumping whole packet takes too long */
    count = 64;
  }
  for (i = 0; i < count; i++) {
    if (linecounter == 0) {
      n_printf("%p: ", memptr);
    }
    n_printf("%02x ", *memptr++);
    if (linecounter == 7) {
      n_printf("-");
    }
    linecounter++;
    if (linecounter == 16) {
      n_printf("\n");
      linecounter = 0;
    }
  }
  n_printf("\n");
}

static void printECB(ECB_t * ECB)
{
  int i;
   
  if(debug_level('n')) {
    n_printf("--DOS ECB (dump)--\n");
    dumpBytes((u_char *) ECB, 60);
    n_printf("--DOS ECB--\n");
    n_printf("Link             %04x:%04x\n", ECB->Link.segment, ECB->Link.offset);
    n_printf("ESR              %04x:%04x\n", ECB->ESRAddress.segment, ECB->ESRAddress.offset);
    n_printf("InUseFlag        %02x\n", ECB->InUseFlag);
    n_printf("CompletionCode   %02x\n", ECB->CompletionCode);
    n_printf("ECBSocket	     %04x\n", ntohs(ECB->ECBSocket));
    n_printf("ImmediateAddress %02X%02X%02X%02X%02X%02X\n",
      ECB->ImmediateAddress[0],
      ECB->ImmediateAddress[1],
      ECB->ImmediateAddress[2],
      ECB->ImmediateAddress[3],
      ECB->ImmediateAddress[4],
      ECB->ImmediateAddress[5]);
    n_printf("FragmentCount	 %d\n", ECB->FragmentCount);
    for (i = 0; i < ECB->FragmentCount; i++) {
      n_printf("Frag[%d].Length   %d\n", i, ECB->FragTable[i].Length);
      n_printf("Frag[%d].Address	 %p\n", i,
        FARt_PTR(ECB->FragTable[i].Address));
    }
  }
}

static void printIPXHeader(IPXPacket_t * IPXHeader)
{
  if (debug_level('n')) {
    n_printf("--IPX Header (dump)--\n");
    dumpBytes((u_char *) IPXHeader, 30);
    n_printf("--IPX Header --\n");
    n_printf("Checksum         %04x\n", ntohs(IPXHeader->Checksum));
    n_printf("Length           %d\n", ntohs(IPXHeader->Length));
    n_printf("TransportControl %d\n", IPXHeader->TransportControl);
    n_printf("PacketType       %d\n", IPXHeader->PacketType);
    n_printf("Destination      %02X%02X%02X%02X:%02X%02X%02X%02X%02X%02X:%02X%02X\n",
      IPXHeader->Destination.Network[0],
      IPXHeader->Destination.Network[1],
      IPXHeader->Destination.Network[2],
      IPXHeader->Destination.Network[3],
      IPXHeader->Destination.Node[0],
      IPXHeader->Destination.Node[1],
      IPXHeader->Destination.Node[2],
      IPXHeader->Destination.Node[3],
      IPXHeader->Destination.Node[4],
      IPXHeader->Destination.Node[5],
      IPXHeader->Destination.Socket[0],
      IPXHeader->Destination.Socket[1]);
    n_printf("Source           %02X%02X%02X%02X:%02X%02X%02X%02X%02X%02X:%02X%02X\n",
      IPXHeader->Source.Network[0],
      IPXHeader->Source.Network[1],
      IPXHeader->Source.Network[2],
      IPXHeader->Source.Network[3],
      IPXHeader->Source.Node[0],
      IPXHeader->Source.Node[1],
      IPXHeader->Source.Node[2],
      IPXHeader->Source.Node[3],
      IPXHeader->Source.Node[4],
      IPXHeader->Source.Node[5],
      IPXHeader->Source.Socket[0],
      IPXHeader->Source.Socket[1]);
  }
}

static void ipx_async_callback(void)
{
  n_printf("IPX: requesting receiver IRQ\n");
  pic_request(PIC_IPX);
}

static u_char IPXOpenSocket(u_short port, u_short * newPort)
{
  int sock;			/* sock here means Linux socket handle */
  int opt;
  struct sockaddr_ipx ipxs;
  int len;
  struct sockaddr_ipx ipxs2;

  /* DANG_FIXTHIS - do something with longevity flag */
  n_printf("IPX: open socket %#x\n", ntohs(port));
  if (port != 0 && ipx_find_socket(port) != NULL) {
    n_printf("IPX: socket %x already open.\n", ntohs(port));
    /* someone already has this socket open */
    return (RCODE_SOCKET_ALREADY_OPEN);
  }
  /* DANG_FIXTHIS - kludge to support broken linux IPX stack */
  /* need to convert dynamic socket open into a real socket number */
/*  if (port == 0) {
    n_printf("IPX: using socket %x\n", nextDynamicSocket);
    port = nextDynamicSocket++;
  }
*/
  /* do a socket call, then bind to this port */
  sock = socket(AF_IPX, SOCK_DGRAM, PF_IPX);
  if (sock == -1) {
    n_printf("IPX: could not open IPX socket: %s.\n", strerror(errno));
    /* I can't think of anything else to return */
    return (RCODE_SOCKET_TABLE_FULL);
  }

  opt = 1;
  /* Permit broadcast output */
  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
		 &opt, sizeof(opt)) == -1) {
    /* I can't think of anything else to return */
    n_printf("IPX: could not set socket option for broadcast: %s.\n", strerror(errno));
    return (RCODE_SOCKET_TABLE_FULL);
  }
  /* allow setting the type field in the IPX header */
  opt = 1;
  if (setsockopt(sock, SOL_IPX, IPX_TYPE, &opt, sizeof(opt)) == -1) {
    /* I can't think of anything else to return */
    n_printf("IPX: could not set socket option for type: %s.\n", strerror(errno));
    return (RCODE_SOCKET_TABLE_FULL);
  }
  ipxs.sipx_family = AF_IPX;
  ipxs.sipx_network = *((unsigned int *)&MyAddress[0]);
/*  ipxs.sipx_network = htonl(MyNetwork); */
  memset(ipxs.sipx_node, 0, 6);	/* Please fill in my node name */
  ipxs.sipx_port = port;

  /* now bind to this port */
  if (bind(sock, (struct sockaddr *) &ipxs, sizeof(ipxs)) == -1) {
    /* I can't think of anything else to return */
    n_printf("IPX: could not bind socket to address: %s\n", strerror(errno));
    close( sock );
    return (RCODE_SOCKET_TABLE_FULL);
  }
  
  if( port==0 ) {
    len = sizeof(ipxs2);
    if (getsockname(sock,(struct sockaddr *)&ipxs2,&len) < 0) {
      /* I can't think of anything else to return */
      n_printf("IPX: could not get socket name in IPXOpenSocket: %s\n", strerror(errno));
      close( sock );
      return(RCODE_SOCKET_TABLE_FULL);
    } else {
      port = ipxs2.sipx_port;
      n_printf("IPX: opened dynamic socket %04x\n", port);
    }
  }
  
  /* if we successfully bound to this port, then record it */
  ipx_insert_socket(port, /* PSP */ 0, sock);
  add_to_io_select(sock, 1, ipx_async_callback);
  n_printf("IPX: successfully opened socket %i, %04x\n", sock, port);
  *newPort = port;
  return (RCODE_SUCCESS);
}

static u_char IPXCloseSocket(u_short port)
{
  ipx_socket_t *mysock;
  far_t ECBPtr;

  /* see if this socket is actually open */
  n_printf("IPX: close socket %x\n", port);
  mysock = ipx_find_socket(port);
  if (mysock == NULL) {
    n_printf("IPX: close of unopened socket\n");
    return (RCODE_SOCKET_NOT_OPEN);
  }
  /* cancel all pending events on this socket */
  n_printf("IPX: canceling all listen events on socket %x\n", port);
  ECBPtr = mysock->listenList;
  while (FARt_PTR(ECBPtr)) {
    if (IPXCancelEvent(ECBPtr) != RCODE_SUCCESS)
      return RCODE_CANNOT_CANCEL_EVENT;
    ECBPtr = mysock->listenList;
  }
  n_printf("IPX: canceling all AES events on socket %x\n", port);
  ECBPtr = mysock->AESList;
  while (FARt_PTR(ECBPtr)) {
    if (IPXCancelEvent(ECBPtr) != RCODE_SUCCESS)
      return RCODE_CANNOT_CANCEL_EVENT;
    ECBPtr = mysock->AESList;
  }
  /* now close the file descriptor for the socket, and free it */
  n_printf("IPX: closing file descriptor on socket %x\n", port);
  remove_from_io_select(mysock->fd, 1);
  close(mysock->fd);
  ipx_remove_socket(mysock);
  n_printf("IPX: successfully closed socket %x\n", port);
  return (RCODE_SUCCESS);
}

static int GatherFragmentData(char *buffer, ECB_t * ECB)
{
  int i;
  int nextFragLen, totalDataCount;
  u_char *bufptr, *memptr;

  totalDataCount = 0;
  bufptr = buffer;
  for (i = 0; i < ECB->FragmentCount; i++) {
    nextFragLen = ECB->FragTable[i].Length;
    memptr = FARt_PTR(ECB->FragTable[i].Address);
    if (i == 0) {
      /* subtract off IPX header size from first fragment */
      nextFragLen -= 30;
      memptr += 30;
    }
    if (nextFragLen) {
      /* DANG_FIXTHIS - dumb use of hardcoded 1500 here */
      if (totalDataCount + nextFragLen > 1500) {
	return (-1);
      }
      memcpy(bufptr, memptr, nextFragLen);
      bufptr += nextFragLen;
      totalDataCount += nextFragLen;
    }
  }
  n_printf("IPX: gathered %d fragments for %d bytes of data\n",
	   ECB->FragmentCount, totalDataCount);
  if (debug_level('n')) {
    dumpBytes(buffer, totalDataCount);
  }
  return (totalDataCount);
}

static void ipx_esr_call(far_t ECBPtr, u_char AXVal)
{
  struct vm86_regs saved_regs;

  if(in_dpmi && !in_dpmi_dos_int)
    fake_pm_int();
  fake_int_to(BIOSSEG, EOI_OFF);

  saved_regs = REGS;
  n_printf("IPX: Calling ESR at %04x:%04x of ECB at %04x:%04x\n",
    ECBp->ESRAddress.segment, ECBp->ESRAddress.offset,
    ECBPtr.segment, ECBPtr.offset);
  REG(es) = ECBPtr.segment;
  LWORD(esi) = ECBPtr.offset;
  LO(ax) = AXVal;
  do_call_back(ECBp->ESRAddress.segment << 16 | ECBp->ESRAddress.offset);
  REGS = saved_regs;
  n_printf("IPX: ESR callback ended\n");
}

static void ipx_recv_esr_call(void)
{
  n_printf("IPX: Calling receive ESR\n");
  ipx_esr_call(recvECB, ESR_CALLOUT_IPX);
}

static void ipx_aes_esr_call(void)
{
  n_printf("IPX: Calling AES ESR\n");
  ipx_esr_call(aesECB, ESR_CALLOUT_AES);
}

static u_char IPXSendPacket(far_t ECBPtr)
{
  IPXPacket_t *IPXHeader;
  u_char data[MAX_PACKET_DATA];
  struct sockaddr_ipx ipxs;
  int dataLen;
  ipx_socket_t *mysock;

  printECB(ECBp);
  dataLen = GatherFragmentData(data, ECBp);
  if (dataLen == -1) {
    ECBp->InUseFlag = IU_ECB_FREE;
    ECBp->CompletionCode = CC_FRAGMENT_ERROR;
    return RCODE_ECB_NOT_IN_USE;	/* FIXME - what is to return here?? */;
  }
  IPXHeader = (IPXPacket_t *)FARt_PTR(ECBp->FragTable[0].Address);
  /* for a complete emulation, we need to fill in fields in the */
  /* send packet header */
  /* first field is an IPX convention, not really a checksum */
  IPXHeader->Checksum = 0xffff;
  IPXHeader->Length = htons(dataLen + 30);	/* in network order */
  memcpy(&IPXHeader->Source, MyAddress, 10);
  *((u_short *) &IPXHeader->Source.Socket) = ECBp->ECBSocket;
  printIPXHeader(IPXHeader);
  ipxs.sipx_family = AF_IPX;
  /* get destination address from IPX packet header */
  memcpy(&ipxs.sipx_network, IPXHeader->Destination.Network, 4);
  /* if destination address is 0, then send to my net */
  if (ipxs.sipx_network == 0) {
    ipxs.sipx_network = *((unsigned int *)&MyAddress[0]);
/*  ipxs.sipx_network = htonl(MyNetwork); */
  }
  memcpy(&ipxs.sipx_node, IPXHeader->Destination.Node, 6);
  memcpy(&ipxs.sipx_port, IPXHeader->Destination.Socket, 2);
  ipxs.sipx_type = 1;
  /*	ipxs.sipx_port=htons(0x452); */
  mysock = ipx_find_socket(ECBp->ECBSocket);
  if (mysock == NULL) {
    /* DANG_FIXTHIS - should do a bind, send, close here */
    /* DOS IPX allows sending on unopened sockets */
    n_printf("IPX: send to unopened socket %04x\n", ntohs(ECBp->ECBSocket));
    return RCODE_SOCKET_NOT_OPEN;
  }
  if (sendto(mysock->fd, (void *) &data, dataLen, 0,
	     (struct sockaddr *) &ipxs, sizeof(ipxs)) == -1) {
    n_printf("IPX: error sending packet: %s\n", strerror(errno));
    ECBp->InUseFlag = IU_ECB_FREE;
    ECBp->CompletionCode = CC_HARDWARE_ERROR;
    return RCODE_CANNOT_FIND_ROUTE;	/* FIXME - what is to return here?? */
  }
  else {
    ECBp->InUseFlag = IU_ECB_FREE;
    ECBp->CompletionCode = CC_SUCCESS;
    n_printf("IPX: successfully sent packet\n");
  }
  return RCODE_SUCCESS;
}

static u_char IPXListenForPacket(far_t ECBPtr)
{
  ipx_socket_t *mysock;

  printECB(ECBp);
  /* see if the socket is open for this ECB */
  mysock = ipx_find_socket(ECBp->ECBSocket);
  if (mysock == NULL) {
    n_printf("IPX: listen on unopened socket\n");
    ECBp->InUseFlag = IU_ECB_FREE;
    ECBp->CompletionCode = CC_SOCKET_NOT_OPEN;
    return (RCODE_SOCKET_NOT_OPEN);
  }

  ECBp->InUseFlag = IU_ECB_LISTENING;
  ECBp->CompletionCode = CC_SUCCESS;
  /* now put this ECB on the listen list for this socket */
  ECBp->Link = mysock->listenList;
  mysock->listenList = ECBPtr;
  mysock->listenCount++;

  n_printf("IPX: successfully posted listen packet on socket %x\n",
	   ntohs(mysock->socket));
  return (RCODE_SUCCESS);
}

static u_char IPXScheduleEvent(far_t ECBPtr, u_char inUseCode, u_short delayTime)
{
  ipx_socket_t *mysock;

  printECB(ECBp);
  /* see if the socket is open for this ECB */
  mysock = ipx_find_socket(ECBp->ECBSocket);
  if (mysock == NULL) {
    n_printf("IPX: AES event on unopened socket\n");
    ECBp->InUseFlag = IU_ECB_FREE;
    ECBp->CompletionCode = CC_SOCKET_NOT_OPEN;
    return (RCODE_SOCKET_NOT_OPEN);
  }

  ECBp->InUseFlag = inUseCode;
  ECBp->CompletionCode = CC_SUCCESS;
  /* now put this ECB on the AES list for this socket */
  ECBp->Link = mysock->AESList;
  AESECBp->TimeLeft = delayTime;
  mysock->AESList = ECBPtr;
  mysock->AESCount++;

  n_printf("IPX: successfully posted AES event on socket %x\n",
	   ntohs(mysock->socket));
  n_printf("IPX: AES delay time %d\n", AESECBp->TimeLeft);
  return (RCODE_SUCCESS);
}

static u_char IPXCancelEvent(far_t ECBPtr)
{
  ipx_socket_t *mysock;
  ECB_t *prevECB = NULL;
  far_t *ECBList;

  /* if it was a listen, cancel it */
  if (ECBp->InUseFlag == IU_ECB_LISTENING ||
      ECBp->InUseFlag == IU_ECB_IPX_WAITING ||
      ECBp->InUseFlag == IU_ECB_AES_WAITING) {
    /* look up the socket for this ECB */
    mysock = ipx_find_socket(ECBp->ECBSocket);
    if (mysock == NULL) {
      n_printf("IPX: cancel on unopened socket\n");
      ECBp->InUseFlag = IU_ECB_FREE;
      ECBp->CompletionCode = CC_SOCKET_NOT_OPEN;
      return (RCODE_SOCKET_NOT_OPEN);
    }
    n_printf("IPX: canceling event on socket %x\n", ntohs(mysock->socket));
    /* now see if this ECB is actually on the list for this socket */
    if (ECBp->InUseFlag == IU_ECB_LISTENING) {
      /* check the listenList */
      ECBList = &mysock->listenList;
      n_printf("IPX: cancel a listen event from %d events\n",
	       mysock->listenCount);
    }
    else {
      /* check the AESList */
      ECBList = &mysock->AESList;
      n_printf("IPX: cancel an AES event from %d events\n",
	       mysock->AESCount);
    }
    /* this next cast is somewhat evil.  If the first item on the */
    /* list is the one removed, then change the head pointer for */
    /* the list as if it were a ECB.Link field. */
    n_printf("IPX: scanning ECBList for match\n");
    while (FARt_PTR(*ECBList)) {
      /* see if the list pointer equals the ECB we are canceling */
      n_printf("IPX: ECB = %p, ECBList = %p\n", ECBp, FARt_PTR(*ECBList));
      if (FARt_PTR(*ECBList) == ECBp) {
	/* remove it from the list */
	if (prevECB)
	  prevECB->Link = ECBp->Link;
	else
	  *ECBList = ECBp->Link;
	if (ECBp->InUseFlag == IU_ECB_LISTENING) {
	  mysock->listenCount--;
	}
	else {
	  mysock->AESCount--;
	}
	ECBp->InUseFlag = IU_ECB_FREE;
	ECBp->CompletionCode = CC_EVENT_CANCELED;
	n_printf("IPX: successfully canceled event\n");
	return (RCODE_SUCCESS);
      }
      prevECB = FARt_PTR(*ECBList);
      ECBList = &prevECB->Link;
    }
    n_printf("IPX: ECB was not in use.\n");
    return (RCODE_ECB_NOT_IN_USE);
  }
  else {
    if (ECBp->InUseFlag == IU_ECB_FREE) {
      return (RCODE_ECB_NOT_IN_USE);
    }
  }
  return (RCODE_CANNOT_CANCEL_EVENT);
}

void AESTimerTick(void)
{
  ipx_socket_t *mysock;
  AESECB_t *ECB;

  /* run through the socket list, and process any pending AES events */
  mysock = ipx_socket_list;
  while (mysock) {
    /* see if this socket has any AES events pending */
    ECB = FARt_PTR(mysock->AESList);
    while (ECB) {
      if (ECB->TimeLeft > 0) {
	ECB->TimeLeft--;
	n_printf("IPX: AES timer decremented to %d on ECB at %p\n",
		 ECB->TimeLeft, ECB);
	if (ECB->TimeLeft == 0) {
	  /* now setup to call the ESR for this event */
	  pic_request(PIC_IPX_AES);
	  return;
	}
      }
      ECB = FARt_PTR(ECB->Link);
    }
    mysock = mysock->next;
  }
}

static void ipx_fdset(fd_set * set)
{
  ipx_socket_t *s;

  s = ipx_socket_list;
  while (s != NULL) {
    FD_SET(s->fd, set);
    s = s->next;
  }
}

static int ScatterFragmentData(int size, char *buffer, ECB_t * ECB,
                               struct sockaddr_ipx *sipx)
{
  int i;
  int nextFragLen, dataLeftCount;
  u_char *bufptr, *memptr;
  IPXPacket_t *IPXHeader;

  n_printf("received data bytes:\n");
  if (debug_level('n')) {
    dumpBytes(buffer, size);
  }
  dataLeftCount = size;
  bufptr = buffer;
  if (ECB->FragmentCount < 1 || ECB->FragTable[0].Length < 30) {
    /* posted listen has a fragment error */
    n_printf("IPX: listen packet fragment error\n");
    return (size);
  }
  i = 0;
  while (i < ECB->FragmentCount && dataLeftCount) {
    nextFragLen = ECB->FragTable[i].Length;
    memptr = FARt_PTR(ECB->FragTable[i].Address);
    n_printf("IPX: filling fragment %d at %p, max_length %d with %d bytes left\n",
	     i, FARt_PTR(ECB->FragTable[i].Address), nextFragLen,
	     dataLeftCount);
    if (i == 0) {
      /* subtract off IPX header size from first fragment */
      nextFragLen -= 30;
      /* also, fill out IPX header */
      /* use data from sipx to fill out source address */
      IPXHeader = (IPXPacket_t *) memptr;
      IPXHeader->Checksum = 0xFFFF;
      IPXHeader->Length = htons(size + 30); /* in network order */
      /* DANG_FIXTHIS - use real values to fill out IPX header here */
      IPXHeader->TransportControl = 1;
      IPXHeader->PacketType = 0;
      memcpy((u_char *) & IPXHeader->Destination, MyAddress, 10);
      *((u_short *) &IPXHeader->Destination.Socket) = ECB->ECBSocket;
      memcpy(IPXHeader->Source.Network, (char *) &sipx->sipx_network, 4);
      memcpy(IPXHeader->Source.Node, sipx->sipx_node, 6);
      *((u_short *) &IPXHeader->Source.Socket) = sipx->sipx_port;
      /* DANG_FIXTHIS - kludge for bug in linux IPX stack */
      /* IPX stack returns data count including IPX header */
      /*			dataLeftCount -= 30; */
      memptr += 30;
      printIPXHeader(IPXHeader);
    }
    if (nextFragLen > dataLeftCount) {
      nextFragLen = dataLeftCount;
    }
    if (nextFragLen) {
      memcpy(memptr, bufptr, nextFragLen);
      bufptr += nextFragLen;
      dataLeftCount -= nextFragLen;
    }
    i++;
  }
  n_printf("IPX: scattered %d fragments, %d bytes left over.\n",
	   i, dataLeftCount);
  return (dataLeftCount);
}

static int IPXReceivePacket(ipx_socket_t * s)
{
  struct sockaddr_ipx ipxs;
  char buffer[MAX_PACKET_DATA];
  far_t ECBPtr;
  int size, sz;

  sz = sizeof(ipxs);
  size = recvfrom(s->fd, buffer, sizeof(buffer), 0,
		  (struct sockaddr *) &ipxs, &sz);
  n_printf("IPX: received %d bytes of data\n", size);
  if (size > 0 && s->listenCount) {
    ECBPtr = s->listenList;
    printECB(ECBp);
    sz = ScatterFragmentData(size, buffer, ECBp, &ipxs);
    IPXCancelEvent(ECBPtr);
    /* if there was packet left over, then report a fragment error */
    if (sz) {
      error("IPX: fragmentation error, %i left\n", sz);
      ECBp->CompletionCode = CC_FRAGMENT_ERROR;
      /* DANG_FIXTHIS - should remove this override, linux IPX stack returns bad sizes, but we ignore this here */
      ECBp->CompletionCode = CC_SUCCESS;
    }
    else {
      ECBp->CompletionCode = CC_SUCCESS;
    }
    return 1;
  }
  return 0;
}

int IPXCheckForAESReady(int ilevel)
{
  ipx_socket_t *s;
  far_t ECBPtr;
  u_short rcode;

  s = ipx_socket_list;
  while (s) {
    if (s->AESCount) {
      ECBPtr = s->AESList;
      while (FARt_PTR(ECBPtr)) {
	if (AESECBp->TimeLeft == 0) {
	  /* DANG_FIXTHIS - this architecture currently only supports firing of one AES event per call */
	  n_printf("IPX: AES event ready on ECB at %p\n", ECBp);
	  printECB(ECBp);
	  aesECB = ECBPtr;
	  /* now remove the ECB from the AES list */
	  rcode = IPXCancelEvent(ECBPtr);
	  if (rcode == RCODE_SUCCESS) {
	    ECBp->CompletionCode = CC_SUCCESS;
	  }
	  return 1;	/* run IRQ */
	}
	ECBPtr = ECBp->Link;
      }
    }    
    s = s->next;
  }
  return 0;
}

static ipx_socket_t *check_ipx_ready(fd_set * set)
{
  ipx_socket_t *s;
  s = ipx_socket_list;
  while (s) {
    /* DANG_FIXTHIS - for now, just pick up one listen per poll, because we can only set up one ESR callout per relinquish */
    if (FD_ISSET(s->fd, set)) {
      break;
    }
    s = s->next;
  }
  return s;
}

static void IPXRelinquishControl(void)
{
  idle(0, 5, 0, INT2F_IDLE_USECS, "IPX");
}

int ipx_receive(int ilevel)
{
  /* DOS program has given us a time slice */
  /* let's use this as an opportunity to poll outstanding listens */
  fd_set fds;
  int selrtn;
  ipx_socket_t *s;
  far_t ECBPtr;
  struct timeval timeout;

  FD_ZERO(&fds);

  ipx_fdset(&fds);

  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  n_printf("IPX: select\n");

  switch ((selrtn = select(255, &fds, NULL, NULL, &timeout))) {
  case 0:			/* none ready */
    /*			n_printf("IPX: no receives ready\n"); */
    break;

  case -1:			/* error (not EINTR) */
    error("bad ipc_select: %s\n", strerror(errno));
    break;

  default:			/* has at least 1 descriptor ready */
    n_printf("IPX: receive ready (%i), checking fd\n", selrtn);
    if ((s = check_ipx_ready(&fds))) {
      ECBPtr = s->listenList;
      if (IPXReceivePacket(s)) {
        if (FARt_PTR(ECBp->ESRAddress)) {
          recvECB = ECBPtr;
          return 1;	/* run IRQ */
        }
      }
    }
    break;
  }
  return 0;
}

int ipx_int7a(void)
{
  u_short port;			/* port here means DOS IPX socket */
  u_short newPort;
  unsigned short *timerticks;
  u_char *AddrPtr;
  far_t ECBPtr;
  unsigned long network;
  int hops, ticks;

  n_printf("IPX: request number 0x%x\n", LWORD(ebx));
  switch (LWORD(ebx)) {
  case IPX_OPEN_SOCKET:
    if (LO(ax) != 0xff)
      n_printf("IPX: OpenSocket: longevity flag (%#x) not supported\n", LO(ax));
    port = LWORD(edx);
    newPort = 0;
    LO(ax) = IPXOpenSocket(port, &newPort);
    if (LO(ax) == RCODE_SUCCESS)
      LWORD(edx) = newPort;
    break;
  case IPX_CLOSE_SOCKET:
    port = LWORD(edx);
    LO(ax) = IPXCloseSocket(port);
    break;
  case IPX_GET_LOCAL_TARGET:
    /* do nothing here because routing is handled by IPX */
    /* normally this would return an ImmediateAddress, but */
    /* the ECB ImmediateAddress is never used, so just return */
    network = READ_DWORD(SEG_ADR((unsigned int *), es, si));
    n_printf("IPX: GetLocalTarget for network %08lx\n", network );
    if( network==0 || network== *((uint32_t *)&MyAddress[0]) ) {
      n_printf("IPX: returning GLT success for local address\n");
      LO(ax) = RCODE_SUCCESS;
      LWORD(ecx) = 1;
    } else {
      if( IPXGetLocalTarget( network, &hops, &ticks )==0 ) {
        LO(ax) = RCODE_SUCCESS;
        LWORD(ecx) = ticks;
      } else {
        n_printf("IPX: GetLocalTarget failed.\n");
        LO(ax) = RCODE_CANNOT_FIND_ROUTE;
      }
    }
    break;
  case IPX_FAST_SEND:
    n_printf("IPX: fast send\n");
    /* just fall through to regular send */
  case IPX_SEND_PACKET: {
    int ret;
    ECBPtr.segment = REG(es);
    ECBPtr.offset = LWORD(esi);
    n_printf("IPX: send packet ECB at %p\n", ECBp);
    /* What the hell is the async send? Do it synchroniously! */
    ret = IPXSendPacket(ECBPtr);
    if ((ret == RCODE_SUCCESS) && FARt_PTR(ECBp->ESRAddress))
      ipx_esr_call(ECBPtr, ESR_CALLOUT_IPX);
    LO(ax) = ret;
    break;
  }
  case IPX_LISTEN_FOR_PACKET:
    ECBPtr.segment = REG(es);
    ECBPtr.offset = LWORD(esi);
    n_printf("IPX: listen for packet, ECB at %x:%x\n",
	     ECBPtr.segment, ECBPtr.offset);
    /* put this packet on the queue of listens for this socket */
    LO(ax) = IPXListenForPacket(ECBPtr);
    break;
  case IPX_SCHEDULE_IPX_EVENT:
    ECBPtr.segment = REG(es);
    ECBPtr.offset = LWORD(esi);
    n_printf("IPX: schedule IPX event for ECB at %x:%x\n",
	     ECBPtr.segment, ECBPtr.offset);
    /* put this packet on the queue of AES events for this socket */
    LO(ax) = IPXScheduleEvent(ECBPtr, IU_ECB_IPX_WAITING,
			      LWORD(eax));
    break;
  case IPX_CANCEL_EVENT:
    ECBPtr.segment = REG(es);
    ECBPtr.offset = LWORD(esi);
    n_printf("IPX: cancel event for ECB at %p\n", ECBp);
    LO(ax) = IPXCancelEvent(ECBPtr);
    break;
  case IPX_SCHEDULE_AES_EVENT:
    ECBPtr.segment = REG(es);
    ECBPtr.offset = LWORD(esi);
    n_printf("IPX: schedule AES event ECB at %p\n", FARt_PTR(ECBPtr));
    /* put this packet on the queue of AES events for this socket */
    LO(ax) = IPXScheduleEvent(ECBPtr, IU_ECB_AES_WAITING,
			      LWORD(eax));
    break;
  case IPX_GET_INTERVAL_MARKER:
    /* Note that timerticks is actually an unsigned long in BIOS */
    /* this works because of intel lo-hi architecture */
    timerticks = BIOS_TICK_ADDR;
    LWORD(eax) = *timerticks;
    n_printf("IPX: get interval marker %d\n", LWORD(eax));
    /*			n_printf("IPX: doing extra relinquish control\n"); */
    IPXRelinquishControl();
    break;
  case IPX_GET_INTERNETWORK_ADDRESS:
    n_printf("IPX: get internetwork address\n");
    AddrPtr = SEG_ADR((u_char *), es, si);
    memcpy(AddrPtr, MyAddress, 10);
    break;
  case IPX_RELINQUISH_CONTROL:
    n_printf("IPX: relinquish control\n");
    IPXRelinquishControl();
    break;
  case IPX_DISCONNECT:
    n_printf("IPX: disconnect\n");
    break;
  case IPX_SHELL_HOOK:
    n_printf("IPX: shell hook\n");
    break;
  case IPX_GET_MAX_PACKET_SIZE:
    n_printf("IPX: get max packet size\n");
    /* return max data size in AX, and suggested retries in CL */
    /* DANG_FIXTHIS - return a real max packet size here */
    LWORD(eax) = 1024;		/* must be a power of 2 */
    LO(cx) = 20;
    break;
  case IPX_GET_MEDIA_DATA_SIZE:
    n_printf("IPX: get max packet size\n");
    /* return max data size in AX, and suggested retries in CL */
    /* DANG_FIXTHIS - return a real max media size here */
    LWORD(eax) = 1480;
    LO(cx) = 20;
    break;
  case IPX_CLEAR_SOCKET:
    n_printf("IPX: clear socket\n");
    break;
  default:
    n_printf("IPX: Unimplemented function.\n");
    break;
  }
  return 1;
}

/* ipx_close is called on DOSEMU shutdown */
void
ipx_close(void)
{
  ipx_socket_t *s;

  /* run through the socket list and close each socket */
  s = ipx_socket_list;
  while (s != NULL) {
    IPXCloseSocket(s->socket);
    s = ipx_socket_list;
  }
}
#endif

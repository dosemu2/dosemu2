/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* ipx.c for the DOS emulator
 * 		Tim Bird, tbird@novell.com
 *
 * 96/07/31 -	Add callback from ESR and procedure to call into 
 *		IPX from DOSEmu (bios.S ESRFarCall). JES
 */
#include "ipx.h"
#ifdef IPX

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vm86.h>
#include <netipx/ipx.h>
#include <netinet/in.h>
#include <errno.h>

#include "memory.h"
#include "emu.h"
#include "timers.h"
#include "cpu.h"
#include "int.h"
#include "dpmi.h"
#include "bios.h"
#include "inifile.h"
#include "doshelpers.h"

#define MAX_PACKET_DATA		1500

#if 0
/* NOTE: the below is already defined with #include "emu.h"
 *       Must NOT redefine it, else vm86plus won't work !!!
 */
extern struct vm86_struct vm86s;
#endif

/* declare some function prototypes */
static u_char IPXCancelEvent(far_t ECBPtr);
far_t ESRPopRegistersReturn;
far_t ESRPopRegistersIRet;
far_t ESRFarCall;

static ipx_socket_t *ipx_socket_list = NULL;
static int IPXRunning = 0;

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

  /* bind this socket to network 0 */  
  ipxs.sipx_family=AF_IPX;
  ipxs.sipx_network=0;
  ipxs.sipx_port=0;
  
  if(bind(sock,(struct sockaddr *)&ipxs,sizeof(ipxs))==-1)
  {
    n_printf("IPX: could not bind to network 0 in GetMyAddress: %s\n", strerror(errno));
    close( sock );
    return(-1);
  }
  
  len = sizeof(ipxs2);
  if (getsockname(sock,(struct sockaddr *)&ipxs2,&len) < 0) {
    n_printf("IPX: could not get socket name in GetMyAddress: %s\n", strerror(errno));
    close( sock );
    return(-1);
  }

  *((unsigned long *)MyAddress) = ipxs2.sipx_network;
  for (i = 0; i < 6; i++) {
    MyAddress[4+i] = ipxs2.sipx_node[i];
  }
  n_printf("IPX: using network address %02X%02X%02X%02X\n",
    MyAddress[0], MyAddress[1], MyAddress[2], MyAddress[3] );
  n_printf("IPX: using node address of %02X%02X%02X%02X%02X%02X\n",
    MyAddress[4], MyAddress[5], MyAddress[6], MyAddress[7],
    MyAddress[8], MyAddress[9] );
  close( sock );
  return(0);
}
  
void
InitIPXFarCallHelper(void)
{
#if 0
  u_char *ptr;
#endif
  char value[40];
  int addressTemp[6];
  int numFields;
  int ccode;

  ccode = GetMyAddress();
  if( ccode ) {
    n_printf("IPX: cannot initialize MyAddress\n");
    n_printf("IPX: trying to read network and node address from DOS.INI\n");
    
    /* take some time here to read my address from the DOS.INI file */
    GetValueFromIniFile(IPX_DOS_INI_PATH, "ipx", "network", value );
    if( strlen( value )==0 ) {
      n_printf("IPX: cannot read IPX network address from dos.ini file\n");
      n_printf("IPX: please create an [IPX] section in your dos.ini file\n");
      n_printf("IPX: and add the line NETWORK=xxxxxxxx, where xxxxxxxx\n");
      n_printf("IPX: is your IPX network number.\n");
    } else {
      numFields = sscanf( value, "%2x%2x%2x%2x",
        &addressTemp[0], &addressTemp[1], &addressTemp[2], &addressTemp[3] );
      if( numFields != 4 ) {
        n_printf("IPX: invalid IPX network address read from dos.ini\n");
        n_printf("IPX: please create an [IPX] section in your dos.ini file\n");
        n_printf("IPX: and add the line NODE=xxxxxxxxxxxx, where xxxxxxxxxxxx\n");
        n_printf("IPX: is your IPX node number.\n");
      } else {
        MyAddress[0] = addressTemp[0];
        MyAddress[1] = addressTemp[1];
        MyAddress[2] = addressTemp[2];
        MyAddress[3] = addressTemp[3];
      }
    }
    
    GetValueFromIniFile(IPX_DOS_INI_PATH, "ipx", "node", value );
    if( strlen( value )==0 ) {
      n_printf("IPX: cannot read IPX node address from dos.ini file\n");
    } else {
      numFields = sscanf( value, "%2x%2x%2x%2x%2x%2x",
        &addressTemp[0], &addressTemp[1], &addressTemp[2], &addressTemp[3], 
        &addressTemp[4], &addressTemp[5] );
      if( numFields != 6 ) {
        n_printf("IPX: invalid IPX node address read from dos.ini\n");
      } else {
        MyAddress[4] = addressTemp[0];
        MyAddress[5] = addressTemp[1];
        MyAddress[6] = addressTemp[2];
        MyAddress[7] = addressTemp[3];
        MyAddress[8] = addressTemp[4];
        MyAddress[9] = addressTemp[5];
      }
    }
  
    n_printf("IPX: using network address %02X%02X%02X%02X\n",
      MyAddress[0], MyAddress[1], MyAddress[2], MyAddress[3] );
    n_printf("IPX: using node address of %02X%02X%02X%02X%02X%02X\n",
      MyAddress[4], MyAddress[5], MyAddress[6], MyAddress[7],
      MyAddress[8], MyAddress[9] );
  }
   
#if 0
  ptr = (u_char *) IPX_ADD;
  *ptr++ = 0x50;		/* push ax - FarCallHandler removes this before returning */
  *ptr++ = 0xb0;		/* mov al, 7a */
  *ptr++ = 0x7a;
  *ptr++ = 0xcd;		/* int DOS_HELPER_INT (0xe6) */
  *ptr++ = DOS_HELPER_INT;
  *ptr++ = 0xcb;		/* far ret */

  ESRPopRegistersReturn.segment = ((int) ptr) >> 4;
  ESRPopRegistersReturn.offset = ((int) ptr) & 0xf;
  *ptr++ = 0x07;		/* pop es, ds, bp, di, si, dx, cx, bx, ax */
  *ptr++ = 0x1f;
  *ptr++ = 0x5d;
  *ptr++ = 0x5f;
  *ptr++ = 0x5e;
  *ptr++ = 0x5a;
  *ptr++ = 0x59;
  *ptr++ = 0x5b;
  *ptr++ = 0x58;
  *ptr++ = 0xcb;		/* far return */

  ESRPopRegistersIRet.segment = ((int) ptr) >> 4;
  ESRPopRegistersIRet.offset = ((int) ptr) & 0xf;
  *ptr++ = 0x07;		/* pop es, ds, bp, di, si, dx, cx, bx, ax */
  *ptr++ = 0x1f;
  *ptr++ = 0x5d;
  *ptr++ = 0x5f;
  *ptr++ = 0x5e;
  *ptr++ = 0x5a;
  *ptr++ = 0x59;
  *ptr++ = 0x5b;
  *ptr++ = 0x58;
  *ptr++ = 0xcf;		/* iret */
#else
  {
    long i = (long)bios_IPX_PopRegistersReturn - (long)bios_f000;
    i += BIOSSEG << 4;
    ESRPopRegistersReturn.segment = i >> 4;
    ESRPopRegistersReturn.offset = i & 0xf;
    i = (long)bios_IPX_PopRegistersIRet - (long)bios_f000;
    i += BIOSSEG << 4;
    ESRPopRegistersIRet.segment = i >> 4;
    ESRPopRegistersIRet.offset = i & 0xf;
    i = (long)bios_IPX_FarCall - (long)bios_f000;
    i += BIOSSEG << 4;
    ESRFarCall.segment = i >> 4;
    ESRFarCall.offset = i & 0xf;
  }
#endif
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
  _regs.edi = IPX_OFF;
  _regs.es = IPX_SEG;
  n_printf("IPX: request for IPX far call handler address %lx:%lx\n",
	   (unsigned long)_regs.es, (unsigned long)_regs.edi);
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
  sk->listenList.segment = 0;
  sk->listenList.offset = 0;
  sk->listenCount = 0;
  sk->AESList.segment = 0;
  sk->AESList.offset = 0;
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

static u_short SwapInt(u_short value)
{
  u_short temp;

  temp = (value & 0xff) << 8;
  return (temp + (value >> 8));
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
      n_printf("%X: ", (int)memptr);
    }
    n_printf("%02X ", *memptr++);
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
    n_printf("Link             %04X:%04X\n", ECB->Link.segment, ECB->Link.offset);
    n_printf("ESR              %04X:%04X\n", ECB->ESRAddress.segment, ECB->ESRAddress.offset);
    n_printf("InUseFlag        %02x\n", ECB->InUseFlag);
    n_printf("CompletionCode   %02x\n", ECB->CompletionCode);
    n_printf("ECBSocket	     %04x\n", SwapInt(ECB->ECBSocket));
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
      n_printf("Frag[%d].Address	 %04X:%04X\n", i,
        ECB->FragTable[i].Address.segment,
        ECB->FragTable[i].Address.offset);
    }
  }
}

static void printIPXHeader(IPXPacket_t * IPXHeader)
{
  if (debug_level('n')) {
    n_printf("--IPX Header (dump)--\n");
    dumpBytes((u_char *) IPXHeader, 30);
    n_printf("--IPX Header --\n");
    n_printf("Checksum         %04X\n", IPXHeader->Checksum);
    n_printf("Length           %d\n", SwapInt(IPXHeader->Length));
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

static u_char IPXOpenSocket(u_short port, u_short * newPort)
{
  int sock;			/* sock here means Linux socket handle */
  int opt;
  struct sockaddr_ipx ipxs;
  int len;
  struct sockaddr_ipx ipxs2;

  /* DANG_FIXTHIS - do something with longevity flag */
  port = SwapInt(port);
  n_printf("IPX: open socket %x\n", port);
  if (port != 0 && ipx_find_socket(port) != NULL) {
    n_printf("IPX: socket %x already open.\n", port);
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
  ipxs.sipx_port = htons(port);

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
      port = htons(ipxs2.sipx_port);
      n_printf("IPX: opened dynamic socket %04x\n", port);
    }
  }
  
  /* if we successfully bound to this port, then record it */
  ipx_insert_socket(port, /* PSP */ 0, sock);
  n_printf("IPX: successfully opened socket %04x\n", port);
  *newPort = htons(port);
  return (RCODE_SUCCESS);
}

static u_char IPXCloseSocket(u_short port)
{
  ipx_socket_t *mysock;
  far_t ECBPtr;

  /* see if this socket is actually open */
  port = SwapInt(port);
  n_printf("IPX: close socket %x\n", port);
  mysock = ipx_find_socket(port);
  if (mysock == NULL) {
    n_printf("IPX: close of unopened socket\n");
    return (RCODE_SOCKET_NOT_OPEN);
  }
  /* cancel all pending events on this socket */
  n_printf("IPX: canceling all listen events on socket %x\n", port);
  ECBPtr = mysock->listenList;
  while (ECBPtr.segment | ECBPtr.offset) {
    if (IPXCancelEvent(ECBPtr) != RCODE_SUCCESS)
      return RCODE_CANNOT_CANCEL_EVENT;
    ECBPtr = mysock->listenList;
  }
  n_printf("IPX: canceling all AES events on socket %x\n", port);
  ECBPtr = mysock->AESList;
  while (ECBPtr.segment | ECBPtr.offset) {
    if (IPXCancelEvent(ECBPtr) != RCODE_SUCCESS)
      return RCODE_CANNOT_CANCEL_EVENT;
    ECBPtr = mysock->AESList;
  }
  /* now close the file descriptor for the socket, and free it */
  n_printf("IPX: closing file descriptor on socket %x\n", port);
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
    memptr = (u_char *) ((ECB->FragTable[i].Address.segment << 4) +
			 ECB->FragTable[i].Address.offset);
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

static void PrepareForESR(int iretFlag, ECB_t * ECB, far_t ECBPtr, u_char AXVal)
{
  unsigned char *ssp;
  unsigned long sp;

  IPXRunning++;
  ssp = (unsigned char *)(REG(ss)<<4);
  sp = (unsigned long) LWORD(esp);

  /* push all registers */
  pushw(ssp, sp, LWORD(eax));
  pushw(ssp, sp, LWORD(ebx));
  pushw(ssp, sp, LWORD(ecx));
  pushw(ssp, sp, LWORD(edx));
  pushw(ssp, sp, LWORD(esi));
  pushw(ssp, sp, LWORD(edi));
  pushw(ssp, sp, LWORD(ebp));
  pushw(ssp, sp, LWORD(ds));
  pushw(ssp, sp, LWORD(es));

  /* push far return address for ESRPopRegisters... */
  if (iretFlag) {
    pushw(ssp, sp, ESRPopRegistersIRet.segment);
    pushw(ssp, sp, ESRPopRegistersIRet.offset);
  }
  else {
    pushw(ssp, sp, ESRPopRegistersReturn.segment);
    pushw(ssp, sp, ESRPopRegistersReturn.offset);
  }

  LWORD(esp) -= 22;
  
  _regs.cs = ECB->ESRAddress.segment;
  _regs.eip = ECB->ESRAddress.offset;
  _regs.es = ECBPtr.segment;
  _regs.esi = ECBPtr.offset;
  _regs.eax = AXVal;

  n_printf("IPX: setup for ESR address at %04X:%04lX\n",
	   _regs.cs, _regs.eip);
  n_printf("IPX: ES:SI = %04X:%04lX, AX = %X, VIF = %d\n",
	   _regs.es, _regs.esi, LWORD(eax), LWORD(eflags) & VIF);
}

static u_char IPXSendPacket(far_t ECBPtr)
{
  ECB_t *ECB;
  IPXPacket_t *IPXHeader;
  u_char data[MAX_PACKET_DATA];
  struct sockaddr_ipx ipxs;
  int dataLen;
  ipx_socket_t *mysock;

  ECB = (ECB_t *) ((ECBPtr.segment << 4) + ECBPtr.offset);
  printECB(ECB);
  dataLen = GatherFragmentData(data, ECB);
  if (dataLen == -1) {
    ECB->InUseFlag = IU_ECB_FREE;
    ECB->CompletionCode = CC_FRAGMENT_ERROR;
    return RCODE_ECB_NOT_IN_USE;	/* FIXME - what is to return here?? */;
  }
  IPXHeader = (IPXPacket_t *) ((ECB->FragTable[0].Address.segment << 4)
			       + ECB->FragTable[0].Address.offset);
  /* for a complete emulation, we need to fill in fields in the */
  /* send packet header */
  /* first field is an IPX convention, not really a checksum */
  IPXHeader->Checksum = 0xffff;
  IPXHeader->Length = SwapInt(dataLen + 30);	/* in network order */
  memcpy(&IPXHeader->Source, MyAddress, 10);
  *(u_short *) & IPXHeader->Source.Socket = ECB->ECBSocket;
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
  mysock = ipx_find_socket(htons(ECB->ECBSocket));
  if (mysock == NULL) {
    /* DANG_FIXTHIS - should do a bind, send, close here */
    /* DOS IPX allows sending on unopened sockets */
    n_printf("IPX: send to unopened socket %04x\n", htons( ECB->ECBSocket ));
    return RCODE_SOCKET_NOT_OPEN;
  }
  if (sendto(mysock->fd, (void *) &data, dataLen, 0,
	     (struct sockaddr *) &ipxs, sizeof(ipxs)) == -1) {
    n_printf("IPX: error sending packet: %s\n", strerror(errno));
    ECB->InUseFlag = IU_ECB_FREE;
    ECB->CompletionCode = CC_HARDWARE_ERROR;
    return RCODE_CANNOT_FIND_ROUTE;	/* FIXME - what is to return here?? */
  }
  else {
    ECB->InUseFlag = IU_ECB_FREE;
    ECB->CompletionCode = CC_SUCCESS;
    n_printf("IPX: successfully sent packet!\n");
  }
  /* now call the ESR for the ECB */
  /* bit of trickiness here - I will let the ESR, if any, return to */
  /* the return address from the original far call to IPX, instead of */
  /* to my helper routine. */
  if ((ECB->ESRAddress.segment | ECB->ESRAddress.offset) != 0) {
    PrepareForESR(0 /* not an iret, just a normal return */ ,
		  ECB, ECBPtr, ESR_CALLOUT_IPX);
  }
  return RCODE_SUCCESS;
}

static u_char IPXListenForPacket(far_t ECBPtr)
{
  ECB_t *ECB;
  ipx_socket_t *mysock;

  ECB = (ECB_t *) ((ECBPtr.segment << 4) + ECBPtr.offset);
  printECB(ECB);
  /* see if the socket is open for this ECB */
  mysock = ipx_find_socket(htons(ECB->ECBSocket));
  if (mysock == NULL) {
    n_printf("IPX: listen on unopened socket\n");
    ECB->InUseFlag = IU_ECB_FREE;
    ECB->CompletionCode = CC_SOCKET_NOT_OPEN;
    return (RCODE_SOCKET_NOT_OPEN);
  }

  ECB->InUseFlag = IU_ECB_LISTENING;
  ECB->CompletionCode = CC_SUCCESS;
  /* now put this ECB on the listen list for this socket */
  ECB->Link = mysock->listenList;
  mysock->listenList = ECBPtr;
  mysock->listenCount++;

  n_printf("IPX: successfully posted listen packet on socket %x!\n",
	   mysock->socket);
  return (RCODE_SUCCESS);
}

static u_char IPXScheduleEvent(far_t ECBPtr, u_char inUseCode, u_short delayTime)
{
  ECB_t *ECB;
  ipx_socket_t *mysock;

  ECB = (ECB_t *) ((ECBPtr.segment << 4) + ECBPtr.offset);
  printECB(ECB);
  /* see if the socket is open for this ECB */
  mysock = ipx_find_socket(htons(ECB->ECBSocket));
  if (mysock == NULL) {
    n_printf("IPX: AES event on unopened socket\n");
    ECB->InUseFlag = IU_ECB_FREE;
    ECB->CompletionCode = CC_SOCKET_NOT_OPEN;
    return (RCODE_SOCKET_NOT_OPEN);
  }

  ECB->InUseFlag = inUseCode;
  ECB->CompletionCode = CC_SUCCESS;
  /* now put this ECB on the AES list for this socket */
  ECB->Link = mysock->AESList;
  ((AESECB_t *) ECB)->TimeLeft = delayTime;
  mysock->AESList = ECBPtr;
  mysock->AESCount++;

  n_printf("IPX: successfully posted AES event on socket %x!\n",
	   mysock->socket);
  n_printf("IPX: AES delay time %d!\n", ((AESECB_t *) ECB)->TimeLeft);
  return (RCODE_SUCCESS);
}

static u_char IPXCancelEvent(far_t ECBPtr)
{
  ECB_t *ECB;
  ipx_socket_t *mysock;
  ECB_t *prevECB;
  far_t ECBList;

  ECB = (ECB_t *) ((ECBPtr.segment << 4) + ECBPtr.offset);
  /* if it was a listen, cancel it */
  if (ECB->InUseFlag == IU_ECB_LISTENING ||
      ECB->InUseFlag == IU_ECB_IPX_WAITING ||
      ECB->InUseFlag == IU_ECB_AES_WAITING) {
    /* look up the socket for this ECB */
    mysock = ipx_find_socket(htons(ECB->ECBSocket));
    if (mysock == NULL) {
      n_printf("IPX: cancel on unopened socket\n");
      ECB->InUseFlag = IU_ECB_FREE;
      ECB->CompletionCode = CC_SOCKET_NOT_OPEN;
      return (RCODE_SOCKET_NOT_OPEN);
    }
    n_printf("IPX: canceling event on socket %x\n",
	     mysock->socket);
    /* now see if this ECB is actually on the list for this socket */
    if (ECB->InUseFlag == IU_ECB_LISTENING) {
      /* check the listenList */
      ECBList = mysock->listenList;
      n_printf("IPX: cancel a listen event from %d events\n",
	       mysock->listenCount);
      prevECB = (ECB_t *) (&mysock->listenList);
    }
    else {
      /* check the AESList */
      ECBList = mysock->AESList;
      n_printf("IPX: cancel an AES event from %d events\n",
	       mysock->AESCount);
      prevECB = (ECB_t *) (&mysock->AESList);
    }
    /* this next cast is somewhat evil.  If the first item on the */
    /* list is the one removed, then change the head pointer for */
    /* the list as if it were a ECB.Link field. */
    n_printf("IPX: scanning ECBList for match\n");
    while (ECBList.segment | ECBList.offset) {
      /* see if the list pointer equals the ECB we are canceling */
      n_printf("IPX: ECBPtr = %x:%x, ECBList = %x:%x\n",
	       ECBPtr.segment, ECBPtr.offset,
	       ECBList.segment, ECBList.offset);
      if (ECBList.segment == ECBPtr.segment &&
	  ECBList.offset == ECBPtr.offset) {
	/* remove it from the list */
	prevECB->Link = ECB->Link;
	if (ECB->InUseFlag == IU_ECB_LISTENING) {
	  mysock->listenCount--;
	}
	else {
	  mysock->AESCount--;
	}
	ECB->InUseFlag = IU_ECB_FREE;
	ECB->CompletionCode = CC_EVENT_CANCELED;
	n_printf("IPX: successfully canceled event!\n");
	return (RCODE_SUCCESS);
      }
      prevECB = (ECB_t *) ((ECBList.segment << 4) + ECBList.offset);
      ECBList = prevECB->Link;
    }
    n_printf("IPX: ECB was not in use.\n");
    return (RCODE_ECB_NOT_IN_USE);
  }
  else {
    if (ECB->InUseFlag == IU_ECB_FREE) {
      return (RCODE_ECB_NOT_IN_USE);
    }
  }
  return (RCODE_CANNOT_CANCEL_EVENT);
}

void AESTimerTick(void)
{
  ipx_socket_t *mysock;
  far_t ECBPtr;
  AESECB_t *ECB;
  int done;

  /* run through the socket list, and process any pending AES events */
  mysock = ipx_socket_list;
  done = 0;
  while (mysock && !done) {
    /* see if this socket has any AES events pending */
    ECBPtr = mysock->AESList;
    while ((ECBPtr.segment | ECBPtr.offset) && !done) {
      ECB = (AESECB_t *) ((ECBPtr.segment << 4) + ECBPtr.offset);
      if (ECB->TimeLeft > 0) {
	ECB->TimeLeft--;
	n_printf("IPX: AES timer decremented to %d on ECB at %x:%x\n",
		 ECB->TimeLeft, ECBPtr.segment, ECBPtr.offset);
      }
      ECBPtr = ECB->Link;
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
    memptr = (u_char *) ((ECB->FragTable[i].Address.segment << 4) +
			 ECB->FragTable[i].Address.offset);
    n_printf("IPX: filling fragment %d at %x:%x, length %d with %d data left\n",
	     i, ECB->FragTable[i].Address.segment,
	     ECB->FragTable[i].Address.offset, nextFragLen,
	     dataLeftCount);
    if (i == 0) {
      /* subtract off IPX header size from first fragment */
      nextFragLen -= 30;
      /* also, fill out IPX header */
      /* use data from sipx to fill out source address */
      IPXHeader = (IPXPacket_t *) memptr;
      IPXHeader->Checksum = 0xFFFF;
      IPXHeader->Length = SwapInt(size + 30); /* in network order */
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
	   ECB->FragmentCount, dataLeftCount);
  return (dataLeftCount);
}

static int IPXReceivePacket(ipx_socket_t * s)
{
  struct sockaddr_ipx ipxs;
  char buffer[MAX_PACKET_DATA];
  ECB_t *ECB;
  far_t ECBPtr;
  int size, sz;
  int ESRFired = 0;

  sz = sizeof(ipxs);
  size = recvfrom(s->fd, buffer, sizeof(buffer), 0,
		  (struct sockaddr *) &ipxs, &sz);
  n_printf("IPX: received %d bytes of data\n", size);
  if (size > 0 && s->listenCount) {
    ECBPtr = s->listenList;
    ECB = (ECB_t *) ((ECBPtr.segment << 4) + ECBPtr.offset);
    sz = ScatterFragmentData(size, buffer, ECB, &ipxs);
    IPXCancelEvent(ECBPtr);
    /* if there was packet left over, then report a fragment error */
    if (sz) {
      ECB->CompletionCode = CC_FRAGMENT_ERROR;
      /* DANG_FIXTHIS - should remove this override, linux IPX stack returns bad sizes, but we ignore this here */
      ECB->CompletionCode = CC_SUCCESS;
    }
    else {
      ECB->CompletionCode = CC_SUCCESS;
    }
    /* now call the ESR for this packet */
    if ((ECB->ESRAddress.segment | ECB->ESRAddress.offset) != 0) {
      PrepareForESR(0 /* not an iret, just a normal return */ ,
		    ECB, ECBPtr, ESR_CALLOUT_IPX);
      ESRFired = 1;
    }
  }
  return (ESRFired);
}

static int IPXCheckForAESReady(void)
{
  ipx_socket_t *s;
  far_t ECBPtr;
  AESECB_t *ECB;
  int ESRFired = 0;
  u_char waitType;
  u_short rcode;

  s = ipx_socket_list;
  while (s != NULL && !ESRFired) {
    if (s->AESCount) {
      ECBPtr = s->AESList;
      while ((ECBPtr.segment | ECBPtr.offset) && !ESRFired) {
	ECB = (AESECB_t *) ((ECBPtr.segment << 4) + ECBPtr.offset);
	if (ECB->TimeLeft == 0) {
	  /* DANG_FIXTHIS - this architecture currently only supports firing of one AES event per call */
	  n_printf("IPX: AES event ready on ECB at %x:%x\n",
		   ECBPtr.segment, ECBPtr.offset);
	  printECB((ECB_t *) ECB);

	  /* first, save off the type of event, either AES or IPX */
	  waitType = ECB->InUseFlag;

	  /* now remove the ECB from the AES list */
	  rcode = IPXCancelEvent(ECBPtr);
	  if (rcode == RCODE_SUCCESS) {
	    ECB->CompletionCode = CC_SUCCESS;
	  }
	  /* now setup to call the ESR for this event */
	  if ((ECB->ESRAddress.segment | ECB->ESRAddress.offset) != 0) {
	    PrepareForESR(0 /* not an iret, just a normal return */ ,
			  (ECB_t *) ECB, ECBPtr, ESR_CALLOUT_AES);
	    ESRFired = 1;
	  }
	}
	ECBPtr = ECB->Link;
      }
    }
    s = s->next;
  }
  return (ESRFired);
}

static int check_ipx_ready(fd_set * set)
{
  ipx_socket_t *s;
  int ESRFired = 0;

  s = ipx_socket_list;
  while (s != NULL) {
    /* DANG_FIXTHIS - for now, just pick up one listen per poll, because we can only set up one ESR callout per relinquish */
    if (FD_ISSET(s->fd, set)) {
      break;
    }
    s = s->next;
  }
  if (s != NULL) {
    ESRFired = IPXReceivePacket(s);
  }
  return (ESRFired);
}

static void IPXRelinquishControl(void)
{
  /* DOS program has given us a time slice */
  /* let's use this as an opportunity to poll outstanding listens */
  fd_set fds;
  int selrtn;
  struct timeval timeout;
  int ESRFired = 0;

  if(IPXRunning) return;

  FD_ZERO(&fds);

  ipx_fdset(&fds);

  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  n_printf("IPX: starting repeated select\n");


  do {
      selrtn = select(255, &fds, NULL, NULL, &timeout);
  } while (selrtn == -1 && errno == EINTR);

  switch (selrtn) {
  case 0:			/* none ready */
    /*			n_printf("IPX: no receives ready\n"); */
    break;

  case -1:			/* error (not EINTR) */
    error("bad ipc_select: %s\n", strerror(errno));
    break;

  default:			/* has at least 1 descriptor ready */
    n_printf("IPX: receive ready, checking fd\n");
    ESRFired = check_ipx_ready(&fds);
    break;
  }

  if (!ESRFired) {
    /*		n_printf("IPX: checking AES events\n"); */
    ESRFired = IPXCheckForAESReady();
  }

}

int
IPXFarCallHandler(void)
{
  u_short port;			/* port here means DOS IPX socket */
  u_short newPort;
  ECB_t *ECB;
  unsigned short *timerticks;
  u_char *AddrPtr;
  far_t ECBPtr;
  unsigned long network, *networkPtr;
  int hops, ticks;
  unsigned char *ssp;
  unsigned long sp;

  ssp = (unsigned char *)(REG(ss)<<4);
  sp = (unsigned long) LWORD(esp);

  LWORD(eax) = popw(ssp, sp);
  LWORD(esp) += 2;
  n_printf("IPX: request number 0x%lX\n", _regs.ebx);
  switch (LWORD(ebx)) {
  case IPX_OPEN_SOCKET:
    port = LWORD(edx);
    LO(ax) = IPXOpenSocket(port, &newPort);
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
    networkPtr = (unsigned long *)(((_regs.es & 0xffff)<<4) + LWORD(esi));
    network = *networkPtr;
    n_printf("IPX: GetLocalTarget for network %08lx\n", network );
    if( network==0 || network== *((unsigned long *)&MyAddress[0]) ) {
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
  case IPX_SEND_PACKET:
    ECBPtr.segment = _regs.es & 0xffff;
    ECBPtr.offset = LWORD(esi);
    n_printf("IPX: send packet ECB at %x:%x\n",
	     ECBPtr.segment, ECBPtr.offset);
    LO(ax) = IPXSendPacket(ECBPtr);
    break;
  case IPX_LISTEN_FOR_PACKET:
    ECBPtr.segment = _regs.es & 0xffff;
    ECBPtr.offset = LWORD(esi);
    n_printf("IPX: listen for packet ECB at %x:%x\n",
	     ECBPtr.segment, ECBPtr.offset);
    /* put this packet on the queue of listens for this socket */
    LO(ax) = IPXListenForPacket(ECBPtr);
    break;
  case IPX_SCHEDULE_IPX_EVENT:
    ECBPtr.segment = _regs.es & 0xffff;
    ECBPtr.offset = LWORD(esi);
    n_printf("IPX: schedule IPX event for ECB at %x:%x\n",
	     ECBPtr.segment, ECBPtr.offset);
    /* put this packet on the queue of AES events for this socket */
    LO(ax) = IPXScheduleEvent(ECBPtr, IU_ECB_IPX_WAITING,
			      LWORD(eax));
    break;
  case IPX_CANCEL_EVENT:
    ECBPtr.segment = _regs.es & 0xffff;
    ECBPtr.offset = LWORD(esi);
    n_printf("IPX: cancel event for ECB at %x:%x\n",
	     ECBPtr.segment, ECBPtr.offset);
    LO(ax) = IPXCancelEvent(ECBPtr);
    break;
  case IPX_SCHEDULE_AES_EVENT:
    ECB = (ECB_t *) (((_regs.es & 0xffff) << 4) + LWORD(esi));
    ECBPtr.segment = _regs.es & 0xffff;
    ECBPtr.offset = LWORD(esi);

    n_printf("IPX: schedule AES event ECB at %x\n", (unsigned int)ECB);
    n_printf("IPX: es:si = %x:%x\n", (unsigned int)(_regs.es & 0xffff),
      LWORD(esi));

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
    AddrPtr = (u_char *) (((_regs.es & 0xffff) << 4) + LWORD(esi));
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
    break;
  }
  return 1;
}

void IPXEndCall(void) {
	if(--IPXRunning < 0) IPXRunning = 0; /* Just incase */
    	n_printf("IPX: ESR Ended\n");
	if(!IPXRunning) pic_request(PIC_IPX); /* Ask for more */
	
}

void IPXCallRel(void) {

  if(in_dpmi && !in_dpmi_dos_int)
    fake_pm_int();

  /* push iret frame on _SS:_SP. At F000:2146 (bios.S) we get an
   * iret and return to _CS:_IP */
  fake_int(LWORD(cs), LWORD(eip));

  /* push iret frame. At F000:20F7 (bios.S) we get an
   * iret and return to F000:2140 for EOI */
  fake_int(BIOSSEG, EOI_OFF);

  /* push all 16-bit regs plus _DS,_ES. At F000:20F4 (bios.S) we find
   * 'pop es; pop ds; popa' */
  fake_pusha();

  /* push return address F000:20F4 */
  fake_call(BIOSSEG, POPA_IRET_OFF);	/* popa+iret */

  _regs.cs = ESRFarCall.segment;
  _regs.eip = ESRFarCall.offset;
}

/* ipx_close is called on DOSEMU shutdown */
void
ipx_close(void)
{
  ipx_socket_t *s;

  /* run through the socket list and close each socket */
  s = ipx_socket_list;
  while (s != NULL) {
    IPXCloseSocket(SwapInt(s->socket));
    s = ipx_socket_list;
  }
}
#endif

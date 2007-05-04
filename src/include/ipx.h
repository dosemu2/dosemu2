/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* ipx.h header file for IPX for the DOS emulator
 * 		Tim Bird, tbird@novell.com
 */

#include "config.h"
#ifdef USING_NET
#ifndef IPXDMU_H
#define IPXDMU_H

/* commment out the next line to turn off IPX emulation */
#if 1
#define IPX 1
#endif

#if 0
#include "cpu.h"
#endif
#include <sys/types.h>

#define INT2F_DETECT_IPX		0x7A00

#define IPX_OPEN_SOCKET					0x0000
#define IPX_CLOSE_SOCKET				0x0001
#define IPX_GET_LOCAL_TARGET			0x0002
#define IPX_SEND_PACKET					0x0003
#define IPX_LISTEN_FOR_PACKET			0x0004
#define IPX_SCHEDULE_IPX_EVENT			0x0005
#define IPX_CANCEL_EVENT				0x0006
#define IPX_SCHEDULE_AES_EVENT			0x0007
#define IPX_GET_INTERVAL_MARKER			0x0008
#define IPX_GET_INTERNETWORK_ADDRESS	0x0009
#define IPX_RELINQUISH_CONTROL			0x000A
#define IPX_DISCONNECT					0x000B
#define IPX_SHELL_HOOK					0x000C	/* undocumented */
#define IPX_GET_MAX_PACKET_SIZE			0x000D	/* undocumented */
#define IPX_CLEAR_SOCKET				0x000E	/* undocumented */
#define IPX_FAST_SEND					0x000F
#define IPX_GET_MEDIA_DATA_SIZE			0x001A	/* undocumented */

#define CC_SUCCESS			0x00
#define CC_EVENT_CANCELED	0xFC
#define CC_FRAGMENT_ERROR	0xFD
#define CC_PACKET_UNDELIVERABLE	0xFE
#define CC_SOCKET_NOT_OPEN  0xFF
#define CC_HARDWARE_ERROR	0xFF

#define LONG_LIVED			0xFF
#define SHORT_LIVED			0x00

#define ESR_CALLOUT_IPX		0xFF
#define ESR_CALLOUT_AES		0x00

#define RCODE_SUCCESS				0
#define RCODE_SOCKET_ALREADY_OPEN	0xff
#define RCODE_ECB_NOT_IN_USE		0xff
#define RCODE_SOCKET_NOT_OPEN       0xff
#define RCODE_SOCKET_TABLE_FULL		0xfe
#define RCODE_CANNOT_FIND_ROUTE         0xfa
#define RCODE_CANNOT_CANCEL_EVENT	0xf9

#define IU_ECB_FREE			0
#define IU_ECB_SENDING		0xff
#define IU_ECB_LISTENING	0xfe
#define IU_ECB_IPX_WAITING	0xfd
#define IU_ECB_AES_WAITING	0xfc

#define IPX_ADDRESS_LENGTH	12

typedef struct IPXAddressStruct {
  u_char Network[4];
  u_char Node[6];
  u_char Socket[2];
} __attribute__((packed)) IPXAddress_t;

typedef struct IPXPacketStructure {
  u_short Checksum;
  u_short Length;
  u_char TransportControl;
  u_char PacketType;
  IPXAddress_t Destination;
  IPXAddress_t Source;
} __attribute__((packed)) IPXPacket_t;

typedef struct FragStruct {
  far_t Address;
  u_short Length;
} __attribute__((packed)) Frag_t;

typedef struct ECBStruct {
  far_t Link;
  far_t ESRAddress;	/* 0 = no callback */
  u_char InUseFlag;
  u_char CompletionCode;
  u_short ECBSocket;
  /* following 2 fields are not for user use */
  u_char IPXWorkspace[4];
  u_char DriverWorkspace[12];
  /* not used because Linux IPX routes stuff */
  u_char ImmediateAddress[6];
  u_short FragmentCount;
  Frag_t FragTable[4];
} __attribute__((packed)) ECB_t;

typedef struct AESECBStruct {
  far_t Link;
  far_t ESRAddress;	/* 0 = no callback */
  u_char InUseFlag;
  u_char CompletionCode;
  u_short ECBSocket;
  /* following 2 fields are not for user use */
  u_short TimeLeft;
} __attribute__((packed)) AESECB_t;

typedef struct ipx_socket_struct {
  struct ipx_socket_struct *next;
  far_t listenList;
  int listenCount;
  far_t AESList;
  int AESCount;
  u_short socket;
  u_short PSP;
  int fd;
}

ipx_socket_t;

extern void ipx_init(void);
extern int IPXInt2FHandler(void);
extern void AESTimerTick(void);
extern int ipx_receive(int ilevel);
extern int IPXCheckForAESReady(int ilevel);
extern void ipx_send_esr_call(void);
extern int IPXGetLocalTarget( unsigned long network, int *hops, int *ticks );
extern void ipx_close(void);

#endif /* IPX_H */
#endif /* USING_NET */

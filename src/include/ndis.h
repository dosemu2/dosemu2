/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2020-2026 https://github.com/robert-j
 *
 * NDIS 2.0 / 2.0.1 Interface Definitions
 *
 * Generated from the Network Driver Interface Specification (NDIS),
 * October 8, 1990, by 3Com Corporation and Microsoft Corporation:
 *
 * https://www.edm2.com/index.php/3Com/Microsoft_LAN_Manager_Network_Driver_Interface_Specification_Version_2.0.1_Final
 *
 * This header covers DOS real-mode NDIS only. OS/2 and Token Ring
 * (802.5) specifics have been omitted.
 *
 * Imported into dosemu2 from https://github.com/robert-j/pktndis and
 * extended with the definitions of the system request function, of the
 * MAC status word and of the packet filter, which the original header
 * did not cover.
 */

#ifndef NDIS_H
#define NDIS_H

#ifndef __ASSEMBLER__

#include <stdint.h>

#pragma pack(2)

/*
 * Basic types matching 16-bit DOS layout
 */
typedef uint8_t   UCHAR;
typedef uint16_t  USHORT;
typedef uint32_t  ULONG;
typedef uint32_t  FARPTR;       /* seg:off pointer in DOS memory */

#define MK_FARPTR(seg, off) (((uint32_t)(uint16_t)(seg) << 16) | (uint16_t)(off))
#define FARPTR_SEG(p) ((uint16_t)((p) >> 16))
#define FARPTR_OFF(p) ((uint16_t)(p))

#endif /* __ASSEMBLER__ */

/*
 * The constants below are usable from assembly too, the DOS-side parts
 * of the driver and of its test harness rely on that.
 */

#define NAME_LEN  16
#define ADDR_LEN  16

#define NDIS_VERSION_MAJOR  2
#define NDIS_VERSION_MINOR  0

/* --------------------------------------------------------------------------
 * Error / return codes (NDIS_)
 * -------------------------------------------------------------------------- */

#define NDIS_SUCCESS                        0x0000
#define NDIS_ERR_WAIT_FOR_RELEASE           0x0001
#define NDIS_ERR_REQUEST_QUEUED             0x0002
#define NDIS_ERR_FRAME_NOT_RECOGNIZED       0x0003
#define NDIS_ERR_FRAME_REJECTED             0x0004
#define NDIS_ERR_FORWARD_FRAME              0x0005
#define NDIS_ERR_OUT_OF_RESOURCE            0x0006
#define NDIS_ERR_INVALID_PARAMETER          0x0007
#define NDIS_ERR_INVALID_FUNCTION           0x0008
#define NDIS_ERR_NOT_SUPPORTED              0x0009
#define NDIS_ERR_HARDWARE_ERROR             0x000A
#define NDIS_ERR_TRANSMIT_ERROR             0x000B
#define NDIS_ERR_NO_SUCH_DESTINATION        0x000C
#define NDIS_ERR_BUFFER_TOO_SMALL           0x000D
#define NDIS_ERR_ALREADY_STARTED            0x0020
#define NDIS_ERR_INCOMPLETE_BINDING         0x0021
#define NDIS_ERR_DRIVER_NOT_INITIALIZED     0x0022
#define NDIS_ERR_HARDWARE_NOT_FOUND         0x0023
#define NDIS_ERR_HARDWARE_FAILURE           0x0024
#define NDIS_ERR_CONFIGURATION_FAILURE      0x0025
#define NDIS_ERR_INTERRUPT_CONFLICT         0x0026
#define NDIS_ERR_INCOMPATIBLE_MAC           0x0027
#define NDIS_ERR_INITIALIZATION_FAILED      0x0028
#define NDIS_ERR_NO_BINDING                 0x0029
#define NDIS_ERR_NETWORK_DISCONNECTED       0x002A
#define NDIS_ERR_INCOMPATIBLE_OS_VERSION    0x002B
#define NDIS_ERR_ALREADY_REGISTERED         0x002C
#define NDIS_ERR_PATH_NOT_FOUND             0x002D
#define NDIS_ERR_INSUFFICIENT_MEMORY        0x002E
#define NDIS_ERR_INFO_NOT_FOUND             0x002F
#define NDIS_ERR_GENERAL_FAILURE            0x00FF

/* --------------------------------------------------------------------------
 * System request opcodes (SR_), passed to CommonChar.CcSysReq
 * -------------------------------------------------------------------------- */

#define SR_INITIATE_BIND                1   /* start binding (protocol) */
#define SR_BIND                         2   /* bind protocol to MAC */
#define SR_INITIATE_UNBIND              4   /* start unbinding (protocol) */
#define SR_UNBIND                       5   /* unbind protocol from MAC */

/* --------------------------------------------------------------------------
 * General request opcodes (GR_)
 * -------------------------------------------------------------------------- */

#define GR_INITIATE_DIAGNOSTICS         1   /* run-time hardware diagnostics */
#define GR_READ_ERROR_LOG               2   /* return adapter error log */
#define GR_SET_STATION_ADDRESS          3   /* set network address */
#define GR_OPEN_ADAPTER                 4   /* open the adapter */
#define GR_CLOSE_ADAPTER                5   /* close the adapter */
#define GR_RESET_MAC                    6   /* hardware reset */
#define GR_SET_PACKET_FILTER            7   /* set receive filter */
#define GR_ADD_MULTICAST_ADDRESS        8   /* add multicast address */
#define GR_DELETE_MULTICAST_ADDRESS     9   /* remove multicast address */
#define GR_UPDATE_STATISTICS            10  /* refresh MAC statistics */
#define GR_CLEAR_STATISTICS             11  /* clear MAC statistics */
#define GR_INTERRUPT_REQUEST            12  /* request hardware interrupt */
#define GR_SET_FUNCTIONAL_ADDRESS       13  /* set functional address */
#define GR_SET_LOOKAHEAD                14  /* set lookahead length */

/* --------------------------------------------------------------------------
 * Status indication opcodes (SI_)
 *
 * Note: these follow the NDIS DDK headers, in which InterruptStatus comes
 * before EndReset. The values printed in the specification differ.
 * -------------------------------------------------------------------------- */

#define SI_RING_STATUS                  0x0001  /* ring status change */
#define SI_ADAPTER_CHECK                0x0002  /* fatal adapter error */
#define SI_START_RESET                  0x0003  /* adapter reset started */
#define SI_INTERRUPT_STATUS             0x0004  /* async interrupt notification */
#define SI_END_RESET                    0x0005  /* adapter reset completed */

/* --------------------------------------------------------------------------
 * MAC service flags (SF_)
 * -------------------------------------------------------------------------- */

#define SF_BROADCAST                    0x00000001  /* broadcast supported */
#define SF_MULTICAST                    0x00000002  /* multicast supported */
#define SF_FUNCTIONAL                   0x00000004  /* functional addressing */
#define SF_PROMISCUOUS                  0x00000008  /* promiscuous mode */
#define SF_SOFT_ADDRESS                 0x00000010  /* settable station address */
#define SF_CURRENT_STATS                0x00000020  /* statistics always current */
#define SF_INITIATE_DIAGS               0x00000040  /* InitiateDiagnostics */
#define SF_LOOPBACK                     0x00000080  /* loopback supported */
#define SF_RECEIVE_CHAIN                0x00000100  /* ReceiveChain supported */
#define SF_SOURCE_ROUTING               0x00000200  /* source routing */
#define SF_RESET_MAC                    0x00000400  /* ResetMAC supported */
#define SF_OPEN_CLOSE                   0x00000800  /* Open/CloseAdapter */
#define SF_INTERRUPT_REQUEST            0x00001000  /* InterruptRequest */
#define SF_SOURCE_ROUTING_BRIDGE        0x00002000  /* source routing bridge */
#define SF_GDT_VIRTUAL_ADDR             0x00004000  /* GDT virtual addresses */
#define SF_MULTIPLE_XFER                0x00008000  /* multiple TransferData (v2.0.1) */
#define SF_FRAME_SIZE_ZERO              0x00010000  /* FrameSize=0 in lookahead (v2.0.1) */

/* --------------------------------------------------------------------------
 * Packet filter flags
 * -------------------------------------------------------------------------- */

#define FILTER_DIRECTED                 0x0001  /* directed/multicast/group */
#define FILTER_BROADCAST                0x0002  /* broadcast packets */
#define FILTER_PROMISCUOUS              0x0004  /* all packets on LAN */
#define FILTER_SOURCE_ROUTING           0x0008  /* source routing packets */

/* --------------------------------------------------------------------------
 * Module function flags
 * -------------------------------------------------------------------------- */

#define MFF_BIND_UPPER                  0x00000001
#define MFF_BIND_LOWER                  0x00000002
#define MFF_DYNAMICALLY_BOUND           0x00000004

/* --------------------------------------------------------------------------
 * Boundary protocol levels and interface types
 * -------------------------------------------------------------------------- */

#define UL_MAC                          1   /* upper boundary: MAC level */
#define UI_MAC                          1   /* upper boundary: MAC interface */

/* --------------------------------------------------------------------------
 * Protocol lower dispatch interface flags
 * -------------------------------------------------------------------------- */

#define PLD_NON_LLC                     0x00000001  /* handles non-LLC frames */
#define PLD_SPECIFIC_LSAP               0x00000002  /* handles specific LSAP */
#define PLD_NONSPECIFIC_LSAP            0x00000004  /* handles nonspecific LSAP */

/* --------------------------------------------------------------------------
 * MAC status word (MACSpecStat.MssStatus)
 *
 * The low 3 bits are an error code rather than a bit mask.
 * -------------------------------------------------------------------------- */

#define MAC_STATUS_MISSING              0x0000
#define MAC_STATUS_DIAG_ERR             0x0001
#define MAC_STATUS_CONFIG_ERROR         0x0002
#define MAC_STATUS_FAULT                0x0003
#define MAC_STATUS_SOFT_FAULT           0x0004
#define MAC_STATUS_OK                   0x0007
#define MAC_STATUS_BOUND                0x0008
#define MAC_STATUS_OPENED               0x0010
#define MAC_STATUS_DIAGNOSTICS          0x0020

/* value for the statistics fields that are not maintained */
#define NDIS_NOT_MAINTAINED             0xffffffff

/* initial value of the "Indicate" flag byte passed to the indications */
#define INDICATION_INIT                 0xff

/* --------------------------------------------------------------------------
 * PROTOCOL.INI parameter types
 * -------------------------------------------------------------------------- */

#define PARAM_TYPE_INT                  0
#define PARAM_TYPE_STRING               1

/* --------------------------------------------------------------------------
 * Protocol Manager request opcodes
 * -------------------------------------------------------------------------- */

#define PM_GET_PROTOCOL_MANAGER_INFO    1   /* get config memory image */
#define PM_REGISTER_MODULE              2   /* register module and bindings */
#define PM_BIND_AND_START               3   /* initiate binding */
#define PM_GET_PROTOCOL_MANAGER_LINKAGE 4   /* get ProtMan entry point */

/* --------------------------------------------------------------------------
 * Structures
 * -------------------------------------------------------------------------- */

#ifndef __ASSEMBLER__

/*
 * Common Characteristics Table
 *
 * Shared header for all NDIS modules (MAC drivers, protocols).
 */
typedef struct {
    USHORT  CcSize;                     /* Table size in bytes */
    UCHAR   CcNdisMajor;               /* NDIS major version (BCD) */
    UCHAR   CcNdisMinor;               /* NDIS minor version (BCD) */
    USHORT  CcReserved;                /* Reserved */
    UCHAR   CcModMajor;                /* Module major version (BCD) */
    UCHAR   CcModMinor;                /* Module minor version (BCD) */
    ULONG   CcModFunc;                 /* Module function flags */
    UCHAR   CcName[NAME_LEN];          /* Module name (ASCIIZ) */
    UCHAR   CcUprLevel;                /* Upper boundary protocol level */
    UCHAR   CcUprType;                 /* Upper boundary interface type */
    UCHAR   CcLwrLevel;                /* Lower boundary protocol level */
    UCHAR   CcLwrType;                 /* Lower boundary interface type */
    USHORT  CcModId;                    /* Module ID (set by ProtMan) */
    USHORT  CcModDS;                    /* Module data segment */
    FARPTR  CcSysReq;                  /* System request entry point */
    FARPTR  CcSChar;                   /* -> service-specific chars */
    FARPTR  CcSStat;                   /* -> service-specific status */
    FARPTR  CcUprDisp;                 /* -> upper dispatch table */
    FARPTR  CcLwrDisp;                 /* -> lower dispatch table */
    FARPTR  CcRsv0;                    /* Reserved (must be NULL) */
    FARPTR  CcRsv1;                    /* Reserved (must be NULL) */
} CommonChar;

/*
 * Multicast Address Entry
 */
typedef struct {
    UCHAR   mAddr[ADDR_LEN];           /* Multicast address */
} MCastAddr;

/*
 * Multicast Address Buffer
 */
#define NUM_MCADDRS 8
typedef struct {
    USHORT  McbMax;                     /* Maximum multicast addresses */
    USHORT  McbCnt;                     /* Current count */
    MCastAddr McbAddrs[NUM_MCADDRS];   /* Variable-length address array */
} MCastBuf;

/*
 * MAC Service-Specific Characteristics Table
 */
typedef struct {
    USHORT  MscSize;                    /* Table size in bytes */
    UCHAR   MscType[NAME_LEN];         /* MAC type name (ASCIIZ) */
    USHORT  MscStnAdrSz;               /* Station address length */
    UCHAR   MscPermStnAdr[ADDR_LEN];   /* Permanent station address */
    UCHAR   MscCurrStnAdr[ADDR_LEN];   /* Current station address */
    ULONG   MscCurrFncAdr;             /* Current functional address */
    FARPTR  MscMCp;                    /* -> multicast buffer */
    ULONG   MscLinkSpd;                /* Link speed (bits/sec) */
    ULONG   MscService;                /* Service flags (SF_*) */
    USHORT  MscMaxFrame;               /* Maximum frame size */
    ULONG   MscTBufCap;                /* Transmit buffer capacity */
    USHORT  MscTBlkSz;                 /* Transmit buffer block size */
    ULONG   MscRBufCap;                /* Receive buffer capacity */
    USHORT  MscRBlkSz;                 /* Receive buffer block size */
    UCHAR   MscVenCode[3];             /* IEEE vendor code */
    UCHAR   MscVenAdapter;             /* Vendor adapter code */
    FARPTR  MscVenAdaptDesc;           /* -> vendor adapter desc (ASCIIZ) */
    USHORT  MscInterrupt;              /* IRQ level (v2.0.1) */
    USHORT  MscTxQDepth;               /* Transmit queue depth (v2.0.1) */
    USHORT  MscMaxDataBlks;            /* Max data blocks in descriptors (v2.0.1) */
} MACSpecChar;

/*
 * MAC Service-Specific Status Table
 */
typedef struct {
    USHORT  MssSize;                    /* Table size in bytes */
    ULONG   MssDiagDT;                 /* Last diagnostics date/time */
    ULONG   MssStatus;                  /* MAC status flags */
    USHORT  MssFilter;                  /* Current packet filter */
    FARPTR  MssMediaStat;              /* -> media-specific status (MAC8023Stat) */
    ULONG   MssClearDT;               /* Last clear-statistics date/time */
    ULONG   MssFR;                     /* Frames received: total */
    ULONG   MssRFCRC;                  /* Receive fail: CRC error */
    ULONG   MssFRByt;                  /* Frames received: total bytes */
    ULONG   MssRFLack;                 /* Receive fail: lack of buffers */
    ULONG   MssFRMC;                   /* Frames received: multicast */
    ULONG   MssFRBC;                   /* Frames received: broadcast */
    ULONG   MssRFErr;                  /* Receive fail: errors in general */
    ULONG   MssRFMax;                  /* Receive fail: exceeds max size */
    ULONG   MssRFMin;                  /* Receive fail: less than min size */
    ULONG   MssFRMCByt;               /* Frames received: multicast bytes */
    ULONG   MssFRBCByt;               /* Frames received: broadcast bytes */
    ULONG   MssRFHW;                   /* Receive fail: hardware error */
    ULONG   MssFS;                     /* Frames sent: total */
    ULONG   MssFSByt;                  /* Frames sent: total bytes */
    ULONG   MssFSMC;                   /* Frames sent: multicast */
    ULONG   MssFSBC;                   /* Frames sent: broadcast */
    ULONG   MssFSBCByt;               /* Frames sent: broadcast bytes */
    ULONG   MssFSMCByt;               /* Frames sent: multicast bytes */
    ULONG   MssSFTime;                 /* Send fail: time-out */
    ULONG   MssSFHW;                   /* Send fail: hardware error */
} MACSpecStat;

/*
 * 802.3 Media-Specific Status Table
 */
typedef struct {
    USHORT  M83sSize;                   /* Table size */
    USHORT  M83sVer;                    /* Version */
    ULONG   M83sRFAln;                 /* Receive fail: alignment error */
    ULONG   M83sRMask;                 /* Receive fail bit mask */
    ULONG   M83sRFOvrn;               /* Receive fail: overrun */
    ULONG   M83sFSCols;               /* Frames sent: after collisions */
    ULONG   M83sFSDfr;                /* Frames sent: after deferring */
    ULONG   M83sSFColMx;              /* Frames not sent: max collisions */
    ULONG   M83sTotCol;               /* Total collisions during transmit */
    ULONG   M83sTotLCol;              /* Total late collisions */
    ULONG   M83sFSCol1;               /* Frames sent: after 1 collision */
    ULONG   M83sFSColM;               /* Frames sent: multiple collisions */
    ULONG   M83sFSHrtB;               /* Frames sent: CD heart beat */
    ULONG   M83sJabber;               /* Jabber errors */
    ULONG   M83sLostCS;               /* Lost carrier sense during transmit */
    ULONG   M83sTMask;                /* Transmit fail bit mask */
    ULONG   M83snumunder;             /* Number of underruns (v2.0.1) */
    USHORT  M83sRingUtil;             /* Ring utilization measure (v2.0.1) */
} MAC8023Stat;

/*
 * MAC Upper Dispatch Table
 *
 * Called by protocols to interact with the MAC driver.
 * All functions use Pascal calling convention, far.
 *
 *   Request — Issue a general request to the MAC (GR_* opcodes).
 *     Covers SetPacketFilter, AddMulticastAddress, OpenAdapter,
 *     ResetMAC, InterruptRequest, etc. Param1/Param2 are
 *     opcode-specific. May complete asynchronously (REQUEST_QUEUED)
 *     with a later RequestConfirm callback.
 *
 *   USHORT Request(USHORT ProtID, USHORT ReqHandle,
 *                  USHORT Param1, void far *Param2,
 *                  USHORT Opcode, USHORT MACDS);
 *
 *   TransmitChain — Submit a frame for transmission. The frame is
 *     described by a TxBufDesc containing an optional immediate data
 *     area (max 64 bytes) plus a scatter list of data blocks.
 *     May complete asynchronously with TransmitConfirm.
 *
 *   USHORT TransmitChain(USHORT ProtID, USHORT ReqHandle,
 *                        TxBufDesc far *BufDesc, USHORT MACDS);
 *
 *   TransferData — Copy received frame data into protocol buffers.
 *     Called by the protocol from within its ReceiveLookahead handler
 *     to retrieve the full frame. Returns bytes copied via first param.
 *     Multiple calls per indication allowed if SF_MULTIPLE_XFER.
 *
 *   USHORT TransferData(USHORT far *BytesCopied, USHORT FrameOfs,
 *                       TDBufDesc far *BufDesc, USHORT MACDS);
 *
 *   ReceiveRelease — Release a receive buffer held by the protocol
 *     after returning WAIT_FOR_RELEASE from ReceiveChain.
 *
 *   USHORT ReceiveRelease(USHORT ReqHandle, USHORT MACDS);
 *
 *   IndicationOn — Re-enable ReceiveLookahead, ReceiveChain, and
 *     Status indications from the MAC. Must be paired with a prior
 *     IndicationOff. Returns with interrupts disabled.
 *
 *   USHORT IndicationOn(USHORT MACDS);
 *
 *   IndicationOff — Suppress indications from the MAC. The MAC
 *     queues events while indications are off. The protocol must
 *     not block and must call IndicationOn as soon as possible.
 *     Returns with interrupts disabled.
 *
 *   USHORT IndicationOff(USHORT MACDS);
 */
typedef struct {
    FARPTR  MudCCp;                    /* -> common characteristics */
    FARPTR  MudRequest;                /* General request entry */
    FARPTR  MudTransmitChain;          /* TransmitChain entry */
    FARPTR  MudTransferData;           /* TransferData entry */
    FARPTR  MudReceiveRelease;         /* ReceiveRelease entry */
    FARPTR  MudIndicationOn;           /* IndicationOn entry */
    FARPTR  MudIndicationOff;          /* IndicationOff entry */
} MACUprDisp;

/*
 * Protocol Lower Dispatch Table
 *
 * Called by the MAC to deliver indications and confirmations
 * to the protocol. All functions use Pascal calling convention, far.
 *
 *   RequestConfirm — Asynchronous completion of a general Request.
 *     Delivers the final Status for a request that returned
 *     REQUEST_QUEUED. Includes the original request Opcode.
 *
 *   USHORT RequestConfirm(USHORT ProtID, USHORT MACID,
 *                         USHORT ReqHandle, USHORT Status,
 *                         USHORT Request, USHORT ProtDS);
 *
 *   TransmitConfirm — Asynchronous completion of TransmitChain.
 *     Delivers the final transmit Status. The protocol may now
 *     reuse or free the transmit buffers.
 *
 *   USHORT TransmitConfirm(USHORT ProtID, USHORT MACID,
 *                           USHORT ReqHandle, USHORT Status,
 *                           USHORT ProtDS);
 *
 *   ReceiveLookahead — A frame has arrived. Buffer contains the
 *     first BytesAvail bytes. The protocol inspects them and either
 *     calls TransferData to retrieve the full frame, or returns
 *     FRAME_NOT_RECOGNIZED to pass to the next protocol.
 *     FrameSize is the total frame length (0 if unknown). Indicate
 *     is a flag byte the protocol clears to request indications be
 *     left off after this handler returns.
 *     Called with indications implicitly disabled.
 *
 *   USHORT ReceiveLookahead(USHORT MACID, USHORT FrameSize,
 *                           USHORT BytesAvail, void far *Buffer,
 *                           UCHAR far *Indicate, USHORT ProtDS);
 *
 *   IndicationComplete — Signals end of an indication batch.
 *     Called after one or more ReceiveLookahead, ReceiveChain, or
 *     StatusIndication calls. The protocol performs any deferred
 *     processing here. May still be called even when indications
 *     are disabled.
 *
 *   USHORT IndicationComplete(USHORT MACID, USHORT ProtDS);
 *
 *   ReceiveChain — A complete frame has arrived in MAC-owned buffers
 *     described by BufDesc. The protocol may process the frame
 *     synchronously (return SUCCESS) or retain the buffer (return
 *     WAIT_FOR_RELEASE, then call ReceiveRelease later). For frames
 *     > 256 bytes, the first data block is >= 256 bytes.
 *     Called with indications implicitly disabled.
 *
 *   USHORT ReceiveChain(USHORT MACID, USHORT FrameSize,
 *                       USHORT ReqHandle, RxBufDesc far *BufDesc,
 *                       UCHAR far *Indicate, USHORT ProtDS);
 *
 *   StatusIndication — Notifies the protocol of a MAC status change.
 *     Opcode identifies the event (SI_* codes): adapter check, reset
 *     start/end, ring status, or interrupt status. Param is
 *     opcode-specific. Called with indications implicitly disabled.
 *
 *   USHORT StatusIndication(USHORT MACID, USHORT Param,
 *                           UCHAR far *Indicate, USHORT Opcode,
 *                           USHORT ProtDS);
 */
typedef struct {
    FARPTR  PldCCp;                    /* -> common characteristics */
    ULONG   PldFlags;                  /* Interface flags (PLD_*) */
    FARPTR  PldRequestConfirm;         /* RequestConfirm entry */
    FARPTR  PldTransmitConfirm;        /* TransmitConfirm entry */
    FARPTR  PldRcvLkAhead;             /* ReceiveLookahead entry */
    FARPTR  PldIndComplete;            /* IndicationComplete entry */
    FARPTR  PldRcvChain;               /* ReceiveChain entry */
    FARPTR  PldStatInd;                /* StatusIndication entry */
} ProtLwrDisp;

/*
 * Transmit Data Block
 */
typedef struct {
    UCHAR   TxPtrType;                 /* 0=physical, 2=GDT */
    UCHAR   TxRsvd;                    /* Reserved (must be zero) */
    USHORT  TxDataLen;                 /* Data block length */
    FARPTR  TxDataPtr;                 /* Data block address */
} TxDataBlock;

#define MAX_IMMED_LEN       64         /* max TxBufDesc.TxImmedLen */
#define MAX_TX_DATABLK      8          /* max TxBufDesc.TxData */

/*
 * Transmit Buffer Descriptor
 */
typedef struct {
    USHORT  TxImmedLen;                /* Immediate data length (max 64) */
    FARPTR  TxImmedPtr;                /* -> immediate data */
    USHORT  TxDataCount;               /* Number of data blocks (max 8) */
    TxDataBlock TxData[1];             /* Variable-length data block array */
} TxBufDesc;

/*
 * Transfer Data Block
 */
typedef struct {
    UCHAR   TDPtrType;                 /* 0=physical, 2=GDT */
    UCHAR   TDRsvd;                    /* Reserved (must be zero) */
    USHORT  TDDataLen;                 /* Data block length */
    FARPTR  TDDataPtr;                 /* Data block address */
} TDDataBlock;

#define MAX_TD_DATABLK      8          /* max TDBufDesc.TDData */

/*
 * Transfer Data Buffer Descriptor
 */
typedef struct {
    USHORT  TDDataCount;               /* Number of data blocks (max 8) */
    TDDataBlock TDData[1];             /* Variable-length data block array */
} TDBufDesc;

/*
 * Receive Chain Data Block
 */
typedef struct {
    USHORT  RxDataLen;                 /* Data block length */
    FARPTR  RxDataPtr;                 /* Data block address */
} RxDataBlock;

#define MAX_RX_DATABLK      8          /* max RxBufDesc.RxData */

/*
 * Receive Chain Buffer Descriptor
 */
typedef struct {
    USHORT  RxDataCount;               /* Number of data blocks (max 8) */
    RxDataBlock RxData[1];             /* Variable-length data block array */
} RxBufDesc;

/*
 * Protocol Manager Request Block
 */
typedef struct {
    USHORT  Opcode;                    /* PM request opcode (PM_*) */
    USHORT  Status;                    /* Status at completion */
    FARPTR  Pointer1;                  /* First parameter pointer */
    FARPTR  Pointer2;                  /* Second parameter pointer */
    USHORT  Word1;                     /* Parameter word */
} ReqBlock;

/*
 * PROTOCOL.INI Configuration Memory Image
 *
 * Linked list of module configurations, each containing a linked
 * list of keyword entries with typed parameters.
 */

typedef struct {
    USHORT  ParamType;                 /* 0=signed int, 1=string */
    USHORT  ParamLen;                  /* String length (incl. null) or 4 */
    union {
        int32_t Num;                   /* Integer value */
        char    Str[1];                /* Variable-length string */
    } ParamVal;
} Param;

typedef struct KeywordEntry {
    FARPTR  NextKeywordEntry;          /* -> next entry (NULL if last) */
    FARPTR  PrevKeywordEntry;          /* -> prev entry (NULL if first) */
    char    KeyWord[NAME_LEN];         /* Keyword (left side of "=") */
    USHORT  NumParams;                 /* Number of parameters */
    Param   Params[1];                 /* Variable-length param array */
} KeywordEntry;

typedef struct ModuleConfig {
    FARPTR  NextModule;                /* -> next module (NULL if last) */
    FARPTR  PrevModule;                /* -> prev module (NULL if first) */
    char    ModName[NAME_LEN];         /* Bracketed module name */
    KeywordEntry KE[1];                /* Head of keyword list */
} ModuleConfig;

#pragma pack()

/* dosemu2 internal NDIS MAC driver, see src/dosext/net/ndis.c */
extern void ndis_init(void);
extern void ndis_reset(void);
extern void ndis_term(void);

#endif /* __ASSEMBLER__ */

/*
 * Name of the DOS character device implementing the MAC driver.
 * It has to be matched by the DriverName= keyword of the PROTOCOL.INI
 * section describing this adapter.
 */
#define NDIS_DEVICE_NAME "DE2NDIS$"

/* sub-functions of DOS_HELPER_NDIS_HELPER, see ndis.sys */
#define NDIS_SUBHELPER_INIT      0  /* query availability and memory size */
#define NDIS_SUBHELPER_CONFIG    1  /* parse PROTOCOL.INI, build tables */
#define NDIS_SUBHELPER_DONE      2  /* registration with PROTMAN succeeded */

/* error codes returned by the helper in BL */
#define NDIS_ERROR_DISABLED      1  /* NDIS support not enabled */
#define NDIS_ERROR_ALREADY       2  /* driver already initialized */
#define NDIS_ERROR_NOCONFIG      3  /* no matching PROTOCOL.INI section */
#define NDIS_ERROR_NONET         4  /* no networking available */

#endif /* NDIS_H */

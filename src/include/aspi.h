/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* 
 * include/aspi.h copied and modified from the wine project's wintypes.h
 * drivers/aspi.c - emulate the DOS ASPI programming interface using 
 *		    the Linux generic SCSI driver (/dev/sg?)
 * most of this code shamelessly stolen from the wine
 * project aspi.c written by  Bruce Milner <Bruce.Milner@genetics.utah.edu>
 *
 * hacked by         Karl Kiniger <ki@kretz.co.at>
 * further hacked by Hans Lermen <lermen@dosemu.org>
 */

#if !defined(ASPI_H)
#define ASPI_H

typedef Bit32u 		FARPTR16;
typedef Bit32u		FARPROC16;
#define FARPTR16_TO_LIN(a) \
	((a&0xffff) + ((a >> 12)& 0x000ffff0))

#pragma pack(1)

#define SS_PENDING	0x00
#define SS_COMP		0x01
#define SS_ABORTED	0x02
#define SS_ERR		0x04
#define SS_OLD_MANAGE	0xe1
#define SS_ILLEGAL_MODE	0xe2
#define SS_NO_ASPI	0xe3
#define SS_FAILED_INIT	0xe4
#define SS_INVALID_HA	0x81
#define SS_INVALID_SRB	0xe0
#define SS_ASPI_IS_BUSY	0xe5
#define SS_BUFFER_TO_BIG	0xe6

#define SC_HA_INQUIRY		0x00
#define SC_GET_DEV_TYPE 	0x01
#define SC_EXEC_SCSI_CMD	0x02
#define SC_ABORT_SRB		0x03
#define SC_RESET_DEV		0x04

/* Host adapter status codes */
#define HASTAT_OK		0x00
#define HASTAT_SEL_TO		0x11
#define HASTAT_DO_DU		0x12
#define HASTAT_BUS_FREE		0x13
#define HASTAT_PHASE_ERR	0x14

/* Target status codes */
#define STATUS_GOOD		0x00
#define STATUS_CHKCOND		0x02
#define STATUS_BUSY		0x08
#define STATUS_RESCONF		0x18


typedef union SRB16 * LPSRB16;

#define SRB_HEADER \
  Bit8u        SRB_Cmd;                /* ASPI command code		(W) */\
  Bit8u        SRB_Status;             /* ASPI command status byte	(R) */\
  Bit8u        SRB_HaId;               /* ASPI host adapter number	(W) */\
  Bit8u        SRB_Flags;              /* ASPI request flags		(W) */\
  Bit32u       SRB_Hdr_Rsvd;           /* Reserved, MUST = 0		(-) */

struct SRB_HaInquiry16 {
  Bit8u	SRB_Cmd;
  Bit8u	SRB_Status;
  Bit8u	HA_number;
  Bit8u	SRB_Flags;
  Bit16u SRB_55AASignature;
  Bit16u SRB_ExtBufferSize;
  Bit8u	HA_Count;
  Bit8u	HA_SCSI_ID;
  Bit8u	HA_ManagerId[16];
  Bit8u	HA_Identifier[16];
  Bit8u	HA_Unique[16];
  Bit16u HA_Extensions;
} __attribute__ ((packed));

typedef struct SRB_HaInquiry16 SRB_HaInquiry16;

struct SRB_ExecSCSICmd16 {
  SRB_HEADER
  Bit8u        SRB_Target;             /* Targets SCSI ID		(W) */
  Bit8u        SRB_Lun;                /* Targets LUN number		(W) */
  Bit32u       SRB_BufLen;             /* Data Allocation LengthPG	(W/R) */
  Bit8u        SRB_SenseLen;           /* Sense Allocation Length	(W) */
  FARPTR16     SRB_BufPointer;         /* Data Buffer Pointer		(W) */
  Bit32u       SRB_Rsvd1;              /* Reserved, MUST = 0		(-/W) */
  Bit8u        SRB_CDBLen;             /* CDB Length = 6			(W) */
  Bit8u        SRB_HaStat;             /* Host Adapter Status		(R) */
  Bit8u        SRB_TargStat;           /* Target Status			(R) */
  FARPROC16    SRB_PostProc;	       /* Post routine			(W) */
  Bit8u        SRB_Rsvd2[34];          /* Reserved, MUST = 0		    */
  Bit8u	       CDBByte[0];	       /* SCSI CBD - variable length	(W) */
  /* variable example for 6 byte cbd
   * Bit8u        CDBByte[6];              SCSI CDB			(W)
   * Bit8u        SenseArea6[SENSE_LEN];   Request Sense buffer 	(R)
   */
} __attribute__ ((packed)) ;

typedef struct SRB_ExecSCSICmd16 SRB_ExecSCSICmd16;

struct SRB_Abort16 {
  /* ASPI command code = SC_ABORT_SRB */
  SRB_HEADER
  LPSRB16     SRB_ToAbort;        /* Pointer to SRB to abort  */
} __attribute__ ((packed));

typedef struct SRB_Abort16 SRB_Abort16;

struct SRB_BusDeviceReset16 {
  /* ASPI command code = SC_RESET_DEV */
  SRB_HEADER
  Bit8u        SRB_Target;         /* Target's SCSI ID         */
  Bit8u        SRB_Lun;            /* Target's LUN number      */
  Bit8u        SRB_ResetRsvd1[14]; /* Reserved, MUST = 0       */
  Bit8u        SRB_HaStat;         /* Host Adapter Status      */
  Bit8u        SRB_TargStat;       /* Target Status            */
  FARPTR16     SRB_PostProc;       /* Post routine             */
  Bit8u        SRB_ResetRsvd2[34]; /* Reserved, MUST = 0       */
} __attribute__ ((packed));

typedef struct SRB_BusDeviceReset16 SRB_BusDeviceReset16;

struct SRB_GDEVBlock16 {
  /* ASPI command code = SC_GET_DEV_TYPE */
  SRB_HEADER
  Bit8u        SRB_Target;         /* Target's SCSI ID         */
  Bit8u        SRB_Lun;            /* Target's LUN number      */
  Bit8u        SRB_DeviceType;     /* Target's peripheral device type */
} __attribute__ ((packed));

typedef struct SRB_GDEVBlock16 SRB_GDEVBlock16;



struct SRB_Common16 {
  SRB_HEADER
};


typedef struct SRB_Common16 SRB_Common16;


union SRB16 {
  SRB_Common16		common;
  SRB_HaInquiry16	inquiry;
  SRB_ExecSCSICmd16	cmd;
  SRB_Abort16		abort;
  SRB_BusDeviceReset16	reset;
  SRB_GDEVBlock16	devtype;
};

typedef union SRB16 SRB16;

extern char *aspi_add_device(char *name, char *devtype, int);
extern void aspi_helper(int);

#endif

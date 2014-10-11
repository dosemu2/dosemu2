/*
 * (C) Copyright 1992, ..., 2006 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* VBE error codes */
#define VBE_ERROR_UNSUP			-1		/* unsupported VBE function */
#define VBE_ERROR_GENERAL_FAIL		1		/* VBE call failed */
#define VBE_ERROR_HARDWARE_FAIL		2		/* invalid on current hardware */
#define VBE_ERROR_MODE_FAIL		3		/* invalid in current mode */

/* VBE info block */
struct VBE_vi {
	uint32_t VBESig;	// 0x00
	uint16_t VESAVersion;	// 0x04
	uint32_t OEMID;		// 0x06
	uint32_t Capabilities;	// 0x0a
	uint32_t ModeList;	// 0x0e
	uint16_t Memory;	// 0x12
/* VBE 2.0+ */
	uint16_t OEMSoftRev;	// 0x14
	uint32_t OEMVendorName;	// 0x16
	uint32_t OEMProdName;	// 0x1a
	uint32_t OEMProductRev;	// 0x1e
	uint8_t reserved[0x100-0x22];
	uint8_t OEMData[0x100];
} __attribute__((packed));

/* VBE mode info block */
struct VBE_vm {
	uint16_t ModeAttrib;	// 0x00
	uint8_t WinAAttrib;	// 0x02
	uint8_t WinBAttrib;	// 0x03
	uint16_t WinGran;	// 0x04
	uint16_t WinSize;	// 0x06
	uint16_t WinASeg;	// 0x08
	uint16_t WinBSeg;	// 0x0a
	uint32_t WinFuncPtr;	// 0x0c
	uint16_t BytesPLine;	// 0x10
	uint16_t XRes;		// 0x12
	uint16_t YRes;		// 0x14
	uint8_t XCharSize;	// 0x16
	uint8_t YCharSize;	// 0x17
	uint8_t NumPlanes;	// 0x18
	uint8_t BitsPPixel;	// 0x19
	uint8_t Banks;		// 0x1a
	uint8_t MemModel;	// 0x1b
	uint8_t BankSize;	// 0x1c
	uint8_t Pages;		// 0x1d
	uint8_t Reserved1;	// 0x1e
	uint8_t RedMaskSize;	// 0x1f
	uint8_t RedFieldPos;	// 0x20
	uint8_t GreenMaskSize;	// 0x21
	uint8_t GreenFieldPos;	// 0x22
	uint8_t BlueMaskSize;	// 0x23
	uint8_t BlueFieldPos;	// 0x24
	uint8_t RsvdMaskSize;	// 0x25
	uint8_t RsvdFieldPos;	// 0x26
	uint8_t DirectColor;	// 0x27
	uint32_t PhysBasePtr;	// 0x28
	uint32_t OffScreenOfs;	// 0x2c
	uint16_t OffScreenMem;	// 0x30
	uint8_t reserved[0x100-0x32];
} __attribute__((packed));

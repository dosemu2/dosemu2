/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* VBE error codes */
#define VBE_ERROR_UNSUP			-1		/* unsupported VBE function */
#define VBE_ERROR_GENERAL_FAIL		1		/* VBE call failed */
#define VBE_ERROR_HARDWARE_FAIL		2		/* invalid on current hardware */
#define VBE_ERROR_MODE_FAIL		3		/* invalid in current mode */

/*
 * macros for easier access to some VBE data structures
 */

/* VBE info block */
#define VBE_viVBESig		VBE_W(0x00)
#define VBE_viVESAVersion	VBE_S(0x04)
#define VBE_viOEMID		VBE_W(0x06)
#define VBE_viCapabilities	VBE_W(0x0a)
#define VBE_viModeList		VBE_W(0x0e)
#define VBE_viMemory		VBE_S(0x12)
/* VBE 2.0+ */
#define VBE_viOEMSoftRev	VBE_S(0x14)
#define VBE_viOEMVendorName	VBE_W(0x16)
#define VBE_viOEMProdName	VBE_W(0x1a)
#define VBE_viOEMProductRev	VBE_W(0x1e)
#define VBE_OEMData		0x100		/* offset to OEM data block */

#define VBE_viSize		0x200

/* VBE mode info block */
#define VBE_vmModeAttrib	VBE_S(0x00)
#define VBE_vmWinAAttrib	VBE_B(0x02)
#define VBE_vmWinBAttrib	VBE_B(0x03)
#define VBE_vmWinGran		VBE_S(0x04)
#define VBE_vmWinSize		VBE_S(0x06)
#define VBE_vmWinASeg		VBE_S(0x08)
#define VBE_vmWinBSeg		VBE_S(0x0a)
#define VBE_vmWinFuncPtr	VBE_W(0x0c)
#define VBE_vmBytesPLine	VBE_S(0x10)
#define VBE_vmXRes		VBE_S(0x12)
#define VBE_vmYRes		VBE_S(0x14)
#define VBE_vmXCharSize		VBE_B(0x16)
#define VBE_vmYCharSize		VBE_B(0x17)
#define VBE_vmNumPlanes		VBE_B(0x18)
#define VBE_vmBitsPPixel	VBE_B(0x19)
#define VBE_vmBanks		VBE_B(0x1a)
#define VBE_vmMemModel		VBE_B(0x1b)
#define VBE_vmBankSize		VBE_B(0x1c)
#define VBE_vmPages		VBE_B(0x1d)
#define VBE_vmReserved1		VBE_B(0x1e)
#define VBE_vmRedMaskSize	VBE_B(0x1f)
#define VBE_vmRedFieldPos	VBE_B(0x20)
#define VBE_vmGreenMaskSize	VBE_B(0x21)
#define VBE_vmGreenFieldPos	VBE_B(0x22)
#define VBE_vmBlueMaskSize	VBE_B(0x23)
#define VBE_vmBlueFieldPos	VBE_B(0x24)
#define VBE_vmRsvdMaskSize	VBE_B(0x25)
#define VBE_vmRsvdFieldPos	VBE_B(0x26)
#define VBE_vmDirectColor	VBE_B(0x27)
#define VBE_vmPhysBasePtr	VBE_W(0x28)
#define VBE_vmOffScreenOfs	VBE_W(0x2c)
#define VBE_vmOffScreenMem	VBE_S(0x30)

#define VBE_vmSize		0x100

/* some useful macros */
#define VBE_B(a) VBE_BUFFER[a]
#define VBE_S(a) *((unsigned short *) (VBE_BUFFER + a))
#define VBE_W(a) *((unsigned *) (VBE_BUFFER + a))

#define VBE_SEG_OFS(a, b)	(((a) << 16) + ((b) & 0xffff))

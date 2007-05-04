/* 
 * This file is derived from dos_mscdex.cpp which is
 *  Copyright (C) 2002-2005  The DOSBox Team
 *
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <sys/types.h>
#include "Linux/cdrom.h"
#include "emu.h"
#include "int.h"
#include "cpu.h"
#include "mfs.h"
#include "mangle.h"
#include "utilities.h"

#define CALC_PTR(PTR,OFFSET,RESULT_TYPE) ((RESULT_TYPE *)(PTR+OFFSET))

#define MSCD_READ_ADRESSING       13
#define MSCD_READ_STARTSECTOR     20
#define MSCD_READ_NUMSECTORS      18

#define MSCDEX_VERSION_HIGH		2
#define MSCDEX_VERSION_LOW		23

#define MSCDEX_ERROR_BAD_FORMAT		11
#define MSCDEX_ERROR_UNKNOWN_DRIVE	15
#define MSCDEX_ERROR_DRIVE_NOT_READY	21

static int numDrives = 0;
static int cd_drives[4] = { -1, -1, -1, -1 };	/* drive letter (A=0) for each CDROM */

static int GetDriver(int drive)
{
	int i = 0;
	for (i = 0; i < 4; i++)
		if (cd_drives[i] == drive)
			break;
	return i;
}

void register_cdrom(int drive, int device)
{
	device--;
	if (device < 0 || device >= 4)
		return;
	if (cd_drives[device] == -1)
		numDrives++;
	cd_drives[device] = drive;
}

void unregister_cdrom(int drive)
{
	int device = GetDriver(drive);
	if (device >= 4)
		return;
	cd_drives[drive] = -1;
	numDrives--;
}

static int ReadSectors(int drive, Bit32u sector, Bit16u num, unsigned char *buf)
{
	unsigned char req_buf[24];
	unsigned save_ax = _AX;
	int error;
	Bit8u driver = GetDriver(drive);
	if (driver >= 4)
		return MSCDEX_ERROR_UNKNOWN_DRIVE;
	*CALC_PTR(req_buf, MSCD_READ_STARTSECTOR, u_long) = sector;
	*CALC_PTR(req_buf, MSCD_READ_ADRESSING, u_char) = 0;
	*CALC_PTR(req_buf, MSCD_READ_NUMSECTORS, u_short) = num;
	_AH = 2 | (driver << 6);
	cdrom_helper(req_buf, buf);
	error = _AL == 0 ? 0 : MSCDEX_ERROR_DRIVE_NOT_READY;
	_AX = save_ax;
	return error;
}

static int ReadVTOC(int drive, Bit16u volume, unsigned char *buf)
{
	char id[5];
	Bit8u type;
	int error = ReadSectors(drive, 16 + volume, 1, buf);
	if (error)
		return error;
	MEMCPY_2UNIX(id, buf + 1, 5);
	if (memcmp("CD001", id, 5) != 0)
		return MSCDEX_ERROR_BAD_FORMAT;
	type = READ_BYTE(buf);
	return 0;
}

int get_volume_label_cdrom(int drive, char *buf)
{
	char *p;
	unsigned char readbuf[CD_FRAMESIZE];
	if (ReadVTOC(drive, 0x00, readbuf))
		return 0;
	memcpy(buf, readbuf + 40, 31);
	buf[31] = '\0';
	p = strchr(buf, '\0');
	while (--p >= buf && isspace((unsigned char)*p)) ;
	p[1] = '\0';
	return 1;
}

static int fill_buffer(int copyFlag, Bit32u buffer, unsigned char *readBuf,
		       unsigned entryLength)
{
	// TO DO : name gefunden, Daten in den Buffer kopieren
	if (copyFlag) {
		Bit8u writeBuf[256];
		C_printf
		    ("MSCDEX: GetDirEntry: Copyflag structure not entirely "
		     "accurate maybe\n");
		if (entryLength > 256)
			return MSCDEX_ERROR_BAD_FORMAT;
		// 00h  BYTE    length of XAR in Logical Block Numbers
		writeBuf[0] = readBuf[1];
		// 01h  DWORD   Logical Block Number of file start
		memcpy(&writeBuf[1], &readBuf[0x2], 4);
		// 05h  WORD    size of disk in logical blocks
		writeBuf[5] = 0;
		writeBuf[6] = 8;
		// 07h  DWORD   file length in bytes
		memcpy(&writeBuf[7], &readBuf[0xa], 4);
		// 0bh  DWORD   date and time
		memcpy(&writeBuf[0xb], &readBuf[0x12], 7);
		// 12h  BYTE    bit flags
		writeBuf[0x12] = readBuf[0x19];
		// 13h  BYTE    interleave size
		writeBuf[0x13] = readBuf[0x1a];
		// 14h  BYTE    interleave skip factor
		writeBuf[0x14] = readBuf[0x1b];
		// 15h  WORD    volume set sequence number
		memcpy(&writeBuf[0x15], &readBuf[0x1c], 2);
		writeBuf[0x17] = readBuf[0x20];
		memcpy(&writeBuf[0x18], &readBuf[21],
		       readBuf[0x20] <= 38 ? readBuf[0x20] : 38);
		MEMCPY_2DOS(buffer, writeBuf, 0x18 + 40);
	} else {
		// Direct copy
		MEMCPY_2DOS(buffer, readBuf, entryLength);
	}
	return 1;		/* ISO 9660 */
}

static int namecomp(const char *name1, size_t len1, const char *name2,
		    size_t len2)
{
	if (len1 > 1 && name1[len1 - 1] == '.')
		len1--;
	C_printf("MSCDEX: %zu %.*s\n", len1, (int)len1, name1);
	if (len1 == len2 && memcmp(name1, name2, len1) == 0) {
		C_printf("MSCDEX: Get DirEntry : Found : %s\n", name1);
		return 1;
	}
	return 0;
}

static int GetDirectoryEntry(int drive, int copyFlag, char *pathname,
			     Bit32u buffer)
{
	char searchName[256];
	int error, dirSize;
	unsigned char defBuffer[CD_FRAMESIZE];
	unsigned dirEntrySector, entryLength, index;
	char *searchPos = searchName;
	size_t searchlen;

	/* skip initial \ */
	MEMCPY_2UNIX(searchName, pathname + 1, 255);
	searchName[255] = '\0';
	strupperDOS(searchName);

	//strip of tailing . (XCOM APOCALIPSE)
	searchlen = strlen(searchName);
	if (searchlen > 1 && strcmp(searchName, ".."))
		if (searchName[searchlen - 1] == '.')
			searchName[searchlen - 1] = 0;

	C_printf("MSCDEX: Get DirEntry : Find : %s\n", searchName);
	// read vtoc
	error = ReadSectors(drive, 16, 1, defBuffer);
	if (error)
		return error;
	// TODO: has to be iso 9660
	if (memcmp("CD001", defBuffer + 1, 5) != 0)
		return MSCDEX_ERROR_BAD_FORMAT;
	// get directory position
	dirEntrySector = *(unsigned *)(defBuffer + 156 + 2);
	dirSize = *(int *)(defBuffer + 156 + 10);
	do {
		// Get string part
		char *useName = searchPos;
		size_t useNameLength;

		searchPos = strchr(searchPos, '\\');

		if (searchPos) {
			useNameLength = searchPos - useName;
			searchPos++;
		} else
			useNameLength = strlen(useName);

		while (1) {
			error =
			    ReadSectors(drive, dirEntrySector, 1, defBuffer);
			if (error)
				return error;
			index = 0;

			do {
				char *entryName, *longername;
				unsigned nameLength;

				entryLength = defBuffer[index];
				C_printf("MSCDEX: entryLength=%u, index=%d\n",
					 entryLength, index);
				if (entryLength == 0)
					break;
				nameLength = defBuffer[index + 32];
				entryName = defBuffer + index + 33;
				if (namecomp
				    (entryName, nameLength, useName,
				     useNameLength))
					break;
				/* Xcom Apocalipse searches for MUSIC.
				 * and expects to find MUSIC;1
				 * All Files on the CDROM are of the kind
				 * blah;1
				 */
				longername = memchr(entryName, ';', nameLength);
				if (longername &&
				    namecomp(entryName, longername - entryName,
					     useName, useNameLength))
					break;
				index += entryLength;
			} while (index + 33 <= CD_FRAMESIZE);
			if (index + 33 <= CD_FRAMESIZE)
				break;
			// continue search in next sector
			dirSize -= 2048;
			if (dirSize <= 0)
				return 2;	// file not found
			dirEntrySector++;
		}
		// directory wechseln
		dirEntrySector = *(unsigned *)(defBuffer + index + 2);
		dirSize = *(unsigned *)(defBuffer + index + 10);
	} while (searchPos);
	return fill_buffer(copyFlag, buffer, defBuffer + index, entryLength);
};

int mscdex(void)
{
	unsigned char *buf = MK_FP32(_ES, _BX);
	unsigned long dev;
	unsigned seg, strat, intr;
	int error;
	int i;
	char devname[] = "MSCD0001";

	if (numDrives == 0)
		return 0;

	switch (_AL) {
	case 0x00:		/* install check */
		_BX = numDrives;
		if (_BX > 0) {
			int firstdrive = INT_MAX;
			for (i = 0; i < 4; i++) {
				if (cd_drives[i] != -1
				    && cd_drives[i] < firstdrive)
					firstdrive = cd_drives[i];
			}
			_CX = firstdrive;
		}
		break;
	case 0x01:		/* driver info */
		for (i = 0; i < 4; i++) {
			if (cd_drives[i] != -1) {
				/* subunit: always 0 for cdrom.sys */
				WRITE_BYTE(buf, 0x00);
				devname[7] = i + '1';
				WRITE_DWORD(buf + 1, is_dos_device(devname));
				buf += 5;
			}
		};
		break;
	case 0x02:		/* copyright file name */
	case 0x03:		/* abstract file name */
	case 0x04:		/* documentation file name */
		{
			char readbuf[CD_FRAMESIZE];
			if (ReadVTOC(_CX, 0x00, readbuf) == 0) {
				MEMCPY_2DOS(buf, readbuf + 702 + (_AL - 2) * 37,
					    37);
				WRITE_BYTE(buf + 37, 0);
				NOCARRY;
			} else {
				_AX = MSCDEX_ERROR_UNKNOWN_DRIVE;
				CARRY;
			}
			break;
		}
	case 0x05:		/* read vtoc */
		NOCARRY;
		error = ReadVTOC(_CX, _DX, buf);
		if (error) {
			_AL = error;
			CARRY;
		};
		break;
	case 0x08:		/* read sectors */
		NOCARRY;
		error = ReadSectors(_CX, (_SI << 16) + _DI, _DX, buf);
		if (error) {
			_AL = error;
			CARRY;
		};
		break;
	case 0x09:		/* write sectors - not supported */
		_AL = MSCDEX_ERROR_DRIVE_NOT_READY;
		CARRY;
		break;
	case 0x0B:		/* CD-ROM drive check */
		_AX = 0;
		for (i = 0; i < 4; i++)
			if (_CX == cd_drives[i]) {
				_AX = 1;
				break;
			}
		_BX = 0xadad;
		break;
	case 0x0C:
		_BX = (MSCDEX_VERSION_HIGH << 8) + MSCDEX_VERSION_LOW;
		break;
	case 0x0D:		/* get drives */
		for (i = 0; i < 4; i++)
			if (cd_drives[i] != -1)
				WRITE_BYTE(buf++, cd_drives[i]);
		break;
	case 0x0F:		/* Get directory entry */
		CARRY;
		_AX =
		    GetDirectoryEntry(_CL, _CH & 1, buf,
				      SEGOFF2LINEAR(_SI, _DI));
		if (_AX == 0 || _AX == 1)
			NOCARRY;
		break;
	case 0x10:
		{
			int driver = GetDriver(_CX);
			if (driver >= 4)
				break;
			devname[7] = driver + '1';
			dev = is_dos_device(devname);
			seg = dev >> 16;
			dev = SEGOFF2LINEAR(seg, dev & 0xffff);
			strat = READ_WORD(dev + 6);
			intr = READ_WORD(dev + 8);
			fake_call_to(seg, intr);
			fake_call_to(seg, strat);
			break;
		}
	default:
		C_printf("unknown mscdex\n");
		return 0;
	}
	return 1;
}

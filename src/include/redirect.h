/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef REDIRECT_H
#define REDIRECT_H

#ifndef __ASSEMBLER__

#define LINUX_RESOURCE "\\\\LINUX\\FS"
#define LINUX_PRN_RESOURCE "\\\\LINUX\\PRN"

/* #define MAX_PATH_LENGTH 57 */
/* 2001/01/05 Manfred Scherer
 * With the value 57 I can create pathlength until 54.
 * In native DOS I can create pathlength until 63.
 * With the value 66 it should be possible to create
 * paths with length 63.
 * I've tested it on my own system, and I found the value 66
 * is right for me.
 */
#define MAX_PATH_LENGTH 66
#define MAX_RESOURCE_LENGTH_EXT 1024

#define REDIR_PRINTER_TYPE    3
#define REDIR_DISK_TYPE       4
#define REDIR_CLIENT_SIGNATURE 0x6a00
#define REDIR_DEVICE_READ_ONLY 0b0000000000000001 /* Same as NetWare Lite */
#define REDIR_DEVICE_CDROM_MASK 0b0000000000000110 /* CDROM unit number */
#define REDIR_DEVICE_PERMANENT  0x8
#define REDIR_DEVICE_IDX_SHIFT  4

#define REDIR_STATUS_DISABLED  0x80

#define DOS_GET_REDIRECTION_MODE 0x5F00
#define DOS_SET_REDIRECTION_MODE 0x5F01
#define DOS_GET_REDIRECTION    0x5F02
#define DOS_REDIRECT_DEVICE    0x5F03
#define DOS_CANCEL_REDIRECTION 0x5F04
#define DOS_GET_REDIRECTION_EXT 0x5F05
#define DOS_GET_REDIRECTION_EX6 0x5F06

uint16_t RedirectDevice(char *dStr, char *sStr,
                        uint8_t deviceType, uint16_t deviceParameter);
int ResetRedirection(int);
extern void mfs_set_stk_offs(int);
extern int mfs_define_drive(const char *path);
/* temporary solution til QUALIFY_FILENAME works */
int build_posix_path(char *dest, const char *src, int allowwildcards);
#endif

#define REDVER_NONE    0
#define REDVER_PC30    1
#define REDVER_PC31    2
#define REDVER_PC40    3

#define REDVER_CQ30    4	// Microsoft Compaq v3.00 variant
#define SDASIZE_CQ30   0x0832

#define DOSEMU_EMUFS_DRIVER_VERSION 4
#define DOSEMU_EMUFS_DRIVER_MIN_VERSION 2

#endif

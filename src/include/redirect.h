/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef REDIRECT_H
#define REDIRECT_H

#define LINUX_RESOURCE "\\\\LINUX\\FS"
#define LINUX_PRN_RESOURCE "\\\\LINUX\\PRN"

int RedirectDisk(int, char *, int);
int RedirectPrinter(char *);
int CancelDiskRedirection(int);
int ResetRedirection(int);
int GetRedirectionRoot(int,char **,int *);
void redirect_devices(void);
extern void mfs_set_stk_offs(int);
/* temporary solution til QUALIFY_FILENAME works */
int build_posix_path(char *dest, const char *src, int allowwildcards);

#endif

/* 
 * (C) Copyright 1992, ..., 2006 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */


int OpenNetworkLink(char *, unsigned short);
void CloseNetworkLink(int);
int GetDeviceHardwareAddress(char *, char *);
int GetDeviceMTU(char *);
int GetDosnetID(void);
void GenerateDosnetID(void);

/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

void LibpacketInit(void);
int OpenNetworkLink(char *);
void CloseNetworkLink(int);
int GetDeviceHardwareAddress(char *, unsigned char *);
int GetDeviceMTU(char *);

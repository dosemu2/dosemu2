/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */


int OpenNetworkType(unsigned short);
int OpenBroadcastNetworkType(void);
void CloseNetworkLink(int);
int WriteToNetwork(int, const char *, const char *, int);
int ReadFromNetwork(int, char *, char *, int);
int GetDeviceHardwareAddress(char *, char *);
int GetDeviceMTU(char *);

int tun_alloc(char *dev);

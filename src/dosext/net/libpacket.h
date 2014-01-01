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

void pkt_io_select(void(*)(void *), void *);
ssize_t pkt_read(int fd, void *buf, size_t count);
ssize_t pkt_write(int fd, const void *buf, size_t count);

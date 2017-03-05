/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

void LibpacketInit(void);
int OpenNetworkLink(char *);
void CloseNetworkLink(int);
int GetDeviceHardwareAddress(unsigned char *);
int GetDeviceMTU(void);

void pkt_io_select(void(*)(void *), void *);
ssize_t pkt_read(int fd, void *buf, size_t count);
ssize_t pkt_write(int fd, const void *buf, size_t count);
int pkt_is_registered_type(int type);

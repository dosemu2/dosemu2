
int OpenNetworkType(unsigned short);
int OpenBroadcastNetworkType();
void CloseNetworkLink(int);
int WriteToNetwork(int, const char *, const char *, int);
int ReadFromNetwork(int, char *, char *, int);
int GetDeviceHardwareAddress(char *, char *);
int GetDeviceMTU(char *);

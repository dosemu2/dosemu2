
extern int OpenNetworkType(unsigned short);
extern void CloseNetworkLink(int);
extern int WriteToNetwork(int,const char *,const char *,int);
extern int ReadFromNetwork(int, char *, char *,int);
extern int GetDeviceHardwareAddress(char *, char *);
extern int GetDeviceMTU(char *);

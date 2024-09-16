#ifndef IPX_BE_H
#define IPX_BE_H

struct ipx_ops {
    int (*GetMyAddress)(unsigned long ipx_net, unsigned char *MyAddress);
    int (*open)(u_short port, u_char *MyAddress, u_short *newPort, int *err);
    int (*recv)(int fd, u_char *buffer, int bufLen, u_char *MyAddress,
	far_t ECBPtr);
    int (*send)(int fd, u_char *data, int dataLen, u_char *MyAddress);
};

void ipx_register_ops(const struct ipx_ops *ops);

#endif

#ifndef IPX_BE_H
#define IPX_BE_H

struct ipx_ops {
    const unsigned char *(*GetMyAddress)(void);
    int (*open)(u_short port, u_short *newPort, int *err);
    int (*close)(int sock);
    int (*recv)(int fd, u_char *buffer, int bufLen, u_short port);
    int (*send)(int fd, u_char *data, int dataLen);
};

void ipx_register_ops(const struct ipx_ops *ops);

#endif

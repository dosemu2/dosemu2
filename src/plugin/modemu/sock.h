struct sock {
    int fd;
    int alive;
};
extern struct sock sock;

#define sockIsAlive() (sock.alive)

void
sockClose(void);
void
sockShutdown(void);
int
sockDial(void);
int sockConnectStart(void);

#ifdef DOSEMU
void modemu_async_callback(void *arg);
#endif

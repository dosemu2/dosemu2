struct {
    int fd;
    int alive;
} sock;

#define sockIsAlive() (sock.alive)

void
sockClose(void);
void
sockShutdown(void);
int
sockDial(void);

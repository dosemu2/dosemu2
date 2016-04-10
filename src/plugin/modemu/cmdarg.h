struct {
    enum { CA_STDINOUT,	CA_SHOWDEV, CA_COMMX, CA_DEVGIVEN } ttymode;
    const char *commx;
    const char *atcmd;
    const char *dev;
} cmdarg;

void
cmdargParse(const char **argv);

struct cmdarg {
    enum { CA_STDINOUT,	CA_SHOWDEV, CA_COMMX, CA_DEVGIVEN } ttymode;
    const char *commx;
    const char *atcmd;
    const char *dev;
};
extern struct cmdarg cmdarg;

void
cmdargParse(const char **argv);

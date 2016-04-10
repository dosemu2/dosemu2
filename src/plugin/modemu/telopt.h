#if 0
/*if use putOptCmd*/
#include <arpa/telnet.h>/*->telopt.h (IAC)*/
#include "sockbuf.h"	/*->telopt.h (putSock1)*/
#endif

/* requirements for a telnet option */
typedef enum {
    TOR_MUSTNOT,	/* disable the opt or disconnect */
    TOR_BETTERNOT,	/* disable the opt or enable unwillingly */
    TOR_NEUTRAL,	/* modemu doesn't initiate any action for the opt.
			   {en,dis}able the opt as the peer requests. */
    TOR_BETTER,		/* enable the opt or disable unwillingly */
    TOR_MUST		/* enable the opt or disconnect */
} TelOptReq;

typedef struct {
    TelOptReq req;	/* requirement for the opt */
    int state;		/* current state (enabled:1 or disabled:0) */
    int pending;	/* state is pending (requested but no reply yet) */
} TelOptState;

typedef struct {
    int opt;		/* an telnet option. TELOPT_XXX */
    TelOptState local;	/* local state of the option */
    TelOptState remote;	/* remote state of the option */
} TelOptStates;

struct {
    int binsend;	/* local binary opt is enabled */
    int binrecv;	/* remote binary opt is enabled */
    int sgasend;	/* local SGA opt is enabled (char-at-a-time mode) */
    int sentReqs;	/* have sent option requests to the peer
			   or skip sending them */
    TelOptStates **stTab; /* = stTab[] in telopt.c */
} telOpt;

#define putOptCmd(s,c) { putSock1(IAC); putSock1(s); putSock1(c); }

void
telOptReset(void);
void
telOptInit(void);
void
telOptPrintCmd(const char *str, int cmd);
void
telOptSendReqs(void);
int
telOptHandle(int cmd, int opt);
int
telOptSBHandle(int opt);

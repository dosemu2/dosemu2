#include <stdio.h>	/*stderr,(sscanf,sprintf)*/
#include <string.h>	/*(strncpy)*/
#include <stdlib.h>	/*(getenv)*/
#include <sys/time.h>	/*->ttybuf.h (timeval)*/
#include <arpa/telnet.h>/*TELOPT_xxx*/

#include "defs.h"	/*->atcmd.h (uchar)*/
#include "atcmd.h"	/*atcmd*/
#include "sock.h"	/*sockIsAlive*/
#include "ttybuf.h"	/*(putTty1)*/
#include "telopt.h"	/*telOpt*/
#include "cmdlex.h"	/*(cmdLex)*/
#include "cmdarg.h"	/*cmdarg*/
#include "verbose.h"	/*VERB_MISC*/

#ifdef BINMODE_AS_DEFAULT
# define BINCMD "%B0=1%B1=1"
#else
# define BINCMD
#endif

#define INITSTR "AT" \
	"S2=43"		/* escape char = '+' */ \
	"S3=13"		/* CR */ \
	"S4=10"		/* LF */ \
	"S5=8"		/* BS */ \
	"S7=20"		/* timelimit for non-blocking connect */ \
	"S12=50"	/* escape sequence guard time */ \
	BINCMD		/* binary mode */ \
	"%T1"		/* terminal-type = $TERM */ \
	"&W"		/* write to NVRAM */

void
atcmdInit(void)
{
    Cmdstat s;

    /*memset(atcmd, 0, sizeof(atcmd));*/
    if (cmdLex(INITSTR) != CMDST_OK
	|| ( (s = cmdLex(getenv("MODEMU"))) != CMDST_OK && s != CMDST_NOAT )
	|| ( (s = cmdLex(cmdarg.atcmd)) != CMDST_OK && s != CMDST_NOAT )
	) {
	fprintf(stderr, "Error in initialization commands.\r\n");
	CHAR_CR = '\r'; /* force normal settings */
	CHAR_LF = '\n';
    }
}

/* LIT(A) -> "10" */
#define LIT_(s) #s
#define LIT(s) LIT_(s)

/* D */
/* dial command */
void
atcmdD(const char *s, AtdAType at, AtdPType pt)
{
    /*fprintf(stderr,"<%s>,%d,%d\r\n",s,at,pt);*/
    if (*s == '"') s++;
    /* "%[^:\"]:%[^\"]" */
    sscanf(s, "%" LIT(ADDR_MAX) "[^:\"]:%" LIT(PORT_MAX) "[^\"]",
	   atcmd.d.addr.str, atcmd.d.port.str);
    atcmd.d.addr.type = at;
    atcmd.d.port.type = pt;
    /*fprintf(stderr,"<%s>:<%s>\r\n",atcmd.d.addr.str, atcmd.d.port.str);*/
}

/* "x0" or "x" -> 0, "x1" -> 1, ... */
static int
getNumArg(const char *s)
{
#define isdigit(c) ('0' <= (c) && (c) <= '9')
    for ( ; *s != '\0'; s++) if (isdigit(*s)) return atoi(s);
    return 0;
}

/* fake command */
/* ("x1","012") -> 0, ("y3","012") -> 1 (just a range check) */
int
atcmdFake(const char *s, const char *vals)
{
    int i;

    i = getNumArg(s) + '0';
    for (; *vals != '\0'; vals++) if (i == *vals) return 0;
    return 1;
}

/* Hn */
/* n: 0(disconnect) */
int
atcmdH(const char *s)
{
    if (getNumArg(s) != 0) return 1;
    if (sockIsAlive()) {
	sockClose();
	verboseOut(VERB_MISC, "Connection closed with ATH.\r\n");
    }
    return 0;
}


/* In */
/* n: 4(show current settings), 5(show '&W'ed settings) or */
/*    6(show telnet option states) */

static void
prPercent(Atcmd *atcmdp)
{
    char buf[64];

    sprintf(buf, "%c%c%%B0=%d  %%B1=%d  %%D%d  %%L%d  %%R%d",
	    CHAR_CR, CHAR_LF,
	    atcmdp->pb[0], atcmdp->pb[1], atcmdp->pd, atcmdp->pl, atcmdp->pr);
    putTtyN(buf, strlen(buf));
    if (atcmdp->pt.wont) {
	putTtyStr("  %T0");
    } else {
	sprintf(buf, "  %%T=\"%s\"", atcmdp->pt.str);
	putTtyN(buf, strlen(buf));
    }
    sprintf(buf, "  %%V%d", atcmdp->pv);
    putTtyN(buf, strlen(buf));
}

static void
prSreg(uchar *s)
{
    int i;
    char buf[8];

    for (i = 0; i <= SREG_MAX; i++,s++) {
	if (i % 8 == 0) {
	    putTty1(CHAR_CR); putTty1(CHAR_LF);
	} else putTtyStr("  ");
	sprintf(buf, "S%02d=%03d", i, *s);
	putTtyN(buf, 7);
    }
}

static void
prOption(void)
{
    static char *onoff[]={"off", "on "};
    char buf[64];

    putTty1(CHAR_CR); putTty1(CHAR_LF);
    putTtyStr("MODEMU telnet option states:");
    putTty1(CHAR_CR); putTty1(CHAR_LF);
    putTty1(CHAR_CR); putTty1(CHAR_LF);
    putTtyStr(   "OPTION  LOCAL  REMOTE");
    sprintf(buf, "Binary   %s    %s",
	    onoff[telOpt.stTab[TELOPT_BINARY]->local.state],
	    onoff[telOpt.stTab[TELOPT_BINARY]->remote.state]);
    putTty1(CHAR_CR); putTty1(CHAR_LF); putTtyN(buf, strlen(buf));
    sprintf(buf, "Echo     %s    %s",
	    onoff[telOpt.stTab[TELOPT_ECHO]->local.state],
	    onoff[telOpt.stTab[TELOPT_ECHO]->remote.state]);
    putTty1(CHAR_CR); putTty1(CHAR_LF); putTtyN(buf, strlen(buf));
    sprintf(buf, "SGA      %s    %s",
	    onoff[telOpt.stTab[TELOPT_SGA]->local.state],
	    onoff[telOpt.stTab[TELOPT_SGA]->remote.state]);
    putTty1(CHAR_CR); putTty1(CHAR_LF); putTtyN(buf, strlen(buf));
    sprintf(buf, "TType    %s    %s",
	    onoff[telOpt.stTab[TELOPT_TTYPE]->local.state],
	    onoff[telOpt.stTab[TELOPT_TTYPE]->remote.state]);
    putTty1(CHAR_CR); putTty1(CHAR_LF); putTtyN(buf, strlen(buf));
    putTty1(CHAR_CR); putTty1(CHAR_LF);
}

static void
prVersion(void)
{
    putTty1(CHAR_CR); putTty1(CHAR_LF);
    putTtyStr("modemu version " LIT(VERSION_MAJOR) "." LIT(VERSION_MINOR));
    putTty1(CHAR_CR); putTty1(CHAR_LF);
}

int
atcmdI(const char *s)
{
    int idx;

    idx = getNumArg(s);
    switch (idx) {
    case 4:
	putTty1(CHAR_CR); putTty1(CHAR_LF);
	putTtyStr("MODEMU current settings:");
	putTty1(CHAR_CR); putTty1(CHAR_LF);
	prPercent(&atcmd);
	prSreg(atcmd.s);
	putTty1(CHAR_CR); putTty1(CHAR_LF);
	break;
    case 5:
	putTty1(CHAR_CR); putTty1(CHAR_LF);
	putTtyStr("MODEMU '&W'ed settings:");
	putTty1(CHAR_CR); putTty1(CHAR_LF);
	prPercent(&atcmdNV);
	prSreg(atcmdNV.s);
	putTty1(CHAR_CR); putTty1(CHAR_LF);
	break;
    case 6:
	prOption();
	break;
    case 7:
	prVersion();
	break;
    default:
	return 1;
    }
    return 0;
}



/* Sn? */
/* n: S register number */
int
atcmdSQuery(const char *s)
{
    int idx;
    char buf[4];

    idx = getNumArg(s);
    if (idx > SREG_MAX) return 1;
    putTty1(CHAR_CR); putTty1(CHAR_LF);
    sprintf(buf, "%03u", atcmd.s[idx]);
    putTtyN(buf, 3);
    putTty1(CHAR_CR); putTty1(CHAR_LF); /*at least Courier does*/
    return 0;
}

/* "1=2" -> (1,2), "1=" -> (1,0), "=2" -> (0,2), "=" -> (0,0) */
static void
getNumArg2(const char *s, int *n1p, int *n2p)
{
    char *s2;

    *n1p = strtol(s, &s2, 10);
    *n2p = strtol(s2+1, NULL, 10);
}

/* Sn=m */
/* n: S register number */
/* m: value 0-255 */
int
atcmdSSet(const char *s)
{
    int idx, val;

    getNumArg2(s+1, &idx, &val);
    if (idx > SREG_MAX || val > 255) return 1;
    atcmd.s[idx] = val;
    return 0;
}

/* Z */
/* recover &Wed settings and disconnect */
void
atcmdZ(void)
{
    atcmd = atcmdNV;
    if (sockIsAlive()) {
	sockClose();
	verboseOut(VERB_MISC, "Connection closed with ATZ.\r\n");
    }
}

/* &W */
/* save current settings */
void
atcmdAW(void)
{
    atcmdNV = atcmd;
}

/* %Bn=m */
/* n: 0(local) or 1(remote) */
/* m: 0(better non-binary), 1(better binary), */
/*    2(must non-binary) or 3(must binary) */
int
atcmdPB(const char *s)
{
    int idx, val;

    getNumArg2(s+2, &idx, &val);
    if (idx > 1 || val > 3) return 1;
    atcmd.pb[idx] = val;
    telOpt.sentReqs = 0; /* renegotiate when returning online */
    return 0;
}

/* %Dn */
/* n: 0(enable dial canceling) or 1(disable dial canceling) */
int
atcmdPD(const char *s)
{
    int i;

    i = getNumArg(s);
    if (i > 1) return 1;
    atcmd.pd = i;
    return 0;
}

/* %Ln */
/* n: 0(better char mode), 1(better line mode), */
/*    2(must char mode) or 3(must line mode) */
int
atcmdPL(const char *s)
{
    int i;

    i = getNumArg(s);
    if (i > 3) return 1;
    atcmd.pl = i;
    telOpt.sentReqs = 0; /* renegotiate when returning online */
    return 0;
}

/* %Q */
/* quit modemu */
void
atcmdPQ(void)
{
    sockShutdown(); /* may discard unsent chars in kernel,
		       or do ATH before quitting */
    exit(0);
}

/* %Rn */
/* n: 0(cooked?? mode) or 1(raw mode: 8bit thru, no IAC handling) */
/* overrides %B and %L settings */
int
atcmdPR(const char *s)
{
    int i;

    i = getNumArg(s);
    if (i > 1) return 1;
    atcmd.pr = i;
    return 0;
}

/* %Tn */
/* n: 0(dont support telnet term-type option) or */
/*    1(send $TERM for term-type option request) */
int
atcmdPT(const char *s)
{
    int i;

    i = getNumArg(s);
    switch (i) {
    case 0:
	atcmd.pt.wont = 1;
	break;
    case 1:
	strncpy(atcmd.pt.str, getenv("TERM"), PT_MAX);
	atcmd.pt.len = strlen(atcmd.pt.str);
	atcmd.pt.wont = 0;
	break;
    default:
	return 1;
    }
    return 0;
}

/* %T="xxxx" */
/* send xxxx for term-type option request */
int
atcmdPTSet(const char *s)
{
    sscanf(s+4, "%" LIT(PT_MAX) "[^\"]", atcmd.pt.str);
    /*strncpy(atcmd.pt.str, s+3, PT_MAX);*/
    atcmd.pt.len = strlen(atcmd.pt.str);
    /*telOpt.sentReqs = 0; renegotiation will be of no effect*/
    atcmd.pt.wont = 0;
    return 0;
}

/* %Vn */
/* n: 0-255 */
/*    bit0: output misc info to make up for less descriptive ATX0 indication */
/*    bit1: output telnet option negotioation */
int
atcmdPV(const char *s)
{
    int i;

    i = getNumArg(s);
    if (i > 255) return 1;
    atcmd.pv = i;
    return 0;
}

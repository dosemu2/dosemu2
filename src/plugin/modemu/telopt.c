#include <stdio.h>	/*stderr,(fprintf)*/
#include <stdlib.h>
#include <sys/time.h>	/*->ttybuf.h (timeval)*/
#define TELCMDS /*to use strings defined in telnet.h*/
#define TELOPTS
#include <arpa/telnet.h>/*IAC,DO,DONT,...*/

#include "defs.h"	/*->sockbuf.h (uchar,SOCKBUFR_SIZE,TTYBUFR_SIZE)*/
#include "sockbuf.h"	/*->telopt.h (putSock1)*/
#include "telopt.h"	/*TelOptReq,TelOptEnt*/
#include "ttybuf.h"	/*(putTtyN)*/
#include "atcmd.h"	/*atcmd*/
#include "verbose.h"	/*VERB_TELOPT*/

/* telnet option negotiation module */

static TelOptStates stTabMaster[] = {
    /*[opt]		[local]		[remote]*/
    { TELOPT_BINARY,	{TOR_BETTER},	{TOR_BETTER}	},/*0*/
    { TELOPT_ECHO,	{TOR_MUSTNOT},	{TOR_BETTER}	},/*1*/
    { TELOPT_SGA,	{TOR_BETTER},	{TOR_MUST}	},/*3*/
    { TELOPT_TTYPE,	{TOR_NEUTRAL},	{TOR_MUSTNOT}	},/*24*/
    { -1,		{TOR_MUSTNOT},	{TOR_MUSTNOT}	} /* default state */
};

TelOptStates *stTab[NTELOPTS]; /* telOptInit() makes it usable */

static /*const*/ TelOptStates *defaultSt; /* used when unknown options come */


/* must call before each telnet session begins */
void
telOptReset(void)
{
    TelOptStates *tosp;

    for (tosp = stTabMaster; tosp->opt >= 0; tosp++) {
	tosp->local.state = 
	tosp->remote.state = 0; /* all options are disabled initially */
	tosp->local.pending = 
	tosp->remote.pending = 0;
    }
    telOpt.binsend =
    telOpt.binrecv =
    telOpt.sgasend = 0;
    telOpt.sentReqs = 0;
}


/* must call once before using this module */
void
telOptInit(void)
{
    TelOptStates *tosp;
    int i;

    for (tosp = stTabMaster; tosp->opt >= 0; tosp++) ;
    for (i = 0; i < NTELOPTS; i++) stTab[i] = tosp; /* default entry */
    defaultSt = tosp;
    for (tosp-- ; tosp >= stTabMaster; tosp--) {
	stTab[tosp->opt] = tosp;
    }
    telOpt.stTab = stTab;
}


static const char *
telcmdStr(int cmd)
{
    static char str[16];

#ifndef TELCMD_FIRST /*is this rule correct for all telnet.h?*/
#define TELCMD_FIRST (256 - sizeof(telcmds)/sizeof(telcmds[0]))
#endif
    if (cmd >= TELCMD_FIRST) {
	return telcmds[cmd - TELCMD_FIRST];
    } else {
	sprintf(str, "?(%d)", cmd);
	return str;
    }
}


static const char *
teloptStr(int opt)
{
    static char str[16];

    if (opt < NTELOPTS) {
	return telopts[opt];
    } else {
	sprintf(str, "?(%d)", opt);
	return str;
    }
}


void
telOptPrintCmd(const char *str, int cmd)
{
    verboseOut(VERB_TELOPT, "%s IAC %s\r\n", str, telcmdStr(cmd));
}


static void
printCmdOpt(const char *str, int cmd, int opt)
{
    verboseOut(VERB_TELOPT, "%s %s %s\r\n", str, telcmdStr(cmd), teloptStr(opt));
}


static void
setReqs(void)
{
    static TelOptReq tabP[]
	= { TOR_BETTERNOT, TOR_BETTER, TOR_MUSTNOT, TOR_MUST };
    static TelOptReq tabN[]
	= { TOR_BETTER, TOR_BETTERNOT, TOR_MUST, TOR_MUSTNOT };

    /* %Bn=m (binary mode control) */
    stTab[TELOPT_BINARY]->local.req = tabP[atcmd.pb[1]];
    stTab[TELOPT_BINARY]->remote.req = tabP[atcmd.pb[0]];
    /* %Ln (linemode control) */
    stTab[TELOPT_SGA]->remote.req = tabN[atcmd.pl];
    stTab[TELOPT_ECHO]->remote.req = tabN[atcmd.pl];
    /* %Tn (terminal-type response control) */
    stTab[TELOPT_TTYPE]->local.req = atcmd.pt.wont? TOR_MUSTNOT : TOR_NEUTRAL;
}


/* tell the peer my option-state-to-be requests */
void
telOptSendReqs(void)
{
    TelOptStates *tosp;

    setReqs();

    for (tosp = stTabMaster; tosp->opt >= 0; tosp++) {
	switch (tosp->local.req) {
	case TOR_MUSTNOT:
	case TOR_BETTERNOT:
	    if (tosp->local.state == 1) {
		putOptCmd(WONT, tosp->opt);
		printCmdOpt(">", WONT, tosp->opt);
		tosp->local.pending = 1;
	    }
	    break;
	case TOR_BETTER:
	case TOR_MUST:
	    if (tosp->local.state == 0) {
		putOptCmd(WILL, tosp->opt);
		printCmdOpt(">", WILL, tosp->opt);
		tosp->local.pending = 1;
	    }
	    break;
	default:;
	}	    
	switch (tosp->remote.req) {
	case TOR_MUSTNOT:
	case TOR_BETTERNOT:
	    if (tosp->remote.state == 1) {
		putOptCmd(DONT, tosp->opt);
		printCmdOpt(">", DONT, tosp->opt);
		tosp->remote.pending = 1;
	    }
	    break;
	case TOR_BETTER:
	case TOR_MUST:
	    if (tosp->remote.state == 0) {
		putOptCmd(DO, tosp->opt);
		printCmdOpt(">", DO, tosp->opt);
		tosp->remote.pending = 1;
	    }
	    break;
	default:;
	}	    
    }
    telOpt.sentReqs = 1;
}


/* summarize option states into flags */
static void
telOptSummarize(void)
{
    telOpt.binsend = stTab[TELOPT_BINARY]->local.state;
    telOpt.binrecv = stTab[TELOPT_BINARY]->remote.state;
    telOpt.sgasend = stTab[TELOPT_SGA]->remote.state;
}


/* telnet option request/response handling */
int
telOptHandle(int cmd, int opt)
{
    TelOptState *tostp;
    TelOptStates *tosp;
    int reqState;	/* cmd's requiring state */
    int posiResCmd;	/* positive response command for cmd */
    int negaResCmd;	/* negative response command for cmd */
    TelOptReq mustNegate;	/* must negate if req is this */
    TelOptReq betterNegate;	/* better negate if req is this */
//    TelOptReq betterAssert;	/* better assert if req is this */
//    TelOptReq mustAssert;	/* must assert if req is this */

    printCmdOpt("<", cmd, opt);

    tosp = (opt < NTELOPTS)? stTab[opt] : defaultSt;

    switch (cmd) {
    case WILL:
	tostp = &tosp->remote;
	reqState = 1;
	mustNegate = TOR_MUSTNOT;
	betterNegate = TOR_BETTERNOT;
//	betterAssert = TOR_BETTER;
//	mustAssert = TOR_MUST;
	posiResCmd = DO;
	negaResCmd = DONT;
	break;
    case WONT:
	tostp = &tosp->remote;
	reqState = 0;
	mustNegate = TOR_MUST;
	betterNegate = TOR_BETTER;
//	betterAssert = TOR_BETTERNOT;
//	mustAssert = TOR_MUSTNOT;
	posiResCmd = DONT;
	negaResCmd = DO;
	break;
    case DO:
	tostp = &tosp->local;
	reqState = 1;
	mustNegate = TOR_MUSTNOT;
	betterNegate = TOR_BETTERNOT;
//	betterAssert = TOR_BETTER;
//	mustAssert = TOR_MUST;
	posiResCmd = WILL;
	negaResCmd = WONT;
	break;
    case DONT:
	tostp = &tosp->local;
	reqState = 0;
	mustNegate = TOR_MUST;
	betterNegate = TOR_BETTER;
//	betterAssert = TOR_BETTERNOT;
//	mustAssert = TOR_MUSTNOT;
	posiResCmd = WONT;
	negaResCmd = WILL;
	break;
    default:
	fprintf(stderr, "bug\r\n");
	exit(1);
    }

    if (tostp->req == mustNegate || tostp->req == betterNegate) {
	if (tostp->pending) {
	    tostp->pending = 0;
	    if (tostp->req == mustNegate)
		return 1; /* requirment didn't meet */
	    if (tostp->state == !reqState) { /* this may not happen */
		tostp->state = reqState;
		putOptCmd(posiResCmd, opt); /* positive response */
		printCmdOpt(">", posiResCmd, opt);
	    }
	} else {
	    putOptCmd(negaResCmd, opt); /* negative response */
	    printCmdOpt(">", negaResCmd, opt);
	}
    } else /*if (tostp->req == betterAssert or mustAssert or TOR_NEUTRAL)*/ {
	if (tostp->pending) {
	    tostp->pending = 0;
	    /* don't response because cmd is the response of my request */
	} else {
          if (tostp->state == !reqState) { /* this may not happen */
                putOptCmd(posiResCmd, opt); /* positive response */
                printCmdOpt(">", posiResCmd, opt);
            }
	}
	tostp->state = reqState; /* {en,dis}able option as requested */
    }

    telOptSummarize();
    return 0;
}

/* send term-type subnego param */
static void
ttypeSBHandle(void)
{
    putSock1(IAC);
    putSock1(SB);
    putSock1(TELOPT_TTYPE);
    putSock1(TELQUAL_IS);
    putSockN(atcmd.pt.str, atcmd.pt.len);
    putSock1(IAC);
    putSock1(SE);
    verboseOut(VERB_TELOPT, "> SB %s IS %s SE\r\n",
	     telopts[TELOPT_TTYPE], atcmd.pt.str);
}

/* telnet option subnegotiation request handling */
int
telOptSBHandle(int opt)
{
    verboseOut(VERB_TELOPT, "< SB %s SEND SE.\r\n", telopts[opt]);

    switch (opt) {
    case TELOPT_TTYPE:
	ttypeSBHandle();
	break;
    default:
	return 1;
    }
    return 0;
}

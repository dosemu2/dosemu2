#if 0
#include "defs.h"	/*->atcmd.h (uchar)*/
#endif

typedef enum {
    ATDA_NUM,
    ATDA_STR
} AtdAType;

typedef enum {
    ATDP_NUL,
    ATDP_NUM,
    ATDP_STR
} AtdPType;

#define ADDR_MAX 63
#define PORT_MAX 63
#define PT_MAX 40
#define SREG_MAX 12

typedef struct {
    struct {
	struct {
	    char str[ADDR_MAX+1];
	    AtdAType type;
	} addr;
	struct {
	    char str[PORT_MAX+1];
	    AtdPType type;
	} port;
    } d;
    int f;
    uchar s[SREG_MAX+1];
    int pb[2];
    int pd;
    int pl;
    int pr;
    struct {
	char str[PT_MAX+1];
	int len;
	int wont;
    } pt;
    int pv;
} Atcmd;

Atcmd atcmd, atcmdNV;

#define CHAR_ESC (atcmd.s[2])
#define CHAR_CR (atcmd.s[3])
#define CHAR_LF (atcmd.s[4])
#define CHAR_BS (atcmd.s[5])

void
atcmdInit(void);
void
atcmdD(const char *s, AtdAType at, AtdPType pt);
int
atcmdFake(const char *s, const char *vals);
int
atcmdH(const char *s);
int
atcmdI(const char *s);
int
atcmdSQuery(const char *s);
int
atcmdSSet(const char *s);
void
atcmdZ(void);
void
atcmdAW(void);
int
atcmdPB(const char *s);
int
atcmdPD(const char *s);
int
atcmdPL(const char *s);
void
atcmdPQ(void);
int
atcmdPR(const char *s);
int
atcmdPT(const char *s);
int
atcmdPTSet(const char *s);
int
atcmdPV(const char *s);

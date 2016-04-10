#if 0
#include "defs.h"	/*->sockbuf.h (uchar,SOCKBUFR_SIZE,TTYBUFR_SIZE)*/
#endif


/* reading socket */

struct {
    uchar buf[SOCKBUFR_SIZE];
    uchar *ptr;
    uchar *end;
} sockBufR;

#define sockBufRReset() { sockBufR.ptr = sockBufR.end = sockBufR.buf; }
#define getSock1() ((sockBufR.ptr >= sockBufR.end)? -1 : *sockBufR.ptr++)

void
sockBufRead(void);


/* writing socket */

#define SOCKBUFW_SIZE (2 * TTYBUFR_SIZE) /* this seems to be any number */
#define SOCKBUFW_SIZE_A (SOCKBUFW_SIZE + TTYBUFR_SIZE) /* important */

struct {
    uchar buf[SOCKBUFW_SIZE_A];
    uchar *top;
    uchar *ptr;
    int stop;
} sockBufW;

#define sockBufWReset() { \
    sockBufW.ptr = sockBufW.top = sockBufW.buf; \
    sockBufW.stop = 0; \
}
#define sockBufWHasData() (sockBufW.ptr > sockBufW.buf)
#define sockBufWReady() (!sockBufW.stop)

void
sockBufWrite(void);
void
putSock1(uchar c);
void
putSockN(const char *cp, int n);

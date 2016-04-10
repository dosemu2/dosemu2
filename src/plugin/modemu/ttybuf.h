#if 0
#include <sys/time.h>	/*->ttybuf.h (timeval)*/
#include "defs.h"	/*->ttybuf.h (uchar,SOCKBUFR_SIZE,TTYBUFR_SIZE)*/
#endif

struct {
    int rfd;
    int wfd;
} tty;


/* reading tty */

struct {
    uchar buf[TTYBUFR_SIZE];
    uchar *ptr;
    uchar *end;
    struct timeval newT;
    struct timeval prevT;
} ttyBufR;

#define ttyBufRReset() \
{ \
    ttyBufR.ptr = ttyBufR.end = ttyBufR.buf; \
    ttyBufR.prevT.tv_sec = ttyBufR.prevT.tv_usec = 0; \
}
#define getTty1() ((ttyBufR.ptr >= ttyBufR.end)? -1 : *ttyBufR.ptr++)

void
ttyBufRead(void);


/* writing tty */

#define TTYBUFW_SIZE (2 * SOCKBUFR_SIZE) /* this seems to be any number */
#define TTYBUFW_SIZE_A (TTYBUFW_SIZE + SOCKBUFR_SIZE) /* important */

struct {
    uchar buf[TTYBUFW_SIZE_A];
    uchar *top;
    uchar *ptr;
    int stop;
} ttyBufW;

#define ttyBufWReset() { \
    ttyBufW.ptr = ttyBufW.top = ttyBufW.buf; \
    ttyBufW.stop = 0; \
}
#define ttyBufWHasData() (ttyBufW.ptr > ttyBufW.buf)
#define ttyBufWReady() (!ttyBufW.stop)
#define putTtyStr(s) putTtyN(s, sizeof(s)-1)

void
ttyBufWrite(void);
void
putTty1(uchar c);
void
putTtyN(const char *cp, int n);

#include <sys/time.h>	/*timeval*/
#include "timeval.h"	/*(timevalXXX)*/

/* a = b [10mS] */
void
timevalSet10ms(struct timeval *ap, int b)
{
    ap->tv_sec = b / 100;
    ap->tv_usec = (b % 100) * 10*1000;
}

/* a += b */
void
timevalAdd(struct timeval *ap, const struct timeval *bp)
{
    ap->tv_sec += bp->tv_sec;
    ap->tv_usec += bp->tv_usec;
    if (ap->tv_usec >= 1000*1000) {
	ap->tv_usec -= 1000*1000;
	ap->tv_sec += 1;
    }
}

/* a -= b */
void
timevalSub(struct timeval *ap, const struct timeval *bp)
{
    ap->tv_sec -= bp->tv_sec;
    ap->tv_usec -= bp->tv_usec;
    if (ap->tv_usec < 0) {
	ap->tv_usec += 1000*1000;
	ap->tv_sec -= 1;
    }
}

/* (a < b): -1, (a==b): 0, (a > b): 1 */
int
timevalCmp(const struct timeval *ap, const struct timeval *bp)
{
    if (ap->tv_sec < bp->tv_sec) return -1;
    if (ap->tv_sec > bp->tv_sec) return 1;
    if (ap->tv_usec < bp->tv_usec) return -1;
    if (ap->tv_usec > bp->tv_usec) return 1;
    return 0;
}

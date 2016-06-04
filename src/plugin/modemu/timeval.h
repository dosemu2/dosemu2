#if 0
#include <sys/time.h>	/*->timeval.h (timeval)*/
#endif

void
timevalSet10ms(struct timeval *ap, int b);
void
timevalAdd(struct timeval *ap, const struct timeval *bp);
void
timevalSub(struct timeval *ap, const struct timeval *bp);
int
timevalCmp(const struct timeval *ap, const struct timeval *bp);

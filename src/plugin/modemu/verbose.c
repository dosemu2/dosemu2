#include <stdio.h>	/*stderr*/
#include <stdarg.h>	/*va_list*/
#include "verbose.h"	/*(verboseOut)*/
#include "defs.h"	/*->atcmd.h (uchar)*/
#include "atcmd.h"	/*atcmd*/

void
verboseOut(int mask, const char *format, ...)
{
    va_list vl;

    if (!(atcmd.pv & mask)) return;
    va_start(vl, format);
    vfprintf(stderr, format, vl);
    va_end(vl);
}

void
verbosePerror(int mask, const char *s)
{
    if (!(atcmd.pv & mask)) return;
    perror(s);
}

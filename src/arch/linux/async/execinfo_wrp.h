/* execinfo.h compatibility wrapper */

#include "config.h"

#ifdef HAVE_EXECINFO
#include <execinfo.h>
#else
#ifdef HAVE_LIBBFD
/* we provide our own impl of those */
int backtrace(void **buffer, int size);
char **backtrace_symbols(void *const *buffer, int size);
void backtrace_symbols_fd(void *const *buffer, int size, int fd);
#endif
#endif

/* in case of missing <linux/vm86.h> for kernels > 1.3.99 GCC falls back
 * to this file (we have -Isrc/include in all makefiles).
 * But NOTE: If the kernel has been upgraded by patch, then <linux/vm86.h>
 *           will be existing _and_ have ZERO size !
 *           In this case DELETE <linux/vm86.h>.
 */
#include "kversion.h"
#if KERNEL_VERSION > 1003099
#include <asm/vm86.h>
#else
#include "/usr/include/linux/vm86.h"
#endif

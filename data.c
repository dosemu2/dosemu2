#define EXTERN 
#define INIT(x...)		=  ## x

#include <sys/types.h>
#include "emu.h"
#include "xms.h"
#include "disks.h"
#include "timers.h"
#include "int.h"
#include "dosio.h"
#include "termio.h"
#include "machcompat.h"
#include "vc.h"
#include "video.h"
#include "mouse.h"
#include "dpmi/dpmi.h"

#define EXTERN 
#define INIT(x...)		=  ## x

static char *id= "$Id: data.c,v 1.3 1995/02/25 22:38:20 root Exp root $";

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
#include "bios.h"
#include "dpmi.h"
#include "shared.h"
#include "pic.h"
#include "disks.h"

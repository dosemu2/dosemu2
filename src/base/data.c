#define EXTERN 
#define INIT(x...)		=  ## x

#ifndef lint
static char *id= "$Id: data.c,v 1.4 1995/04/08 22:29:37 root Exp $";
#endif

#include <termios.h>
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
#include "../env/video/vga.h" /* NOTE: there exists also /usr/include/vga.h !! */
#include "video.h"
#include "mouse.h"
#include "bios.h"
#include "dpmi.h"
#include "shared.h"
#include "pic.h"
#include "disks.h"
#include "mhpdbg.h"
#include "cmos.h"
#include "priv.h"
#include "dma.h"
#include "sound.h"
#include "serial.h"
#include "dosemu_debug.h"
#include "keyb_server.h"
#include "keyb_clients.h"

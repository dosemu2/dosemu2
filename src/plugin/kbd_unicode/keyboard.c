/* 
 * DANG_BEGIN_MODULE
 * 
 * Description: Keyboard coordinator
 * 
 * Maintainer: Eric Biederman <ebiederm@xmission.com>
 * 
 * REMARK
 * This module coordinates the initialization of the keyboard.
 *
 * /REMARK
 * DANG_END_MODULE
 *
 */

#include "config.h"
#include "emu.h"
#include "init.h"
#include "iodev.h"

#include "keyboard.h"
#include "keyb_clients.h"
#include "keyb_server.h"


void keyb_priv_init(void)
{
	/* this must be initialized before starting port-server */
	keyb_8042_init();
}

void keyb_init(void) 
{
	if (!keyb_server_init()) {
		error("can't init keyboard server\n");
		leavedos(19);
	}
	if (!keyb_client_init()) {
		error("can't open keyboard client\n");
		leavedos(19);
	}
}

void keyb_reset(void)
{
	keyb_8042_reset();
	keyb_server_reset();
	keyb_client_reset();
}

void keyb_close(void)
{
#if 0
	keyb_8042_close();
#endif
	keyb_server_close();
	keyb_client_close();
}

CONSTRUCTOR(static void init(void))
{
	iodev_register("keyb", keyb_init, keyb_reset, keyb_close);
}

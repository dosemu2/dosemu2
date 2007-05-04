/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* This file contains the dummy keyboard client. 
 * It does nothing.
 */

#include "keyboard.h"
#include "keyb_clients.h"
#include "utilities.h"

/* DANG_BEGIN_FUNCTION none_probe
 *
 * Succeed if we can run the dummy keyboard client, (we always can).
 * but first try the other fall-back (slang keyboard)
 *
 * DANG_END_FUNCTION
 */

static int none_probe(void)
{
	return !load_plugin("term");
}

struct keyboard_client Keyboard_none = 
{
	"No keyboard",	/* name */
	none_probe,	/* probe */
	NULL,		/* init */
	NULL,		/* reset */
	NULL,		/* close */
	NULL,		/* run */
	NULL,		/* set_leds */
};


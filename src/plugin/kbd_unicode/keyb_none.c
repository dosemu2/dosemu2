/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * This file contains the dummy keyboard client.
 *
 * Author: Stas Sergeev
 */

#include <unistd.h>
#include <fcntl.h>
#include "keyboard.h"
#include "keyb_clients.h"
#include "utilities.h"

static int old_fl;

/* DANG_BEGIN_FUNCTION none_probe
 *
 * Succeed if we can run the dummy keyboard client, (we always can).
 *
 * DANG_END_FUNCTION
 */

static int none_probe(void)
{
	return 1;
}

static int none_init(void)
{
	old_fl = fcntl(STDIN_FILENO, F_GETFL);
	fcntl(STDIN_FILENO, F_SETFL, old_fl | O_NONBLOCK);
	add_to_io_select(STDIN_FILENO, keyb_client_run_async, NULL);
	return 1;
}

static void none_close(void)
{
	remove_from_io_select(STDIN_FILENO);
	fcntl(STDIN_FILENO, F_SETFL, old_fl);
}

static void none_run(void)
{
	char buf[256];
	int rc;

	rc = read(STDIN_FILENO, buf, sizeof(buf));
	if (rc > 0)
		paste_text(buf, rc, "utf8");
}

struct keyboard_client Keyboard_none =
{
	"No keyboard",	/* name */
	none_probe,	/* probe */
	none_init,	/* init */
	NULL,		/* reset */
	none_close,	/* close */
	none_run,	/* run */
	NULL,		/* set_leds */
};

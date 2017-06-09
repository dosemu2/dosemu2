/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * stdio keyboard client.
 *
 * Author: Stas Sergeev
 */

#include <unistd.h>
#include "keyb_clients.h"

static void none_run(void *arg);

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
	add_to_io_select(STDIN_FILENO, none_run, NULL);
	return 1;
}

static void none_close(void)
{
	remove_from_io_select(STDIN_FILENO);
}

static void none_run(void *arg)
{
	char buf[256];
	int rc;
	rc = read(STDIN_FILENO, buf, sizeof(buf));
	if (rc > 0)
		paste_text(buf, rc, "utf8");
}

struct keyboard_client Keyboard_none =
{
	"stdio",	/* name */
	none_probe,	/* probe */
	none_init,	/* init */
	NULL,		/* reset */
	none_close,	/* close */
	NULL,		/* set_leds */
};

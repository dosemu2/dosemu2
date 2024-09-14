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
#include <errno.h>
#include "ioselect.h"
#include "keyb_clients.h"

static void stdio_kbd_run(int fd, void *arg);

/* DANG_BEGIN_FUNCTION stdio_kbd_probe
 *
 * Succeed if we can run the dummy keyboard client, (we always can).
 *
 * DANG_END_FUNCTION
 */

static int stdio_kbd_probe(void)
{
	if (no_local_video)
		return 0;
	return 1;
}

static int stdio_kbd_init(void)
{
	add_to_io_select(STDIN_FILENO, stdio_kbd_run, NULL);
	return 1;
}

static void stdio_kbd_close(void)
{
	remove_from_io_select(STDIN_FILENO);
}

static void stdio_kbd_run(int fd, void *arg)
{
	char buf[256];
	int rc;
	rc = read(STDIN_FILENO, buf, sizeof(buf));
	switch (rc) {
	case 0:
		error("kbd: EOF from stdin\n");
		return;
	case -1:
		error("kbd: error reading stdin: %s\n", strerror(errno));
		return;
	default:
		ioselect_complete(fd);
		break;
	}
	paste_text(buf, rc, "utf8");
}

struct keyboard_client Keyboard_stdio =
{
	"stdio",	/* name */
	stdio_kbd_probe,	/* probe */
	stdio_kbd_init,	/* init */
	NULL,		/* reset */
	stdio_kbd_close,	/* close */
	NULL,		/* set_leds */
};


static int none_kbd_probe(void)
{
	return 1;
}

static int none_kbd_init(void)
{
	return 1;
}

static void none_kbd_close(void)
{
}

struct keyboard_client Keyboard_none =
{
	"none",	/* name */
	none_kbd_probe,	/* probe */
	none_kbd_init,	/* init */
	NULL,		/* reset */
	none_kbd_close,	/* close */
	NULL,		/* set_leds */
};

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * swappable mouse drivers
 *
 * Author: Stas Sergeev
 */
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "emu.h"
#include "serial.h"
#include "utilities.h"
#include "mouse.h"


struct mouse_drv_wrp {
  struct mouse_drv *drv;
  struct mouse_drv_wrp *next;
  void *udata;
  int initialized;
};
static struct mouse_drv_wrp *mdrv;

struct mouse_client_wrp {
  struct mouse_client *clnt;
  int initialized;
};
#define MAX_MOUSE_CLIENTS 20
static struct mouse_client_wrp Mouse[MAX_MOUSE_CLIENTS];
static int mclnt_num;

/* register mouse at the back of the linked list */
void register_mouse_client(struct mouse_client *mouse)
{
	assert(mclnt_num < MAX_MOUSE_CLIENTS);
	Mouse[mclnt_num++].clnt = mouse;
}

#define mouse_client_f(t, f) \
t mouse_client_##f(void) \
{ \
	int i; \
	for (i = 0; i < mclnt_num; i++) { \
		if (!Mouse[i].initialized || !Mouse[i].clnt->f) \
			continue; \
		Mouse[i].clnt->f(); \
	} \
}
mouse_client_f(void, close)
mouse_client_f(void, post_init)
void mouse_client_show_cursor(int yes)
{
	int i;
	for (i = 0; i < mclnt_num; i++) {
		if (!Mouse[i].initialized || !Mouse[i].clnt->show_cursor)
			continue;
		Mouse[i].clnt->show_cursor(yes);
	}
}

void register_mouse_driver(struct mouse_drv *mouse)
{
	struct mouse_drv_wrp *m, *ms;
	ms = malloc(sizeof(*ms));
	ms->drv = mouse;
	ms->udata = NULL;
	ms->initialized = 0;
	ms->next = NULL;
	if (mdrv == NULL)
		mdrv = ms;
	else {
		for (m = mdrv; m->next; m = m->next);
		m->next = ms;
	}
}

void mousedrv_set_udata(const char *name, void *udata)
{
	struct mouse_drv_wrp *m;
	for (m = mdrv; m->next; m = m->next) {
		if (strcmp(name, m->drv->name) == 0) {
			m->udata = udata;
			break;
		}
	}
}

static int none_init(void)
{
  return TRUE;
}

static struct mouse_client Mouse_none =  {
  "No Mouse",   /* name */
  none_init,	/* init */
  NULL,		/* close */
  NULL
};

static void mouse_client_init(void)
{
  int i;

#ifdef USE_GPM
  load_plugin("gpm");
#endif
  register_mouse_client(&Mouse_raw);
  register_mouse_client(&Mouse_none);
  for (i = 0; i < mclnt_num; i++) {
    m_printf("MOUSE: initialising '%s' mode mouse client\n",
             Mouse[i].clnt->name);

    Mouse[i].initialized = Mouse[i].clnt->init ? Mouse[i].clnt->init() : TRUE;
    if (Mouse[i].initialized) {
      m_printf("MOUSE: Mouse init ok, '%s' mode\n", Mouse[i].clnt->name);
    }
    else {
      m_printf("MOUSE: Mouse init ***failed***, '%s' mode\n",
               Mouse[i].clnt->name);
    }
  }
}

void dosemu_mouse_init(void)
{
  struct mouse_drv_wrp *m;
  for (m = mdrv; m; m = m->next) {
    if (!m->drv->init || m->drv->init())
      m->initialized = 1;
  }

  mouse_client_init();
}

#define AR(...) (__VA_ARGS__, m->udata)
#define MOUSE_DO(n, DEF, ARGS) \
void mouse_##n DEF \
{ \
  struct mouse_drv_wrp *m; \
  for (m = mdrv; m; m = m->next) { \
    struct mouse_drv *d; \
    if (!m->initialized) \
      continue; \
    d = m->drv; \
    if (d->n && d->accepts(m->udata)) \
	d->n AR ARGS; \
  } \
}
MOUSE_DO(move_buttons, (int lbutton, int mbutton, int rbutton),
	(lbutton, mbutton, rbutton))
MOUSE_DO(move_relative, (int dx, int dy, int x_range, int y_range),
	(dx, dy, x_range, y_range))
MOUSE_DO(move_mickeys, (int dx, int dy), (dx, dy))
MOUSE_DO(move_absolute, (int x, int y, int x_range, int y_range),
	(x, y, x_range, y_range))
MOUSE_DO(drag_to_corner, (int x_range, int y_range), (x_range, y_range))
MOUSE_DO(enable_native_cursor, (int flag), (flag))

int mousedrv_accepts(const char *id)
{
  struct mouse_drv_wrp *m;
  for (m = mdrv; m; m = m->next) {
    struct mouse_drv *d;
    if (!m->initialized)
      continue;
    d = m->drv;
    if (strcmp(d->name, id) != 0)
      continue;
    if (d->accepts)
	return d->accepts(m->udata);
  }
  return 0;
}

#define DID(...) (__VA_ARGS__, const char *id)
#define MOUSE_ID_DO(n, DEF, ARGS) \
void mouse_##n##_id DID DEF \
{ \
  struct mouse_drv_wrp *m; \
  for (m = mdrv; m; m = m->next) { \
    struct mouse_drv *d; \
    if (!m->initialized) \
      continue; \
    d = m->drv; \
    if (strcmp(d->name, id) != 0) \
      continue; \
    /* not checking for .accpets */ \
    if (d->n) \
	d->n AR ARGS; \
  } \
}
MOUSE_ID_DO(move_buttons, (int lbutton, int mbutton, int rbutton),
	(lbutton, mbutton, rbutton))
MOUSE_ID_DO(move_mickeys, (int dx, int dy), (dx, dy))
MOUSE_ID_DO(enable_native_cursor, (int flag), (flag))

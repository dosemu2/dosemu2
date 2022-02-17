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
#include "ringbuf.h"
#include "mouse.h"


struct bdrv_s {
  struct mouse_drv *drv;
  void *udata;
  struct rng_s buf;
  void (*callback)(void *);
  void **arg_p;
};

struct mouse_drv_wrp {
  struct mouse_drv *drv;
  struct mouse_drv_wrp *next;
  void *udata;
  int initialized;
  struct bdrv_s bdrv;
};
static struct mouse_drv_wrp *mdrv;

enum MsApiIds {
  MID_move_button,
  MID_move_buttons,
  MID_move_wheel,
  MID_move_relative,
  MID_move_mickeys,
  MID_move_absolute,
  MID_drag_to_corner,
  MID_enable_native_cursor,
};

#define B_MAX_ARGS 10
struct mbuf_s {
  enum MsApiIds id;
  int args[B_MAX_ARGS];
};

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

#define mouse_client_f(t, f, p, c) \
t mouse_client_##f(p) \
{ \
	int i; \
	for (i = 0; i < mclnt_num; i++) { \
		if (!Mouse[i].initialized || !Mouse[i].clnt->f) \
			continue; \
		Mouse[i].clnt->f c; \
	} \
}
mouse_client_f(void, close, void, ())
mouse_client_f(void, post_init, void, ())
mouse_client_f(void, show_cursor, int yes, (yes))
mouse_client_f(void, reset, void, ())

static struct mouse_drv_wrp *do_create_driver(struct mouse_drv *mouse)
{
	struct mouse_drv_wrp *ms;

	ms = malloc(sizeof(*ms));
	ms->drv = mouse;
	ms->bdrv.drv = NULL;
	ms->udata = NULL;
	ms->initialized = 0;
	ms->next = NULL;
	return ms;
}

void register_mouse_driver(struct mouse_drv *mouse)
{
	struct mouse_drv_wrp *m, *ms;

	ms = do_create_driver(mouse);
	if (mdrv == NULL) {
		mdrv = ms;
	} else {
		for (m = mdrv; m->next; m = m->next);
		m->next = ms;
	}
}

void mousedrv_set_udata(const char *name, void *udata)
{
	struct mouse_drv_wrp *m;
	for (m = mdrv; m; m = m->next) {
		if (strcmp(name, m->drv->name) == 0) {
			m->udata = udata;
			break;
		}
	}
}

static void fifo_mdrv_add(struct bdrv_s *r, struct mbuf_s *b)
{
  if (!rng_put(&r->buf, b))
    error("mouse queue overflow\n");
  r->callback(*r->arg_p);
}

#define MOUSE_BO(n, DEF, ...) \
static void fifo_mdrv_##n DEF \
{ \
  struct mbuf_s b = { MID_##n, {__VA_ARGS__} }; \
  fifo_mdrv_add(udata, &b); \
}

MOUSE_BO(move_button, (int num, int press, void *udata),
	num, press)
MOUSE_BO(move_buttons, (int lbutton, int mbutton, int rbutton, void *udata),
	lbutton, mbutton, rbutton)
MOUSE_BO(move_wheel, (int dy, void *udata), dy)
MOUSE_BO(move_relative, (int dx, int dy, int x_range, int y_range,
	void *udata),
	dx, dy, x_range, y_range)
MOUSE_BO(move_mickeys, (int dx, int dy, void *udata), dx, dy)
MOUSE_BO(move_absolute, (int x, int y, int x_range, int y_range, int vis,
	void *udata),
	x, y, x_range, y_range, vis)
MOUSE_BO(drag_to_corner, (int x_range, int y_range, void *udata),
	x_range, y_range)
MOUSE_BO(enable_native_cursor, (int flag, void *udata), flag)

struct mouse_drv fifo_mdrv = {
    .move_button = fifo_mdrv_move_button,
    .move_buttons = fifo_mdrv_move_buttons,
    .move_wheel = fifo_mdrv_move_wheel,
    .move_relative = fifo_mdrv_move_relative,
    .move_mickeys = fifo_mdrv_move_mickeys,
    .move_absolute = fifo_mdrv_move_absolute,
    .drag_to_corner = fifo_mdrv_drag_to_corner,
    .enable_native_cursor = fifo_mdrv_enable_native_cursor,
    .name = "fifo helper",
};

static void fifo_mdrv_init(struct mouse_drv_wrp *m, void (*cb)(void *))
{
#define M_FIFO_LEN 1024
    m->bdrv.drv = &fifo_mdrv;
    rng_init(&m->bdrv.buf, M_FIFO_LEN, sizeof(struct mbuf_s));
    m->bdrv.udata = &m->bdrv;
    m->bdrv.callback = cb;
    m->bdrv.arg_p = &m->udata;
}

static int do_process_fifo(struct mouse_drv_wrp *m)
{
    struct mbuf_s b;
    struct mouse_drv *d = m->drv;

    assert(m->bdrv.drv);
    if (!rng_get(&m->bdrv.buf, &b))
        return 0;
    switch (b.id) {
#define MP(n, ...) \
    case MID_##n: \
    d->n(__VA_ARGS__, m->udata); \
    break

    MP(move_button, b.args[0], b.args[1]);
    MP(move_buttons, b.args[0], b.args[1], b.args[2]);
    MP(move_wheel, b.args[0]);
    MP(move_relative, b.args[0], b.args[1], b.args[2], b.args[3]);
    MP(move_mickeys, b.args[0], b.args[1]);
    MP(move_absolute, b.args[0], b.args[1], b.args[2], b.args[3], b.args[4]);
    MP(drag_to_corner, b.args[0], b.args[1]);
    MP(enable_native_cursor, b.args[0]);
    }
    return rng_count(&m->bdrv.buf);
}

int mousedrv_process_fifo(const char *name)
{
    struct mouse_drv_wrp *m;
    for (m = mdrv; m; m = m->next) {
        if (strcmp(name, m->drv->name) == 0)
            return do_process_fifo(m);
    }
    return 0;
}

void register_mouse_driver_buffered(struct mouse_drv *mouse,
	void (*cb)(void *))
{
	struct mouse_drv_wrp *m, *ms;

	ms = do_create_driver(mouse);
	fifo_mdrv_init(ms, cb);
	if (mdrv == NULL) {
		mdrv = ms;
	} else {
		for (m = mdrv; m->next; m = m->next);
		m->next = ms;
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
  if (config.term)
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

#define MOUSE_DO(n, DEF, ...) \
void mouse_##n DEF \
{ \
  struct mouse_drv_wrp *m; \
  for (m = mdrv; m; m = m->next) { \
    struct mouse_drv *d, *b; \
    void *ud; \
    if (!m->initialized) \
      continue; \
    d = m->drv; \
    b = m->bdrv.drv ?: d; \
    ud = m->bdrv.drv ? m->bdrv.udata : m->udata; \
    if (d->n && d->accepts(from, m->udata)) \
	b->n(__VA_ARGS__, ud); \
  } \
}
MOUSE_DO(move_button, (int num, int press, int from), num, press)
MOUSE_DO(move_buttons, (int lbutton, int mbutton, int rbutton, int from),
	lbutton, mbutton, rbutton)
MOUSE_DO(move_wheel, (int dy, int from), dy)
MOUSE_DO(move_relative, (int dx, int dy, int x_range, int y_range, int from),
	dx, dy, x_range, y_range)
MOUSE_DO(move_mickeys, (int dx, int dy, int from), dx, dy)
MOUSE_DO(move_absolute, (int x, int y, int x_range, int y_range, int vis,
	int from),
	x, y, x_range, y_range, vis)
MOUSE_DO(drag_to_corner, (int x_range, int y_range, int from),
	x_range, y_range)
MOUSE_DO(enable_native_cursor, (int flag, int from), flag)

int mousedrv_accepts(const char *id, int from)
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
      return d->accepts(from, m->udata);
  }
  return 0;
}

#define DID(...) (__VA_ARGS__, const char *id)
#define MOUSE_ID_DO(n, DEF, ...) \
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
	d->n(__VA_ARGS__, m->udata); \
  } \
}
MOUSE_ID_DO(move_button, (int num, int press),
	num, press)
MOUSE_ID_DO(move_buttons, (int lbutton, int mbutton, int rbutton),
	lbutton, mbutton, rbutton)
MOUSE_ID_DO(move_wheel, (int dy), dy)
MOUSE_ID_DO(move_mickeys, (int dx, int dy), dx, dy)
MOUSE_ID_DO(enable_native_cursor, (int flag), flag)

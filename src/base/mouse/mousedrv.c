#include <stdlib.h>
#include "emu.h"
#include "serial.h"
#include "utilities.h"
#include "mouse.h"

static struct mouse_drv_wrp *mdrv, *cur_drv;

struct mouse_client *Mouse = NULL;

/* register mouse at the back of the linked list */
void register_mouse_client(struct mouse_client *mouse)
{
	struct mouse_client *m;

	mouse->next = NULL;
	if (Mouse == NULL)
		Mouse = mouse;
	else {
		for (m = Mouse; m->next; m = m->next);
		m->next = mouse;
	}
}

void register_mouse_driver(struct mouse_drv *mouse, void *udata)
{
	struct mouse_drv_wrp *m, *ms;
	ms = malloc(sizeof(*ms));
	ms->drv = mouse;
	ms->udata = udata;
	ms->next = NULL;
	if (mdrv == NULL)
		mdrv = ms;
	else {
		for (m = mdrv; m->next; m = m->next);
		m->next = ms;
	}
}

static int serial_mouse_init(void)
{
  int x;
  serial_t *sptr=NULL;
  for (x=0;x<config.num_ser;x++){
    sptr = &com_cfg[x];
    if (sptr->mouse) break;
  }
  if (!sptr || !(sptr->mouse)) {
    m_printf("MOUSE: No mouse configured in serial config! num_ser=%d\n",config.num_ser);
    return FALSE;
  }
  m_printf("MOUSE: Mouse configured in serial config! num_ser=%d\n",config.num_ser);
  config.mouse.intdrv = FALSE;
  return TRUE;
}

static struct mouse_client Mouse_serial =  {
  "Serial Mouse",   /* name */
  serial_mouse_init,	/* init */
  NULL,		/* close */
  NULL,		/* run */
  NULL
};

static int none_init(void)
{
  return TRUE;
}

static struct mouse_client Mouse_none =  {
  "No Mouse",   /* name */
  none_init,	/* init */
  NULL,		/* close */
  NULL,		/* run */
  NULL
};

static void mouse_client_init(void)
{
  int ok;

#ifdef USE_GPM
  if (Mouse == NULL)
    load_plugin("gpm");
#endif
  register_mouse_client(&Mouse_serial);
  register_mouse_client(&Mouse_raw);
  register_mouse_client(&Mouse_none);
  while(TRUE) {
    m_printf("MOUSE: initialising '%s' mode mouse client\n",
             Mouse->name);

    ok = Mouse->init?Mouse->init():TRUE;
    if (ok) {
      m_printf("MOUSE: Mouse init ok, '%s' mode\n", Mouse->name);
      break;
    }
    else {
      m_printf("MOUSE: Mouse init ***failed***, '%s' mode\n",
               Mouse->name);
    }
    Mouse = Mouse->next;
  }
}

void dosemu_mouse_init(void)
{
  struct mouse_drv_wrp *m;
  for (m = mdrv; m; m = m->next) {
    if (m->drv->init(m->udata)) {
      cur_drv = m;
      break;
    }
  }

  mouse_client_init();
}

void mouse_move_buttons(int lbutton, int mbutton, int rbutton)
{
    if (cur_drv && cur_drv->drv->move_buttons)
	cur_drv->drv->move_buttons(lbutton, mbutton, rbutton, cur_drv->udata);
}

void mouse_move_relative(int dx, int dy, int x_range, int y_range)
{
    if (cur_drv && cur_drv->drv->move_relative)
	cur_drv->drv->move_relative(dx, dy, x_range, y_range, cur_drv->udata);
}

void mouse_move_mickeys(int dx, int dy)
{
    if (cur_drv && cur_drv->drv->move_mickeys)
	cur_drv->drv->move_mickeys(dx, dy, cur_drv->udata);
}

void mouse_move_absolute(int x, int y, int x_range, int y_range)
{
    if (cur_drv && cur_drv->drv->move_absolute)
	cur_drv->drv->move_absolute(x, y, x_range, y_range, cur_drv->udata);
}

void mouse_drag_to_corner(int x_range, int y_range)
{
    if (cur_drv && cur_drv->drv->drag_to_corner)
	cur_drv->drv->drag_to_corner(x_range, y_range, cur_drv->udata);
}

void mouse_sync_coords(int x, int y, int x_range, int y_range)
{
    if (cur_drv && cur_drv->drv->sync_coords)
	cur_drv->drv->sync_coords(x, y, x_range, y_range, cur_drv->udata);
}

void mouse_enable_native_cursor(int flag)
{
    if (cur_drv && cur_drv->drv->enable_native_cursor)
	cur_drv->drv->enable_native_cursor(flag, cur_drv->udata);
}

/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>
#include "emu.h"
#include "utilities.h"
#include "vgaemu.h"
#include "remap.h"
#include "vgatext.h"
#include "render.h"
#include "video.h"
#include "render_priv.h"

static struct remap_object *remap_obj;
struct rmcalls_wrp {
  struct remap_calls *calls;
  int prio;
};
#define MAX_REMAPS 5
static struct rmcalls_wrp rmcalls[MAX_REMAPS];
static int num_remaps;
static struct render_system *Render;
static int render_locked;
static int render_text;
static int is_updating;
static struct bitmap_desc dst_image;
static pthread_t render_thr;
static pthread_mutex_t render_mtx = PTHREAD_MUTEX_INITIALIZER;
static sem_t render_sem;
static void *render_thread(void *arg);
static int remap_mode(void);

static struct bitmap_desc render_lock(void)
{
  struct bitmap_desc img = Render->lock();
  render_locked++;
  return img;
}

static void render_unlock(void)
{
  render_locked--;
  Render->unlock();
}

static void check_locked(void)
{
  if (!render_locked)
    dosemu_error("render not locked!\n");
}

static void render_text_begin(void)
{
  render_text++;
}

static void render_text_end(void)
{
  render_text--;
  if (render_text) {
    render_unlock();
    render_text--;
  }
  assert(!render_text);
}

static void render_text_lock(void)
{
  if (!render_text) {
    dosemu_error("render not in text mode!\n");
    leavedos(95);
    return;
  }
  if (render_text == 1) {
    dst_image = render_lock();
    render_text++;
  }
}

static void render_text_unlock(void)
{
#if 1
  /* nothing: we coalesce multiple locks */
#else
  render_text--;
  render_unlock();
#endif
}

/*
 * Draw a text string for bitmap fonts.
 * The attribute is the VGA color/mono text attribute.
 */
static void bitmap_draw_string(int x, int y, unsigned char *text, int len, Bit8u attr)
{
  RectArea ra;
  ra = convert_bitmap_string(x, y, text, len, attr, dst_image);
  /* put_ximage uses display, mainwindow, gc, ximage       */
  X_printf("image at %d %d %d %d\n", ra.x, ra.y, ra.width, ra.height);
  if (ra.width)
    Render->refresh_rect(ra.x, ra.y, ra.width, ra.height);
}

static void bitmap_draw_line(int x, int y, int len)
{
  RectArea ra;
  ra = draw_bitmap_line(x, y, len, dst_image);
  if (ra.width)
    Render->refresh_rect(ra.x, ra.y, ra.width, ra.height);
}

static void bitmap_draw_text_cursor(int x, int y, Bit8u attr, int start, int end, Boolean focus)
{
  RectArea ra;
  ra = draw_bitmap_cursor(x, y, attr, start, end, focus, dst_image);
  if (ra.width)
    Render->refresh_rect(ra.x, ra.y, ra.width, ra.height);
}

static struct text_system Text_bitmap =
{
  bitmap_draw_string,
  bitmap_draw_line,
  bitmap_draw_text_cursor,
  NULL,
  render_text_lock,
  render_text_unlock,
};

int register_render_system(struct render_system *render_system)
{
  if (Render) {
    dosemu_error("multiple gfx renderers not supported, please report a bug!\n");
    return 0;
  }
  Render = render_system;
  return 1;
}

/*
 * Initialize the interface between the VGA emulator and X.
 * Check if X's color depth is supported.
 */
int remapper_init(int have_true_color, int have_shmap, int features,
    ColorSpaceDesc *csd)
{
  int remap_src_modes, err, ximage_mode;

//  set_remap_debug_msg(stderr);

  if(have_true_color) {
    switch(csd->bits) {
      case  1: ximage_mode = MODE_TRUE_1_MSB; break;
      case 15: ximage_mode = MODE_TRUE_15; break;
      case 16: ximage_mode = MODE_TRUE_16; break;
      case 24: ximage_mode = MODE_TRUE_24; break;
      case 32: ximage_mode = MODE_TRUE_32; break;
      default: ximage_mode = MODE_UNSUP;
    }
  }
  else {
    switch(csd->bits) {
      case  8: ximage_mode = have_shmap ? MODE_TRUE_8 : MODE_PSEUDO_8; break;
      default: ximage_mode = MODE_UNSUP;
    }
  }

  remap_src_modes = find_supported_modes(ximage_mode);
  remap_obj = remap_init(ximage_mode, features, csd);
  if (features & RFF_BITMAP_FONT) {
    use_bitmap_font = 1;
    register_text_system(&Text_bitmap);
    init_text_mapper(ximage_mode, features, csd);
  }
  err = sem_init(&render_sem, 0, 0);
  assert(!err);
  err = pthread_create(&render_thr, NULL, render_thread, NULL);
  assert(!err);

  return remap_src_modes;
}


/*
 * Free resources associated with remap_obj.
 */
void remapper_done(void)
{
  pthread_cancel(render_thr);
  pthread_join(render_thr, NULL);
  sem_destroy(&render_sem);
  done_text_mapper();
  if (remap_obj)
    remap_done(remap_obj);
}

/*
 * Update the displayed image to match the current DAC entries.
 * Will redraw the *entire* screen if at least one color has changed.
 */
static void refresh_truecolor(DAC_entry *col, int index, void *udata)
{
  struct remap_object *ro = udata;
  remap_palette_update(ro, index, vga.dac.bits, col->r, col->g, col->b);
}

/* returns True if the screen needs to be redrawn */
Boolean refresh_palette(void *udata)
{
  return changed_vga_colors(refresh_truecolor, udata);
}

/*
 * Update the active colormap to reflect the current DAC entries.
 */
static void refresh_graphics_palette(void)
{
  if (refresh_palette(remap_obj))
    dirty_all_video_pages();
}

void get_mode_parameters(int *wx_res, int *wy_res)
{
  int x_res, y_res, w_x_res, w_y_res;

  w_x_res = x_res = vga.width;
  w_y_res = y_res = vga.height;

  /* scale really small modes to ~320x240:
     TODO: look at vga registers */
  if(w_x_res <= 160 && w_x_res > 0)
    w_x_res = (320 / w_x_res) * w_x_res;
  if(w_y_res <= 120 && w_y_res > 0)
    w_y_res = (240 / w_y_res) * w_y_res;
  /* 320x200-style modes */
  if(w_x_res <= 320 && w_y_res <= 240) {
    w_x_res *= config.X_mode13fact;
    w_y_res *= config.X_mode13fact;
  } else if(w_y_res <= 240) {
    /* 640x200-style modes */
    w_y_res *= 2;
  } else if(w_x_res <= 320) {
    /* 320x480-style modes */
    w_x_res *= 2;
  }

  if(config.X_winsize_x > 0 && config.X_winsize_y > 0) {
    w_x_res = config.X_winsize_x;
    w_y_res = config.X_winsize_y;
  }

  if(config.X_aspect_43) {
    w_y_res = (w_x_res * 3) >> 2;
  }
#if 0
  cap = remap_get_cap(remap_obj);
  if(!(cap & (ROS_SCALE_ALL | ROS_SCALE_1 | ROS_SCALE_2))) {
    error("setmode: video mode 0x%02x not supported on this screen\n", vga.mode);
    /* why do we need a blank screen? */
    /* Because many games use 16bpp only for the trailer, so not quitting
     * here may actually help. */
#if 0
    leavedos(24);
#endif
  }

  if(!(cap & ROS_SCALE_ALL)) {
    if((cap & ROS_SCALE_2) && !(cap & ROS_SCALE_1)) {
      w_x_res = x_res << 1;
      w_y_res = y_res << 1;
    }
    else {
      w_x_res = x_res;
      w_y_res = y_res;
    }
  }
#endif

  *wx_res = w_x_res;
  *wy_res = w_y_res;
}

/*
 * Modify the current graphics mode.
 * Currently used to turn on/off chain4 addressing, change
 * the VGA screen size, change the DAC size.
 */
static void modify_mode(vga_emu_update_type *veut)
{
  if(vga.reconfig.mem) {
    dirty_all_video_pages();
    vga.reconfig.display = 0;
    vga.reconfig.dac = 0;
    vga.reconfig.mem = 0;
  }

  if(vga.reconfig.display) {
    v_printf(
      "modify_mode: geometry changed to %d x% d, scan_len = %d bytes\n",
      vga.width, vga.height, vga.scan_len
    );
    dirty_all_video_pages();
    vga.reconfig.display = 0;
  }

  if(vga.reconfig.dac) {
    vga.reconfig.dac = 0;
    v_printf("modify_mode: DAC bits = %d\n", vga.dac.bits);
  }

  veut->display_start = vga.display_start;
  veut->display_end = veut->display_start + vga.scan_len * vga.line_compare;
  if (vga.line_compare > vga.height)
    veut->display_end = veut->display_start + vga.scan_len * vga.height;

  if(vga.reconfig.mem || vga.reconfig.display) {
    v_printf("modify_mode: failed to modify current graphics mode\n");
  }
}


/*
 * DANG_BEGIN_FUNCTION update_screen
 *
 * description:
 * Update the part of the screen which has changed, in text mode
 * and in graphics mode. Usually called from the SIGALRM handler.
 *
 * X_update_screen returns 0 if nothing was updated, 1 if the whole
 * screen was updated, and 2 for a partial update.
 *
 * It is called in arch/linux/async/signal.c::SIGALRM_call() as part
 * of a struct video_system (see top of X.c) every 50 ms or
 * every 10 ms if 2 was returned, depending somewhat on various config
 * options as e.g. config.X_updatefreq and VIDEO_CHECK_DIRTY.
 * At least it is supposed to do that.
 *
 * DANG_END_FUNCTION
 *
 * Text and graphics updates are separate functions now; the code was
 * too messy. -- sw
 */

static int update_graphics_loop(int update_offset, vga_emu_update_type *veut)
{
  RectArea ra;
  int update_ret;
#ifdef DEBUG_SHOW_UPDATE_AREA
  static int dsua_fg_color = 0;
#endif

  while((update_ret = vga_emu_update(veut)) > 0) {
    ra = remap_remap_mem(remap_obj, BMP(veut->base,
                             vga.width, vga.height, vga.scan_len),
                             remap_mode(),
                             veut->display_start, update_offset,
                             veut->update_start - veut->display_start,
                             veut->update_len, dst_image, config.X_gamma);
#ifdef DEBUG_SHOW_UPDATE_AREA
    XSetForeground(display, gc, dsua_fg_color++);
    XFillRectangle(display, mainwindow, gc, ra.x, ra.y, ra.width, ra.height);
    XSync(display, False);
#endif
    Render->refresh_rect(ra.x, ra.y, ra.width, ra.height);

    v_printf("update_graphics_screen: display_start = 0x%04x, write_plane = %d, start %d, len %u, win (%d,%d),(%d,%d)\n",
      vga.display_start, vga.mem.write_plane,
      veut->update_start, veut->update_len, ra.x, ra.y, ra.width, ra.height
    );
  }
  return update_ret;
}

static int update_graphics_screen(void)
{
  vga_emu_update_type veut;
  int update_ret;
  unsigned wrap;

  veut.base = vga.mem.base;
  veut.max_max_len = 0;
  veut.max_len = 0;
  veut.display_start = 0;
  veut.display_end = vga.scan_len * vga.line_compare;
  if (vga.line_compare > vga.height)
    veut.display_end = vga.scan_len * vga.height;
  veut.update_gran = 0;
  veut.update_pos = veut.display_start;

  if(vga.reconfig.mem || vga.reconfig.display || vga.reconfig.dac)
    modify_mode(&veut);

  refresh_graphics_palette();

  if(vga.display_start != veut.display_start) {
    veut.display_start = vga.display_start;
    veut.display_end = veut.display_start + vga.scan_len * vga.line_compare;
    if (vga.line_compare > vga.height)
      veut.display_end = veut.display_start + vga.scan_len * vga.height;
    dirty_all_video_pages();
  }

  wrap = 0;
  if (veut.display_end > vga.mem.wrap) {
    wrap = veut.display_end - vga.mem.wrap;
    veut.display_end = vga.mem.wrap;
  }

  /*
   * You can now set the maximum update length, like this:
   *
   * veut->max_max_len = 20000;
   *
   * vga_emu_update() will return -1, if this limit is exceeded and there
   * are still invalid pages; `max_max_len' is only a rough estimate, the
   * actual upper limit will vary (might even be 25000 in the above
   * example).
   */

  veut.max_len = veut.max_max_len;

  update_ret = update_graphics_loop(0, &veut);

  if (wrap > 0) {
    /* This is for programs such as Commander Keen 4 that set the
       display_start close to the end of the video memory, and
       we need to wrap at 0xb0000
    */
    veut.display_end = wrap;
    wrap = veut.display_start;
    veut.display_start = 0;
    veut.max_len = veut.max_max_len;
    update_ret = update_graphics_loop(vga.mem.wrap - wrap, &veut);
    veut.display_start = wrap;
    veut.display_end += vga.mem.wrap;
  }

  if (vga.line_compare < vga.height) {

    veut.display_start = 0;
    veut.display_end = vga.scan_len * (vga.height - vga.line_compare);
    veut.max_len = veut.max_max_len;

    update_ret = update_graphics_loop(vga.scan_len * vga.line_compare, &veut);

    veut.display_start = vga.display_start;
    veut.display_end = veut.display_start + vga.scan_len * vga.line_compare;
  }

  return update_ret < 0 ? 2 : 1;
}

int render_is_updating(void)
{
  return is_updating;
}

static void *render_thread(void *arg)
{
  while (1) {
    is_updating = 0;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    /* small delay til we have a controlled framerate */
    usleep(5000);
    sem_wait(&render_sem);
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    is_updating = 1;
    switch (vga.mode_class) {
    case TEXT:
      blink_cursor();
      if (text_is_dirty()) {
        render_text_begin();
        update_cursor();
        update_text_screen();
        render_text_end();
      }
      break;
    case GRAPH:
      if (vgaemu_is_dirty()) {
        dst_image = render_lock();
        update_graphics_screen();
        render_unlock();
      }
      break;
    default:
      v_printf("VGA not yet initialized\n");
      break;
    }
  }
  return NULL;
}

int update_screen(void)
{
  if(vga.config.video_off) {
    v_printf("update_screen: nothing done (video_off = 0x%x)\n", vga.config.video_off);
    return 1;
  }
  sem_post(&render_sem);
  return 1;
}

void redraw_text_screen(void)
{
  dirty_text_screen();
}

void render_gain_focus(void)
{
  if (vga.mode_class == TEXT)
    text_gain_focus();
}

void render_lose_focus(void)
{
  if (vga.mode_class == TEXT)
    text_lose_focus();
}

static int remap_mode(void)
{
  int mode_type;
  switch(vga.mode_type) {
  case CGA:
    mode_type = vga.pixel_size == 2 ? MODE_CGA_2 : MODE_CGA_1; break;
  case HERC:
    mode_type = MODE_HERC; break;
  case PL1:
    mode_type = MODE_VGA_1; break;
  case PL2:
    mode_type = MODE_VGA_2; break;
  case PL4:
    mode_type = MODE_VGA_4; break;
  case P8:
    mode_type = vga.seq.addr_mode == 2 ? MODE_PSEUDO_8 : MODE_VGA_X; break;
  case P15:
    mode_type = MODE_TRUE_15; break;
  case P16:
    mode_type = MODE_TRUE_16; break;
  case P24:
    mode_type = MODE_TRUE_24; break;
  case P32:
    mode_type = MODE_TRUE_32; break;
  default:
    mode_type = 0;
  }
  return mode_type;
}

void render_blit(int x, int y, int width, int height)
{
  struct bitmap_desc img = render_lock();
  if (vga.mode_class == TEXT)
    text_blit(x, y, width, height, img);
  else
    remap_remap_rect_dst(remap_obj, BMP(vga.mem.base + vga.display_start,
	vga.width, vga.height, vga.scan_len), remap_mode(),
	x, y, width, height, img, config.X_gamma);
  Render->refresh_rect(x, y, width, height);
  render_unlock();
}

int register_remapper(struct remap_calls *calls, int prio)
{
  struct rmcalls_wrp *rm;
  assert(num_remaps < MAX_REMAPS);
  rm = &rmcalls[num_remaps++];
  rm->calls = calls;
  rm->prio = prio;
  return 0;
}

static int find_rmcalls(int sidx)
{
  int i, idx = -1, max1 = -1, max = -1;
  if (sidx >= 0)
    max = rmcalls[sidx].prio;
  for (i = 0; i < num_remaps; i++) {
    int p = rmcalls[i].prio;
    if (p > max1 && (p < max || max == -1)) {
      max1 = p;
      idx = i;
    }
  }
  return idx;
}

struct remap_object *remap_init(int dst_mode, int features,
        const ColorSpaceDesc *color_space)
{
  void *rm;
  struct remap_object *ro;
  struct remap_calls *calls;
  int i = -1;
  do {
    i = find_rmcalls(i);
    if (i == -1)
      break;
    calls = rmcalls[i].calls;
    rm = calls->init(dst_mode, features, color_space);
    if (!rm)
      v_printf("remapper %i \"%s\" failed for mode %x\n",
          i, rmcalls[i].calls->name, dst_mode);
  } while (!rm);
  if (!rm) {
    error("gfx remapper failure\n");
    leavedos(3);
    return NULL;
  }
  ro = malloc(sizeof(*ro));
  ro->calls = calls;
  ro->priv = rm;
  return ro;
}

void remap_done(struct remap_object *ro)
{
  ro->calls->done(ro->priv);
  free(ro);
}

#define REMAP_CALL0(r, x) \
r remap_##x(struct remap_object *ro) \
{ \
  r ret; \
  CHECK_##x(); \
  pthread_mutex_lock(&render_mtx); \
  ret = ro->calls->x(ro->priv); \
  pthread_mutex_unlock(&render_mtx); \
  return ret; \
}
#define REMAP_CALL1(r, x, t, a) \
r remap_##x(struct remap_object *ro, t a) \
{ \
  r ret; \
  CHECK_##x(); \
  pthread_mutex_lock(&render_mtx); \
  ret = ro->calls->x(ro->priv, a); \
  pthread_mutex_unlock(&render_mtx); \
  return ret; \
}
#define REMAP_CALL3(r, x, t1, a1, t2, a2, t3, a3) \
r remap_##x(struct remap_object *ro, t1 a1, t2 a2, t3 a3) \
{ \
  r ret; \
  CHECK_##x(); \
  pthread_mutex_lock(&render_mtx); \
  ret = ro->calls->x(ro->priv, a1, a2, a3); \
  pthread_mutex_unlock(&render_mtx); \
  return ret; \
}
#define REMAP_CALL5(r, x, t1, a1, t2, a2, t3, a3, t4, a4, t5, a5) \
r remap_##x(struct remap_object *ro, t1 a1, t2 a2, t3 a3, t4 a4, t5 a5) \
{ \
  r ret; \
  CHECK_##x(); \
  pthread_mutex_lock(&render_mtx); \
  ret = ro->calls->x(ro->priv, a1, a2, a3, a4, a5); \
  pthread_mutex_unlock(&render_mtx); \
  return ret; \
}
#define REMAP_CALL8(r, x, t1, a1, t2, a2, t3, a3, t4, a4, t5, a5, t6, a6, t7, a7, t8, a8) \
r remap_##x(struct remap_object *ro, t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8) \
{ \
  r ret; \
  CHECK_##x(); \
  pthread_mutex_lock(&render_mtx); \
  ret = ro->calls->x(ro->priv, a1, a2, a3, a4, a5, a6, a7, a8); \
  pthread_mutex_unlock(&render_mtx); \
  return ret; \
}

#define CHECK_get_cap()
#define CHECK_remap_mem() check_locked()
#define CHECK_remap_rect() check_locked()
#define CHECK_remap_rect_dst() check_locked()
#define CHECK_palette_update()
#define CHECK_adjust_gamma()

REMAP_CALL5(int, palette_update, unsigned, i,
	unsigned, bits, unsigned, r, unsigned, g, unsigned, b)
REMAP_CALL8(RectArea, remap_rect, const struct bitmap_desc, src_img,
	int, src_mode,
	int, x0, int, y0, int, width, int, height, struct bitmap_desc, dst_img,
	int, gamma)
REMAP_CALL8(RectArea, remap_rect_dst, const struct bitmap_desc, src_img,
	int, src_mode,
	int, x0, int, y0, int, width, int, height, struct bitmap_desc, dst_img,
	int, gamma)
REMAP_CALL8(RectArea, remap_mem, const struct bitmap_desc, src_img,
	int, src_mode,
	unsigned, src_start,
	unsigned, dst_start, int, offset, int, len,
	struct bitmap_desc, dst_img,
	int, gamma)
REMAP_CALL0(int, get_cap)

void color_space_complete(ColorSpaceDesc *csd)
{
  unsigned ui;

  if((ui = csd->r_mask)) for(csd->r_shift = 0; !(ui & 1); ui >>= 1, csd->r_shift++);
  if(ui) for(; ui; ui >>= 1, csd->r_bits++);
  if((ui = csd->g_mask)) for(csd->g_shift = 0; !(ui & 1); ui >>= 1, csd->g_shift++);
  if(ui) for(; ui; ui >>= 1, csd->g_bits++);
  if((ui = csd->b_mask)) for(csd->b_shift = 0; !(ui & 1); ui >>= 1, csd->b_shift++);
  if(ui) for(; ui; ui >>= 1, csd->b_bits++);
}

int find_supported_modes(unsigned dst_mode)
{
  return ~0;
}

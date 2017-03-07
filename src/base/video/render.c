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
#include "vgatext.h"
#include "render.h"
#include "video.h"
#include "render_priv.h"

#define RENDER_THREADED 1

struct rmcalls_wrp {
  struct remap_calls *calls;
  int prio;
};
#define MAX_REMAPS 5
static struct rmcalls_wrp rmcalls[MAX_REMAPS];
static int num_remaps;
static int is_updating;
static pthread_mutex_t upd_mtx = PTHREAD_MUTEX_INITIALIZER;
#if RENDER_THREADED
static pthread_t render_thr;
#endif
static pthread_mutex_t render_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mode_mtx = PTHREAD_MUTEX_INITIALIZER;
static sem_t render_sem;
static void do_rend(void);
static int remap_mode(void);

#define MAX_RENDERS 5
struct render_wrp {
    struct render_system *render[MAX_RENDERS];
    int num_renders;
    int render_locked;
    int render_text;
    int text_locked;
    struct remap_object *gfx_remap;
    struct remap_object *text_remap;
    struct bitmap_desc dst_image[MAX_RENDERS];
};
static struct render_wrp Render;

__attribute__((warn_unused_result))
static int render_lock(void)
{
  int i, j;
  for (i = 0; i < Render.num_renders; i++) {
    Render.dst_image[i] = Render.render[i]->lock();
    if (!Render.dst_image[i].width) {
      error("render %s failed to lock\n", Render.render[i]->name);
      /* undo locks in case of a failure */
      for (j = 0; j < i; j++)
        Render.render[j]->unlock();
      return -1;
    }
  }
  Render.render_locked++;
  return 0;
}

static void render_unlock(void)
{
  int i;
  for (i = 0; i < Render.num_renders; i++)
    Render.render[i]->unlock();
  Render.render_locked--;
}

static void check_locked(void)
{
  if (!Render.render_locked)
    dosemu_error("render not locked!\n");
}

static int render_text_begin(void)
{
  int err = text_lock();
  if (err)
    return err;
  Render.render_text++;
  return 0;
}

static void render_text_end(void)
{
  text_unlock();
  Render.render_text--;
  assert(!Render.text_locked);
}

static int render_text_lock(void *opaque)
{
  int err;
  if (Render.render_text || Render.text_locked) {
    dosemu_error("render not in text mode!\n");
    leavedos(95);
    return -1;
  }
  err = render_lock();
  if (err)
    return err;
  Render.text_locked++;
  return 0;
}

static void render_text_unlock(void *opaque)
{
  Render.text_locked--;
  render_unlock();
}

static void render_rect_add(int rend_idx, RectArea rect)
{
  Render.render[rend_idx]->refresh_rect(rect.x, rect.y, rect.width, rect.height);
}

/*
 * Draw a text string for bitmap fonts.
 * The attribute is the VGA color/mono text attribute.
 */
static void bitmap_draw_string(void *opaque, int x, int y,
    unsigned char *text, int len, Bit8u attr)
{
  struct remap_object **obj = opaque;
  struct bitmap_desc src_image;
  src_image = convert_bitmap_string(x, y, text, len, attr);
  if (!src_image.img)
    return;
  remap_remap_rect(*obj, src_image, MODE_PSEUDO_8,
			      vga.char_width * x, vga.char_height * y,
			      vga.char_width * len, vga.char_height);
}

static void bitmap_draw_line(void *opaque, int x, int y, int len)
{
  struct remap_object **obj = opaque;
  struct bitmap_desc src_image;
  src_image = draw_bitmap_line(x, y, len);
  remap_remap_rect(*obj, src_image, MODE_PSEUDO_8,
    x, y, len, 1);
}

static void bitmap_draw_text_cursor(void *opaque, int x, int y,
    Bit8u attr, int start, int end, Boolean focus)
{
  struct remap_object **obj = opaque;
  struct bitmap_desc src_image;
  src_image = draw_bitmap_cursor(x, y, attr, start, end, focus);
  remap_remap_rect(*obj, src_image, MODE_PSEUDO_8,
			      vga.char_width * x, vga.char_height * y,
			      vga.char_width, vga.char_height);
}

static struct text_system Text_bitmap =
{
  bitmap_draw_string,
  bitmap_draw_line,
  bitmap_draw_text_cursor,
  NULL,
  render_text_lock,
  render_text_unlock,
  &Render.text_remap,
};

int register_render_system(struct render_system *render_system)
{
  assert(Render.num_renders < MAX_RENDERS);
  Render.render[Render.num_renders++] = render_system;
  return 1;
}

/*
 * Initialize the interface between the VGA emulator and X.
 * Check if X's color depth is supported.
 */
int remapper_init(int have_true_color, int have_shmap, int features,
    ColorSpaceDesc *csd)
{
  int remap_src_modes, ximage_mode;

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
  Render.gfx_remap = remap_init(ximage_mode, features, csd);
  if (features & RFF_BITMAP_FONT) {
    use_bitmap_font = 1;
    /* linear 1 byte per pixel */
    Render.text_remap = remap_init(ximage_mode, features, csd);
    register_text_system(&Text_bitmap);
    init_text_mapper(ximage_mode, features, csd);
  }

  return vga_emu_init(remap_src_modes, csd);
}

#if RENDER_THREADED
static void *render_thread(void *arg)
{
  while (1) {
    sem_wait(&render_sem);
    pthread_mutex_lock(&upd_mtx);
    is_updating = 1;
    pthread_mutex_unlock(&upd_mtx);
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    do_rend();
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_mutex_lock(&upd_mtx);
    is_updating = 0;
    pthread_mutex_unlock(&upd_mtx);
  }
  return NULL;
}
#endif

int render_init(void)
{
  int err = 0;
#if RENDER_THREADED
  err = sem_init(&render_sem, 0, 0);
  assert(!err);
  err = pthread_create(&render_thr, NULL, render_thread, NULL);
  pthread_setname_np(render_thr, "dosemu: render");
  assert(!err);
#endif
  return err;
}

/*
 * Free resources associated with remap_obj.
 */
void remapper_done(void)
{
#if RENDER_THREADED
  pthread_cancel(render_thr);
  pthread_join(render_thr, NULL);
  sem_destroy(&render_sem);
#endif
  done_text_mapper();
  if (Render.text_remap)
    remap_done(Render.text_remap);
  if (Render.gfx_remap)
    remap_done(Render.gfx_remap);
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
Boolean refresh_palette(void *opaque)
{
  struct remap_object **obj = opaque;
  return changed_vga_colors(refresh_truecolor, *obj);
}

/*
 * Update the active colormap to reflect the current DAC entries.
 */
static void refresh_graphics_palette(void)
{
  if (refresh_palette(&Render.gfx_remap))
    dirty_all_video_pages();
}

struct vid_mode_params get_mode_parameters(void)
{
  struct vid_mode_params ret;
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
  if(w_x_res <= 360 && w_y_res <= 240) {
    w_x_res *= config.X_mode13fact;
    w_y_res *= config.X_mode13fact;
  } else if(w_y_res <= 240) {
    /* 640x200-style modes */
    w_y_res *= 2;
  } else if(w_x_res <= 360) {
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
  cap = remap_get_cap(Render.gfx_remap);
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

  ret.w_x_res = w_x_res;
  ret.w_y_res = w_y_res;
  ret.x_res = x_res;
  ret.y_res = y_res;
  ret.mode_class = vga.mode_class;
  ret.text_width = vga.text_width;
  ret.text_height = vga.text_height;
  return ret;
}

/*
 * Modify the current graphics mode.
 * Currently used to turn on/off chain4 addressing, change
 * the VGA screen size, change the DAC size.
 */
static void modify_mode(void)
{
  if(vga.reconfig.mem) {
    dirty_all_video_pages();
    vga.reconfig.mem = 0;
  }

  if(vga.reconfig.dac) {
    dirty_all_vga_colors();
    vga.reconfig.dac = 0;
    v_printf("modify_mode: DAC bits = %d\n", vga.dac.bits);
  }
}


static void update_graphics_loop(unsigned display_start,
	unsigned display_end, int src_offset,
	int update_offset, vga_emu_update_type *veut)
{
  int i = -1;

  while ((i = vga_emu_update(veut, display_start, display_end, i)) != -1) {
    remap_remap_mem(Render.gfx_remap, BMP(vga.mem.base + display_start,
                             vga.width, vga.height, vga.scan_len),
                             remap_mode(),
                             src_offset, update_offset +
                             veut->update_start - display_start,
                             veut->update_len);
  }
}

static void update_graphics_screen(void)
{
  vga_emu_update_type veut;
  unsigned display_end, wrap;

  refresh_graphics_palette();

  display_end = vga.display_start + vga.scan_len * vga.height;
  if (vga.line_compare < vga.height) {
    unsigned wrap2 = vga.display_start + vga.scan_len * vga.line_compare;
    wrap = min(vga.mem.wrap, wrap2);
  } else {
    wrap = vga.mem.wrap;
  }

  update_graphics_loop(vga.display_start, wrap, 0, 0, &veut);

  if (display_end > wrap) {
    int len = wrap - vga.display_start;
    /* This is for programs such as Commander Keen 4 that set the
       display_start close to the end of the video memory, and
       we need to wrap at 0xb0000
    */
    update_graphics_loop(0, display_end - wrap, -len, len, &veut);
  }
}

int render_is_updating(void)
{
  int upd;
  pthread_mutex_lock(&upd_mtx);
  upd = is_updating;
  pthread_mutex_unlock(&upd_mtx);
  return upd;
}

static void do_rend(void)
{
  pthread_mutex_lock(&mode_mtx);
  vga_emu_update_lock();
  if(vga.reconfig.mem || vga.reconfig.dac)
    modify_mode();
  switch (vga.mode_class) {
    case TEXT:
      blink_cursor();
      if (text_is_dirty()) {
        int err = render_text_begin();
        if (err)
          break;
        update_text_screen();
        render_text_end();
      }
      break;
    case GRAPH:
      if (vgaemu_is_dirty()) {
        int err = render_lock();
        if (err)
          break;
        update_graphics_screen();
        render_unlock();
      }
      break;
    default:
      v_printf("VGA not yet initialized\n");
      break;
  }
  vga_emu_update_unlock();
  pthread_mutex_unlock(&mode_mtx);
}

void render_mode_lock(void)
{
  pthread_mutex_lock(&mode_mtx);
}

void render_mode_unlock(void)
{
  pthread_mutex_unlock(&mode_mtx);
}

int render_update_vidmode(void)
{
  int ret = 0;
  if (Video->setmode) {
    pthread_mutex_lock(&mode_mtx);
    ret = Video->setmode(get_mode_parameters());
    pthread_mutex_unlock(&mode_mtx);
  }
  return ret;
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
int update_screen(void)
{
  int upd = render_is_updating();

#if !RENDER_THREADED
  do_rend();
#endif
  if (!upd) {
    if (Video->update_screen)
      Video->update_screen();
  }

  if (Video->handle_events)
    Video->handle_events();

  if(vga.config.video_off) {
    v_printf("update_screen: nothing done (video_off = 0x%x)\n", vga.config.video_off);
    return 1;
  }
  if (upd)
    return 1;
  /* unfortunately SDL is not thread-safe, so display mode updates
   * need to be done from main thread. */
  if(vga.reconfig.display) {
    v_printf(
      "modify_mode: geometry changed to %d x% d, scan_len = %d bytes\n",
      vga.width, vga.height, vga.scan_len
    );
    vga_emu_update_lock();
    render_update_vidmode();
    dirty_all_video_pages();
    vga.reconfig.display = 0;
    vga_emu_update_unlock();
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
  int err = render_lock();
  if (err)
    return;
  if (vga.mode_class == TEXT) {
    struct bitmap_desc src_image;
    if (!use_bitmap_font)
      goto unlock;
    src_image = get_text_canvas();
    remap_remap_rect_dst(Render.text_remap, src_image, MODE_PSEUDO_8,
	x, y, width, height);
  } else {
    /* unfortunately this does not handle mem wrap, so keen4 will
     * have artifacts. Don't use this blit too much... SDL plugin
     * doesn't use it but an X plugin does. Wrap should really be
     * handled by remapper. */
    remap_remap_rect_dst(Render.gfx_remap, BMP(vga.mem.base + vga.display_start,
	vga.width, vga.height, vga.scan_len), remap_mode(),
	x, y, width, height);
  }
unlock:
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
    rm = calls->init(dst_mode, features, color_space, config.X_gamma);
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
#define REMAP_CALL5_WR(_x, t1, a1, t2, a2, t3, a3, t4, a4, t5, a5) \
void remap_##_x(struct remap_object *ro, t1 a1, t2 a2, t3 a3, t4 a4, t5 a5) \
{ \
  RectArea r; \
  int i; \
  CHECK_##_x(); \
  pthread_mutex_lock(&render_mtx); \
  for (i = 0; i < Render.num_renders; i++) { \
    r = ro->calls->_x(ro->priv, a1, a2, a3, a4, a5, Render.dst_image[i]); \
    if (r.width) \
      render_rect_add(i, r); \
  } \
  pthread_mutex_unlock(&render_mtx); \
}
#define REMAP_CALL6_WR(_x, t1, a1, t2, a2, t3, a3, t4, a4, t5, a5, t6, a6) \
void remap_##_x(struct remap_object *ro, t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
{ \
  RectArea r; \
  int i; \
  CHECK_##_x(); \
  pthread_mutex_lock(&render_mtx); \
  for (i = 0; i < Render.num_renders; i++) { \
    r = ro->calls->_x(ro->priv, a1, a2, a3, a4, a5, a6, Render.dst_image[i]); \
    if (r.width) \
      render_rect_add(i, r); \
  } \
  pthread_mutex_unlock(&render_mtx); \
}

#define CHECK_get_cap()
#define CHECK_remap_mem() check_locked()
#define CHECK_remap_rect() check_locked()
#define CHECK_remap_rect_dst() check_locked()
#define CHECK_palette_update()
#define CHECK_adjust_gamma()

REMAP_CALL5(int, palette_update, unsigned, i,
	unsigned, bits, unsigned, r, unsigned, g, unsigned, b)
REMAP_CALL6_WR(remap_rect, const struct bitmap_desc, src_img,
	int, src_mode,
	int, x0, int, y0, int, width, int, height
)
REMAP_CALL6_WR(remap_rect_dst, const struct bitmap_desc, src_img,
	int, src_mode,
	int, x0, int, y0, int, width, int, height
)
REMAP_CALL5_WR(remap_mem, const struct bitmap_desc, src_img,
	int, src_mode,
	unsigned, src_start,
	int, offset, int, len
)
REMAP_CALL0(int, get_cap)

void color_space_complete(ColorSpaceDesc *csd)
{
  unsigned ui;

  if((ui = csd->r_mask)) for(csd->r_shift = 0; !(ui & 1); ui >>= 1, csd->r_shift++);
  if(ui) for(csd->r_bits = 0; ui; ui >>= 1, csd->r_bits++);
  if((ui = csd->g_mask)) for(csd->g_shift = 0; !(ui & 1); ui >>= 1, csd->g_shift++);
  if(ui) for(csd->g_bits = 0; ui; ui >>= 1, csd->g_bits++);
  if((ui = csd->b_mask)) for(csd->b_shift = 0; !(ui & 1); ui >>= 1, csd->b_shift++);
  if(ui) for(csd->b_bits = 0; ui; ui >>= 1, csd->b_bits++);
}

int find_supported_modes(unsigned dst_mode)
{
  return ~0;
}

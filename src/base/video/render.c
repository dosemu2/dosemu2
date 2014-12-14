/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include "config.h"

#include <stdio.h>
#include "emu.h"
#include "vgaemu.h"
#include "remap.h"
#include "vgatext.h"
#include "render.h"
#include "video.h"
#include "render_priv.h"

static struct remap_object *remap_obj;
static struct remap_calls *rmcalls;
int remap_features;
static struct render_system *Render;
static const ColorSpaceDesc *color_space;
static unsigned char *dst_image;
static int dst_mode, dst_width, dst_height, dst_scan_len;
static unsigned ximage_mode;

/*
 * Draw a text string for bitmap fonts.
 * The attribute is the VGA color/mono text attribute.
 */
static void bitmap_draw_string(int x, int y, unsigned char *text, int len, Bit8u attr)
{
  RectArea ra = convert_bitmap_string(x, y, text, len, attr);
  /* put_ximage uses display, mainwindow, gc, ximage       */
  X_printf("image at %d %d %d %d\n", ra.x, ra.y, ra.width, ra.height);
  if (ra.width)
    Render->refresh_rect(ra.x, ra.y, ra.width, ra.height);
}

static void bitmap_draw_line(int x, int y, int len)
{
  RectArea ra;

  ra = draw_bitmap_line(x, y, len);
  if (ra.width)
    Render->refresh_rect(ra.x, ra.y, ra.width, ra.height);
}

static void bitmap_draw_text_cursor(int x, int y, Bit8u attr, int start, int end, Boolean focus)
{
  RectArea ra;

  ra = draw_bitmap_cursor(x, y, attr, start, end, focus);
  if (ra.width)
    Render->refresh_rect(ra.x, ra.y, ra.width, ra.height);
}

static struct text_system Text_bitmap =
{
  bitmap_draw_string,
  bitmap_draw_line,
  bitmap_draw_text_cursor,
};

int register_render_system(struct render_system *render_system)
{
  Render = render_system;
  register_text_system(&Text_bitmap);
  return 1;
}

/*
 * Initialize the interface between the VGA emulator and X.
 * Check if X's color depth is supported.
 */
int remapper_init(unsigned *image_mode,
		  int have_true_color, int have_shmap, ColorSpaceDesc *csd)
{
  int remap_src_modes;

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

  remap_features = 0;
  if(config.X_lin_filt) remap_features |= RFF_LIN_FILT;
  if(config.X_bilin_filt) remap_features |= RFF_BILIN_FILT;

  remap_src_modes = find_supported_modes(ximage_mode);
  *image_mode = ximage_mode;
  color_space = csd;
  dst_mode = ximage_mode;
  init_text_mapper(ximage_mode, csd);
  return remap_src_modes;
}


/*
 * Free resources associated with remap_obj.
 */
void remapper_done(void)
{
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

void get_mode_parameters(int *wx_res, int *wy_res,
			 vga_emu_update_type *veut)
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
  veut->base = vga.mem.base;
  veut->max_max_len = 0;
  veut->max_len = 0;
  veut->display_start = 0;
  veut->display_end = vga.scan_len * vga.line_compare;
  if (vga.line_compare > vga.height)
    veut->display_end = vga.scan_len * vga.height;
  veut->update_gran = 0;
  veut->update_pos = veut->display_start;

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
  struct remap_object *tmp_ro;
  int cap;

  if(vga.reconfig.mem) {
    if(vga.mode_type == P8 || vga.mode_type == PL4) {
      if (vga.mode_type == PL4)
	tmp_ro = remap_init(MODE_VGA_4, dst_mode, remap_features,
		color_space);
      else
	tmp_ro = remap_init(vga.seq.addr_mode == 2 ? MODE_PSEUDO_8 :
		MODE_VGA_X, dst_mode, remap_features,
		color_space);
      cap = remap_get_cap(tmp_ro);
      remap_src_resize(tmp_ro, vga.width, vga.height, vga.scan_len);
      remap_dst_resize(tmp_ro, dst_width, dst_height, dst_scan_len);

      if(!(cap & (ROS_SCALE_ALL | ROS_SCALE_1 | ROS_SCALE_2))) {
        v_printf("modify_mode: no memory config change of current graphics mode supported\n");
        remap_done(tmp_ro);
      }
      else {
        v_printf("modify_mode: chain4 addressing turned %s\n", vga.mem.planes == 1 ? "on" : "off");
        remap_done(remap_obj);
        remap_obj = tmp_ro;
      }

      dirty_all_video_pages();
      /*
       * The new remap object does not yet know about our colors.
       * So we have to force an update. -- sw
       */
      dirty_all_vga_colors();

      vga.reconfig.display =
      vga.reconfig.dac = 0;
    }
    vga.reconfig.mem = 0;
  }

  if(vga.reconfig.display) {
    remap_src_resize(remap_obj, vga.width, vga.height, vga.scan_len);
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
    ra = remap_remap_mem(remap_obj, veut->base, veut->display_start,
                             update_offset,
                             veut->update_start - veut->display_start,
                             veut->update_len, dst_image);

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

static int update_graphics_screen(vga_emu_update_type *veut)
{
  int update_ret;
  unsigned wrap;

  if(vga.reconfig.mem || vga.reconfig.display || vga.reconfig.dac)
    modify_mode(veut);

  refresh_graphics_palette();

  if(vga.display_start != veut->display_start) {
    veut->display_start = vga.display_start;
    veut->display_end = veut->display_start + vga.scan_len * vga.line_compare;
    if (vga.line_compare > vga.height)
      veut->display_end = veut->display_start + vga.scan_len * vga.height;
    dirty_all_video_pages();
  }

  wrap = 0;
  if (veut->display_end > vga.mem.wrap) {
    wrap = veut->display_end - vga.mem.wrap;
    veut->display_end = vga.mem.wrap;
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

  veut->max_len = veut->max_max_len;

  update_ret = update_graphics_loop(0, veut);

  if (wrap > 0) {
    /* This is for programs such as Commander Keen 4 that set the
       display_start close to the end of the video memory, and
       we need to wrap at 0xb0000
    */
    veut->display_end = wrap;
    wrap = veut->display_start;
    veut->display_start = 0;
    veut->max_len = veut->max_max_len;
    update_ret = update_graphics_loop(vga.mem.wrap - wrap, veut);
    veut->display_start = wrap;
    veut->display_end += vga.mem.wrap;
  }

  if (vga.line_compare < vga.height) {

    veut->display_start = 0;
    veut->display_end = vga.scan_len * (vga.height - vga.line_compare);
    veut->max_len = veut->max_max_len;

    update_ret = update_graphics_loop(vga.scan_len * vga.line_compare, veut);

    veut->display_start = vga.display_start;
    veut->display_end = veut->display_start + vga.scan_len * vga.line_compare;
  }

  return update_ret < 0 ? 2 : 1;
}

int update_screen(vga_emu_update_type *veut)
{
  if(vga.config.video_off) {
    v_printf("update_screen: nothing done (video_off = 0x%x)\n", vga.config.video_off);
    return 1;
  }

  return vga.mode_class == TEXT ? update_text_screen() :
    update_graphics_screen(veut);
}

void render_resize(uint8_t *img, int width, int height, int scan_len)
{
  if (vga.mode_class == TEXT) {
    resize_text_mapper(img, width, height, scan_len);
    return;
  }
  remap_dst_resize(remap_obj, width, height, scan_len);
  dst_image = img;
  dst_width = width;
  dst_height = height;
  dst_scan_len = scan_len;
}

void render_init(uint8_t *img, int width, int height, int scan_len)
{
  int mode_type;

  if (remap_obj)
    remap_done(remap_obj);
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
    mode_type = MODE_PSEUDO_8; break;
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

  v_printf("setmode: remap_init(0x%04x, 0x%04x, 0x%04x)\n",
	   mode_type, ximage_mode, remap_features);

  remap_obj = remap_init(mode_type, dst_mode, remap_features,
	color_space);
  remap_adjust_gamma(remap_obj, config.X_gamma);

  remap_src_resize(remap_obj, vga.width, vga.height, vga.scan_len);
  render_resize(img, width, height, scan_len);
  /*
   * The new remap object does not yet know about our colors.
   * So we have to force an update. -- sw
   */
  dirty_all_vga_colors();
}

void render_blit(vga_emu_update_type *veut, int x, int y, int width,
	int height)
{
  if (vga.mode_class == TEXT)
    text_blit(x, y, width, height);
  else
    remap_remap_rect_dst(remap_obj, veut->base + veut->display_start,
	x, y, width, height, dst_image);
  Render->refresh_rect(x, y, width, height);
}

int register_remapper(struct remap_calls *calls)
{
  if (rmcalls)
    return -1;
  rmcalls = calls;
  return 0;
}

struct remap_object *remap_init(int src_mode, int dst_mode, int features,
        const ColorSpaceDesc *color_space)
{
  void *rm;
  struct remap_object *ro;
  rm = rmcalls->init(src_mode, dst_mode, features, color_space);
  if (!rm) {
    error("gfx remapper failure\n");
    leavedos(3);
    return NULL;
  }
  ro = malloc(sizeof(*ro));
  ro->calls = rmcalls;
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
  return ro->calls->x(ro->priv); \
}
#define REMAP_CALL1(r, x, t, a) \
r remap_##x(struct remap_object *ro, t a) \
{ \
  return ro->calls->x(ro->priv, a); \
}
#define REMAP_CALL3(r, x, t1, a1, t2, a2, t3, a3) \
r remap_##x(struct remap_object *ro, t1 a1, t2 a2, t3 a3) \
{ \
  return ro->calls->x(ro->priv, a1, a2, a3); \
}
#define REMAP_CALL5(r, x, t1, a1, t2, a2, t3, a3, t4, a4, t5, a5) \
r remap_##x(struct remap_object *ro, t1 a1, t2 a2, t3 a3, t4 a4, t5 a5) \
{ \
  return ro->calls->x(ro->priv, a1, a2, a3, a4, a5); \
}
#define REMAP_CALL6(r, x, t1, a1, t2, a2, t3, a3, t4, a4, t5, a5, t6, a6) \
r remap_##x(struct remap_object *ro, t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
{ \
  return ro->calls->x(ro->priv, a1, a2, a3, a4, a5, a6); \
}

REMAP_CALL1(void, adjust_gamma, unsigned, gamma)
REMAP_CALL5(int, palette_update, unsigned, i,
	unsigned, bits, unsigned, r, unsigned, g, unsigned, b)
REMAP_CALL3(void, src_resize, int, width, int, height, int, scan_len)
REMAP_CALL3(void, dst_resize, int, width, int, height, int, scan_len)
REMAP_CALL6(RectArea, remap_rect, const unsigned char *, src_img,
	int, x0, int, y0, int, width, int, height, unsigned char *, dst_img)
REMAP_CALL6(RectArea, remap_rect_dst, const unsigned char *, src_img,
	int, x0, int, y0, int, width, int, height, unsigned char *, dst_img)
REMAP_CALL6(RectArea, remap_mem, const unsigned char *, src_img,
	unsigned, src_start, unsigned, dst_start, int, offset, int, len,
	unsigned char *, dst_img)
REMAP_CALL0(int, get_cap)

void color_space_complete(ColorSpaceDesc *color_space)
{
  rmcalls->color_space_complete(color_space);
}

int find_supported_modes(unsigned dst_mode)
{
  return rmcalls->find_supported_modes(dst_mode);
}

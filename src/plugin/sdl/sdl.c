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
 * Purpose: SDL video renderer
 *
 * Author: Stas Sergeev
 * Loosely based on SDL1 plugin by Emmanuel Jeandel and Bart Oldeman
 */

#define SDL_C
#include "config.h"

#include <stdio.h>
#include <stdlib.h>		/* for malloc & free */
#include <string.h>		/* for memset */
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>
#include <SDL.h>

#include "emu.h"
#include "timers.h"
#include "init.h"
#include "sig.h"
#include "bios.h"
#include "video.h"
#include "memory.h"
#ifdef X_SUPPORT
#include <SDL_syswm.h>
#endif
#include "vgaemu.h"
#include "vgatext.h"
#include "render.h"
#include "sdl.h"
#include "keyb_clients.h"
#include "dos2linux.h"
#include "utilities.h"

#define THREADED_REND 1

static int SDL_priv_init(void);
static int SDL_init(void);
static void SDL_close(void);
static int SDL_set_videomode(struct vid_mode_params vmp);
static int SDL_update_screen(void);
static void SDL_put_image(int x, int y, unsigned width, unsigned height);
static void SDL_change_mode(int x_res, int y_res, int w_x_res,
			    int w_y_res);
static void SDL_handle_events(void);
/* interface to xmode.exe */
static int SDL_change_config(unsigned, void *);
static void toggle_grab(int kbd);
static void window_grab(int on, int kbd);
static struct bitmap_desc lock_surface(void);
static void unlock_surface(void);

static struct video_system Video_SDL = {
  SDL_priv_init,
  SDL_init,
  NULL,
  NULL,
  SDL_close,
  SDL_set_videomode,
  SDL_update_screen,
  SDL_change_config,
  SDL_handle_events,
  "sdl"
};

static struct render_system Render_SDL = {
  .lock = lock_surface,
  .unlock = unlock_surface,
  .refresh_rect = SDL_put_image,
  .name = "sdl",
};

static SDL_Renderer *renderer;
static SDL_Surface *surface;
static SDL_Texture *texture_buf;
static SDL_Window *window;
static ColorSpaceDesc SDL_csd;
static Uint32 pixel_format;
static int font_width, font_height;
static int win_width, win_height;
static int m_x_res, m_y_res;
static int use_bitmap_font;
static pthread_mutex_t rects_mtx = PTHREAD_MUTEX_INITIALIZER;
static int sdl_rects_num;
static pthread_mutex_t rend_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t tex_mtx = PTHREAD_MUTEX_INITIALIZER;

static int force_grab = 0;
static int grab_active = 0;
static int kbd_grab_active = 0;
static int m_cursor_visible;
static int initialized;
static int wait_kup;

#ifndef USE_DL_PLUGINS
#undef X_SUPPORT
#endif

#ifdef X_SUPPORT
#ifdef USE_DL_PLUGINS
static void *X_handle;
#define X_load_text_font pX_load_text_font
static int (*X_load_text_font) (Display * display, int private_dpy,
				Window window, const char *p, int *w,
				int *h);
#define X_handle_text_expose pX_handle_text_expose
static int (*X_handle_text_expose) (void);
#define X_close_text_display pX_close_text_display
static int (*X_close_text_display) (void);
#define X_pre_init pX_pre_init
static int (*X_pre_init) (void);
#define X_register_speaker pX_register_speaker
static void (*X_register_speaker) (Display * display);
#define X_set_resizable pX_set_resizable
static void (*X_set_resizable) (Display * display, Window window, int on,
				int x_res, int y_res);
#define X_process_key pX_process_key
static void (*X_process_key)(Display *display, XKeyEvent *e);
#endif

#define CONFIG_SDL_SELECTION 1

static struct {
  Display *display;
  Window window;
} x11;

static void preinit_x11_support(void)
{
#ifdef USE_DL_PLUGINS
  void *handle = load_plugin("X");
  X_register_speaker = dlsym(handle, "X_register_speaker");
  X_load_text_font = dlsym(handle, "X_load_text_font");
  X_pre_init = dlsym(handle, "X_pre_init");
  X_close_text_display = dlsym(handle, "X_close_text_display");
  X_handle_text_expose = dlsym(handle, "X_handle_text_expose");
  X_set_resizable = dlsym(handle, "X_set_resizable");
  X_process_key = dlsym(handle, "X_process_key");
  X_pre_init();
  X_handle = handle;
#endif
}

static void init_x11_support(SDL_Window * win)
{
  int ret;
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);
  if (SDL_GetWindowWMInfo(win, &info) && info.subsystem == SDL_SYSWM_X11) {
    x11.display = info.info.x11.display;
    x11.window = info.info.x11.window;
    init_SDL_keyb(X_handle, x11.display);
    ret = X_load_text_font(x11.display, 1, x11.window, config.X_font,
			   &font_width, &font_height);
    use_bitmap_font = !ret;
  }
}
#endif				/* X_SUPPORT */

static void SDL_done(void)
{
  assert(config.sdl);
  SDL_Quit();
}

int SDL_priv_init(void)
{
  /* The privs are needed for opening /dev/input/mice.
   * Unfortunately SDL does not support gpm.
   * Also, as a bonus, /dev/fb0 can be opened with privs. */
  PRIV_SAVE_AREA
  int ret;
  assert(pthread_equal(pthread_self(), dosemu_pthread_self));
#ifdef X_SUPPORT
  preinit_x11_support();
#endif
  enter_priv_on();
  ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  leave_priv_setting();
  if (ret < 0) {
    error("SDL init: %s\n", SDL_GetError());
    return -1;
  }
  register_exit_handler(SDL_done);
  c_printf("VID: initializing SDL plugin\n");
  return 0;
}

int SDL_init(void)
{
  Uint32 flags = SDL_WINDOW_HIDDEN;
  Uint32 rflags = config.sdl_swrend ? SDL_RENDERER_SOFTWARE : 0;
  int bpp, features;
  Uint32 rm, gm, bm, am;

  assert(pthread_equal(pthread_self(), dosemu_pthread_self));
  if (config.X_lin_filt || config.X_bilin_filt) {
    v_printf("SDL: enabling scaling filter\n");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
  }
  if (config.X_fullscreen)
    flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
  else
    flags |= SDL_WINDOW_RESIZABLE;
#if 0
  /* it is better to create window and renderer at once. They have
   * internal cyclic dependencies, so if you create renderer after
   * creating window, SDL will destroy and re-create the window. */
  int err = SDL_CreateWindowAndRenderer(0, 0, flags, &window, &renderer);
  if (err || !window || !renderer) {
    error("SDL window failed: %s\n", SDL_GetError());
    goto err;
  }
  SDL_SetWindowTitle(window, config.X_title);
#else
  window = SDL_CreateWindow(config.X_title, SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED, 0, 0, flags);
  if (!window) {
    error("SDL window failed: %s\n", SDL_GetError());
    goto err;
  }
  renderer = SDL_CreateRenderer(window, -1, rflags);
  if (!renderer) {
    error("SDL renderer failed: %s\n", SDL_GetError());
    goto err;
  }
#endif

#ifdef X_SUPPORT
  init_x11_support(window);
#else
  use_bitmap_font = 1;
#endif

  if (config.X_fullscreen) {
    window_grab(1, 1);
    force_grab = 1;
  }

  pixel_format = SDL_GetWindowPixelFormat(window);
  if (pixel_format == SDL_PIXELFORMAT_UNKNOWN) {
    error("SDL: unable to get pixel format\n");
    pixel_format = SDL_PIXELFORMAT_RGB888;
  }
  SDL_PixelFormatEnumToMasks(pixel_format, &bpp, &rm, &gm, &bm, &am);
  SDL_csd.bits = bpp;
  SDL_csd.r_mask = rm;
  SDL_csd.g_mask = gm;
  SDL_csd.b_mask = bm;
  color_space_complete(&SDL_csd);
  features = 0;
  if (use_bitmap_font)
    features |= RFF_BITMAP_FONT;
  register_render_system(&Render_SDL);
  if (remapper_init(1, 1, features, &SDL_csd)) {
    error("SDL: SDL_init: VGAEmu init failed!\n");
    config.exitearly = 1;
    return -1;
  }

  c_printf("VID: SDL plugin initialization completed\n");

  return 0;

err:
  SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  return -1;
}

void SDL_close(void)
{
  remapper_done();
  vga_emu_done();
#ifdef X_SUPPORT
  if (x11.display && x11.window != None)
    X_close_text_display();
#endif
  /* destroy texture before renderer, or crash */
  SDL_DestroyTexture(texture_buf);
  SDL_DestroyRenderer(renderer);
  SDL_FreeSurface(surface);
  SDL_DestroyWindow(window);
  SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
}

static void do_redraw(void)
{
  pthread_mutex_lock(&rend_mtx);
  SDL_RenderPresent(renderer);
  pthread_mutex_unlock(&rend_mtx);
}

static void do_redraw_full(void)
{
  pthread_mutex_lock(&tex_mtx);
  pthread_mutex_lock(&rend_mtx);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture_buf, NULL, NULL);
  SDL_RenderPresent(renderer);
  pthread_mutex_unlock(&rend_mtx);
  pthread_mutex_unlock(&tex_mtx);
}

static void SDL_update(void)
{
  int i;
  pthread_mutex_lock(&rects_mtx);
  i = sdl_rects_num;
  sdl_rects_num = 0;
  pthread_mutex_unlock(&rects_mtx);
  if (i > 0)
    do_redraw();
}

static void SDL_redraw(void)
{
#ifdef X_SUPPORT
  if (x11.display && !use_bitmap_font && vga.mode_class == TEXT) {
    redraw_text_screen();
    return;
  }
#endif

  do_redraw_full();
}

static struct bitmap_desc lock_surface(void)
{
  int err;

  err = SDL_LockSurface(surface);
  assert(!err);
  return BMP(surface->pixels, win_width, win_height, surface->pitch);
}


static void unlock_surface(void)
{
  int i;

  SDL_UnlockSurface(surface);

  pthread_mutex_lock(&rects_mtx);
  i = sdl_rects_num;
  pthread_mutex_unlock(&rects_mtx);
  if (!i) {
#if 1
    v_printf("ERROR: update with zero rects count\n");
#else
    error("update with zero rects count\n");
#endif
    return;
  }

  pthread_mutex_lock(&rend_mtx);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture_buf, NULL, NULL);
  pthread_mutex_unlock(&rend_mtx);
}

int SDL_set_videomode(struct vid_mode_params vmp)
{
  v_printf
      ("SDL: X_setmode: video_mode 0x%x (%s), size %d x %d (%d x %d pixel)\n",
       video_mode, vmp.mode_class ? "GRAPH" : "TEXT",
       vmp.text_width, vmp.text_height, vmp.x_res, vmp.y_res);
  if (win_width == vmp.x_res && win_height == vmp.y_res) {
    v_printf("SDL: same mode, not changing\n");
    return 1;
  }
  if (vmp.mode_class == TEXT && !use_bitmap_font)
    SDL_change_mode(0, 0, vmp.text_width * font_width,
		      vmp.text_height * font_height);
  else
    SDL_change_mode(vmp.x_res, vmp.y_res, vmp.w_x_res, vmp.w_y_res);

  return 1;
}

static void set_resizable(int on, int x_res, int y_res)
{
#if 0
  /* no such api :( */
  SDL_SetWindowResizable(window, on ? SDL_ENABLE : SDL_DISABLE);
#else
#ifdef X_SUPPORT
  if (x11.display)
    X_set_resizable(x11.display, x11.window, on, x_res, y_res);
#endif
#endif
}

static void sync_mouse_coords(void)
{
  int m_x, m_y;

  SDL_GetMouseState(&m_x, &m_y);
  mouse_move_absolute(m_x, m_y, m_x_res, m_y_res);
}

static void update_mouse_coords(void)
{
  if (grab_active)
    return;
  sync_mouse_coords();
}

static void SDL_change_mode(int x_res, int y_res, int w_x_res, int w_y_res)
{
  Uint32 flags;

  assert(pthread_equal(pthread_self(), dosemu_pthread_self));
  v_printf("SDL: using mode %dx%d %dx%d %d\n", x_res, y_res, w_x_res,
	   w_y_res, SDL_csd.bits);
  if (surface) {
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture_buf);
  }
  if (x_res > 0 && y_res > 0) {
    texture_buf = SDL_CreateTexture(renderer,
        pixel_format,
        SDL_TEXTUREACCESS_STREAMING,
        x_res, y_res);
    if (!texture_buf) {
      error("SDL target texture failed: %s\n", SDL_GetError());
      leavedos(99);
    }
    surface = SDL_CreateRGBSurface(0, x_res, y_res, SDL_csd.bits,
            SDL_csd.r_mask, SDL_csd.g_mask, SDL_csd.b_mask, 0);
    if (!surface) {
      error("SDL surface failed: %s\n", SDL_GetError());
      leavedos(99);
    }
  } else {
    surface = NULL;
    texture_buf = NULL;
  }

  if (config.X_fixed_aspect)
    SDL_RenderSetLogicalSize(renderer, w_x_res, w_y_res);
  flags = SDL_GetWindowFlags(window);
  if (!(flags & SDL_WINDOW_MAXIMIZED))
    SDL_SetWindowSize(window, w_x_res, w_y_res);
  set_resizable(use_bitmap_font
		|| vga.mode_class == GRAPH, w_x_res, w_y_res);
  if (!initialized) {
    initialized = 1;
    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);
  }
  SDL_RenderClear(renderer);
  SDL_RenderPresent(renderer);
  if (texture_buf) {
    SDL_SetRenderTarget(renderer, texture_buf);
    SDL_RenderClear(renderer);
  }

  m_x_res = w_x_res;
  m_y_res = w_y_res;
  win_width = x_res;
  win_height = y_res;
  /* forget about those rectangles */
  sdl_rects_num = 0;

  update_mouse_coords();
}

int SDL_update_screen(void)
{
  if (render_is_updating())
    return 0;
#ifdef X_SUPPORT
  if (!use_bitmap_font && vga.mode_class == TEXT)
    return 0;
#endif
  SDL_update();
  return 0;
}

static void SDL_put_image(int x, int y, unsigned width, unsigned height)
{
  const SDL_Rect rect = { .x = x, .y = y, .w = width, .h = height };
  int offs = x * SDL_csd.bits / 8 + y * surface->pitch;

  pthread_mutex_lock(&tex_mtx);
  SDL_UpdateTexture(texture_buf, &rect, surface->pixels + offs,
      surface->pitch);
  pthread_mutex_unlock(&tex_mtx);
  pthread_mutex_lock(&rects_mtx);
  sdl_rects_num++;
  pthread_mutex_unlock(&rects_mtx);
}

static void window_grab(int on, int kbd)
{
  if (on) {
    if (kbd) {
      SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "1");
      v_printf("SDL: keyboard grab activated\n");
    } else {
      SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "0");
    }
    SDL_SetWindowGrab(window, SDL_TRUE);
    v_printf("SDL: mouse grab activated\n");
    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    mouse_enable_native_cursor(1);
    kbd_grab_active = kbd;
  } else {
    v_printf("SDL: grab released\n");
    SDL_SetWindowGrab(window, SDL_FALSE);
    if (m_cursor_visible)
      SDL_ShowCursor(SDL_ENABLE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    mouse_enable_native_cursor(0);
    kbd_grab_active = 0;
    sync_mouse_coords();
  }
  grab_active = on;
  /* update title with grab info */
  SDL_change_config(CHG_TITLE, NULL);
}

static void toggle_grab(int kbd)
{
  window_grab(grab_active ^ 1, kbd);
}

static void toggle_fullscreen_mode(void)
{
  config.X_fullscreen = !config.X_fullscreen;
  if (config.X_fullscreen) {
    v_printf("SDL: entering fullscreen mode\n");
    if (!grab_active) {
      window_grab(1, 1);
      force_grab = 1;
    }
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
  } else {
    v_printf("SDL: entering windowed mode!\n");
    SDL_SetWindowFullscreen(window, 0);
    if (force_grab && grab_active) {
      window_grab(0, 0);
    }
    force_grab = 0;
  }
}

/*
 * This function provides an interface to reconfigure parts
 * of SDL and the VGA emulation during a DOSEMU session.
 * It is used by the xmode.exe program that comes with DOSEMU.
 */
static int SDL_change_config(unsigned item, void *buf)
{
  int err = 0;

  v_printf("SDL: SDL_change_config: item = %d, buffer = %p\n", item, buf);

  switch (item) {

  case CHG_TITLE:
    /* low-level write */
    if (buf) {
      char *sw, /**si,*/ *charset;
/*	size_t iconlen = strlen(config.X_icon_name) + 1;
	wchar_t iconw[iconlen];
	if (mbstowcs(iconw, config.X_icon_name, iconlen) == -1)
	  iconlen = 1;
	iconw[iconlen-1] = 0;*/
      charset = "utf8";
      sw = unicode_string_to_charset(buf, charset);
//      si = unicode_string_to_charset(iconw, charset);
      v_printf("SDL: SDL_change_config: win_name = %s\n", sw);
      SDL_SetWindowTitle(window, sw);
      free(sw);
//      free(si);
      break;
    }
    /* high-level write (shows name of emulator + running app) */
    /* fallthrough */

  case CHG_TITLE_EMUNAME:
  case CHG_TITLE_APPNAME:
  case CHG_TITLE_SHOW_APPNAME:
  case CHG_WINSIZE:
  case CHG_BACKGROUND_PAUSE:
  case GET_TITLE_APPNAME:
    change_config(item, buf, grab_active, kbd_grab_active);
    break;

#ifdef X_SUPPORT
  case CHG_FONT:{
      if (!x11.display || x11.window == None || use_bitmap_font)
	break;
      X_load_text_font(x11.display, 1, x11.window, buf,
		       &font_width, &font_height);
      if (win_width != vga.text_width * font_width ||
	  win_height != vga.text_height * font_height) {
	if (vga.mode_class == TEXT) {
	  render_mode_lock();
	  SDL_change_mode(0, 0, vga.text_width * font_width,
			  vga.text_height * font_height);
	  render_mode_unlock();
	}
      }
      break;
    }
#endif

  case CHG_FULLSCREEN:
    v_printf("SDL: SDL_change_config: fullscreen %i\n", *((int *) buf));
    if (*((int *) buf) == !config.X_fullscreen)
      toggle_fullscreen_mode();
    break;

  default:
    err = 100;
  }

  return err;
}

#if CONFIG_SDL_SELECTION
static char *get_selection_string(t_unicode sel_text[], char *charset)
{
	struct char_set_state paste_state;
	struct char_set *paste_charset;
	t_unicode *u = sel_text;
	char *s, *p;
	size_t sel_space = 0;

	while (sel_text[sel_space])
		sel_space++;
	paste_charset = lookup_charset(charset);
	sel_space *= MB_LEN_MAX;
	p = s = malloc(sel_space);
	init_charset_state(&paste_state, paste_charset);

	while (*u) {
		size_t result = unicode_to_charset(&paste_state, *u++,
						   (unsigned char *)p, sel_space);
		if (result == -1) {
			warn("save_selection unfinished2\n");
			break;
		}
		p += result;
		sel_space -= result;
	}
	*p = '\0';
	cleanup_charset_state(&paste_state);
	return s;
}

static int shift_pressed(void)
{
  const Uint8 *state = SDL_GetKeyboardState(NULL);
  return (state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT]);
}

static int window_has_focus(void)
{
  uint32_t flags = SDL_GetWindowFlags(window);
  return (flags & SDL_WINDOW_INPUT_FOCUS);
}
#endif				/* CONFIG_SDL_SELECTION */

static void SDL_handle_events(void)
{
  SDL_Event event;

  assert(pthread_equal(pthread_self(), dosemu_pthread_self));
  if (render_is_updating())
    return;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {

    case SDL_WINDOWEVENT:
      switch (event.window.event) {
      case SDL_WINDOWEVENT_FOCUS_GAINED:
	v_printf("SDL: focus in\n");
	render_gain_focus();
	if (config.X_background_pause && !dosemu_user_froze)
	  unfreeze_dosemu();
	break;
      case SDL_WINDOWEVENT_FOCUS_LOST:
	v_printf("SDL: focus out\n");
	render_lose_focus();
	if (config.X_background_pause && !dosemu_user_froze)
	  freeze_dosemu();
	break;
      case SDL_WINDOWEVENT_RESIZED:
	/* very strange things happen: if renderer size was explicitly
	 * set, SDL reports mouse coords relative to that. Otherwise
	 * it reports mouse coords relative to the window. */
	SDL_RenderGetLogicalSize(renderer, &m_x_res, &m_y_res);
	if (!m_x_res || !m_y_res) {
	  m_x_res = event.window.data1;
	  m_y_res = event.window.data2;
	}
	update_mouse_coords();
	SDL_redraw();
	break;
      case SDL_WINDOWEVENT_EXPOSED:
	SDL_redraw();
	break;
      case SDL_WINDOWEVENT_ENTER:
        mouse_drag_to_corner(m_x_res, m_y_res);
        break;
      }
      break;

    case SDL_KEYDOWN:
      {
	if (wait_kup)
	  break;
	SDL_Keysym keysym = event.key.keysym;
	if ((keysym.mod & KMOD_CTRL) && (keysym.mod & KMOD_ALT)) {
	  if (keysym.sym == SDLK_HOME || keysym.sym == SDLK_k) {
	    force_grab = 0;
	    toggle_grab(keysym.sym == SDLK_k);
	    break;
	  } else if (keysym.sym == SDLK_f) {
	    toggle_fullscreen_mode();
	    /* some versions of SDL re-send the keydown events after the
	     * full-screen switch. We need to filter them out to prevent
	     * the infinite switching loop. */
	    wait_kup = 1;
	    break;
	  }
	}
      }
#if CONFIG_SDL_SELECTION
      clear_if_in_selection();
#endif
#ifdef X_SUPPORT
      if (x11.display && config.X_keycode)
	SDL_process_key_xkb(x11.display, event.key);
      else
#endif
	SDL_process_key(event.key);
      break;
    case SDL_KEYUP:
      wait_kup = 0;
#ifdef X_SUPPORT
      if (x11.display && config.X_keycode)
	SDL_process_key_xkb(x11.display, event.key);
      else
#endif
	SDL_process_key(event.key);
      break;

    case SDL_MOUSEBUTTONDOWN:
      {
	int buttons = SDL_GetMouseState(NULL, NULL);
#if CONFIG_SDL_SELECTION
	if (window_has_focus() && !shift_pressed()) {
	  clear_selection_data();
	} else if (vga.mode_class == TEXT && !grab_active) {
	  if (event.button.button == SDL_BUTTON_LEFT)
	    start_selection(x_to_col(event.button.x, m_x_res),
			    y_to_row(event.button.y, m_y_res));
	  else if (event.button.button == SDL_BUTTON_RIGHT)
	    start_extend_selection(x_to_col(event.button.x, m_x_res),
				   y_to_row(event.button.y, m_y_res));
	  else if (event.button.button == SDL_BUTTON_MIDDLE) {
	    char *paste = SDL_GetClipboardText();
	    if (paste)
	      paste_text(paste, strlen(paste), "utf8");
	  }
	  break;
	}
#endif				/* CONFIG_SDL_SELECTION */
	mouse_move_buttons(buttons & SDL_BUTTON(1),
			   buttons & SDL_BUTTON(2),
			   buttons & SDL_BUTTON(3));
	break;
      }
    case SDL_MOUSEBUTTONUP:
      {
	int buttons = SDL_GetMouseState(NULL, NULL);
#if CONFIG_SDL_SELECTION
	if (vga.mode_class == TEXT && !grab_active) {
	    t_unicode *sel = end_selection();
	    if (sel) {
		char *send_text = get_selection_string(sel, "utf8");
		SDL_SetClipboardText(send_text);
		free(send_text);
	    }
	}
#endif				/* CONFIG_SDL_SELECTION */
	mouse_move_buttons(buttons & SDL_BUTTON(1),
			   buttons & SDL_BUTTON(2),
			   buttons & SDL_BUTTON(3));
	break;
      }

    case SDL_MOUSEMOTION:
#if CONFIG_SDL_SELECTION
      extend_selection(x_to_col(event.motion.x, m_x_res),
			 y_to_row(event.motion.y, m_y_res));
#endif				/* CONFIG_SDL_SELECTION */
      if (grab_active)
	mouse_move_relative(event.motion.xrel, event.motion.yrel,
			    m_x_res, m_y_res);
      else
	mouse_move_absolute(event.motion.x, event.motion.y, m_x_res,
			    m_y_res);
      break;
    case SDL_QUIT:
      leavedos(0);
      break;
    default:
      v_printf("PAS ENCORE TRAITE %x\n", event.type);
      /* TODO */
      break;
    }
  }

#ifdef X_SUPPORT
  if (x11.display && !use_bitmap_font && vga.mode_class == TEXT &&
      X_handle_text_expose()) {
    /* need to check separately because SDL_VIDEOEXPOSE is eaten by SDL */
    redraw_text_screen();
  }
#endif
}

static int SDL_mouse_init(void)
{
  mouse_t *mice = &config.mouse;
  if (Video != &Video_SDL)
    return FALSE;

  mice->type = MOUSE_SDL;
  mouse_enable_native_cursor(config.X_fullscreen);
  /* we have the X cursor, but if we start fullscreen, grab by default */
  m_printf("MOUSE: SDL Mouse being set\n");
  return TRUE;
}

static void SDL_show_mouse_cursor(int yes)
{
  m_cursor_visible = yes;
  SDL_ShowCursor((yes && !grab_active) ? SDL_ENABLE : SDL_DISABLE);
}

struct mouse_client Mouse_SDL = {
  "SDL",			/* name */
  SDL_mouse_init,		/* init */
  NULL,				/* close */
  SDL_show_mouse_cursor		/* show_cursor */
};

static void sdl_scrub(void)
{
  /* allow -S -t for SDL audio and terminal video */
  if (config.sdl && config.term) {
    config.sdl = 0;
    config.X = 0;
    Video = NULL;
  }
}

CONSTRUCTOR(static void init(void))
{
  register_video_client(&Video_SDL);
  register_keyboard_client(&Keyboard_SDL);
  register_mouse_client(&Mouse_SDL);
  register_config_scrub(sdl_scrub);
}

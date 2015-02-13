/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#define SDL_C
#include "config.h"

#include <stdio.h>
#include <stdlib.h> /* for malloc & free */
#include <string.h> /* for memset */
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <pthread.h>
#include <SDL.h>
#include <SDL_syswm.h>

#include "emu.h"
#include "timers.h"
#include "init.h"
#include "bios.h"
#include "video.h"
#include "memory.h"
#include "remap.h"
#if SDL_VERSION_ATLEAST(1,2,10) && !defined(SDL_VIDEO_DRIVER_X11)
#undef X_SUPPORT
#endif
#ifdef X_SUPPORT
#include "../X/screen.h"
#include "../X/X.h"
#endif
#include "vgaemu.h"
#include "vgatext.h"
#include "render.h"
#include "sdl.h"
#include "keyb_clients.h"
#include "dos2linux.h"
#include "utilities.h"

static int SDL_priv_init(void);
static int SDL_init(void);
static void SDL_close(void);
static int SDL_set_videomode(int mode_class, int text_width, int text_height);
static int SDL_update_screen(void);
static void SDL_put_image(int x, int y, unsigned width, unsigned height);
static void SDL_change_mode(int x_res, int y_res, int w_x_res, int w_y_res);
static void SDL_handle_events(void);
/* interface to xmode.exe */
static int SDL_change_config(unsigned, void *);
static void toggle_grab(void);
static struct bitmap_desc lock_surface(void);
static void unlock_surface(void);

struct video_system Video_SDL =
{
  SDL_priv_init,
  SDL_init,
  SDL_close,
  SDL_set_videomode,
  SDL_update_screen,
  SDL_change_config,
  SDL_handle_events,
  "sdl"
};

struct render_system Render_SDL =
{
   SDL_put_image,
   lock_surface,
   unlock_surface,
};

static SDL_Surface *surface;
static SDL_Renderer *renderer;
static SDL_Window *window;
static ColorSpaceDesc SDL_csd;
static int font_width, font_height;
static int m_x_res, m_y_res;
static int initialized;
static int use_bitmap_font;
#define UPDATE_BY_RECTS 0
static struct {
  int num;
#if UPDATE_BY_RECTS
  int max;
  SDL_Rect *rects;
#endif
} sdl_rects;
static pthread_mutex_t update_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mode_mtx = PTHREAD_MUTEX_INITIALIZER;

static int force_grab = 0;
int grab_active = 0;
static int m_cursor_visible;
static int init_failed;

#ifdef X_SUPPORT
#ifdef USE_DL_PLUGINS
void *X_handle;
#define X_load_text_font pX_load_text_font
static int (*X_load_text_font)(Display *display, int private_dpy,
				Window window, const char *p,  int *w, int *h);
#define X_handle_text_expose pX_handle_text_expose
static int (*X_handle_text_expose)(void);
#define X_close_text_display pX_close_text_display
static int (*X_close_text_display)(void);
#define X_pre_init pX_pre_init
static int (*X_pre_init)(void);
#define X_register_speaker pX_register_speaker
static void (*X_register_speaker)(Display *display);
#define X_set_resizable pX_set_resizable
static void (*X_set_resizable)(Display *display, Window window, int on,
	int x_res, int y_res);
#endif

#ifdef CONFIG_SELECTION
#ifdef USE_DL_PLUGINS
#define X_handle_selection pX_handle_selection
static void (*X_handle_selection)(Display *display, Window mainwindow,
				  XEvent *e);
#endif
#define CONFIG_SDL_SELECTION 1
#endif /* CONFIG_SELECTION */

static struct {
  Display *display;
  Window window;
  void (*lock_func)(void);
  void (*unlock_func)(void);
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
#ifdef CONFIG_SDL_SELECTION
  X_handle_selection = dlsym(handle, "X_handle_selection");
#endif
  X_pre_init();
  X_handle = handle;
#endif
}

static void X_lock_display(void)
{
  XLockDisplay(x11.display);
}

static void X_unlock_display(void)
{
  XUnlockDisplay(x11.display);
}

static void init_x11_support(SDL_Window *win)
{
  int ret;
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);
  if (SDL_GetWindowWMInfo(win, &info) && info.subsystem == SDL_SYSWM_X11) {
#if CONFIG_SDL_SELECTION
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
#endif
    x11.display = info.info.x11.display;
    x11.window = info.info.x11.window;
    x11.lock_func = X_lock_display;
    x11.unlock_func = X_unlock_display;
    init_SDL_keyb(X_handle, x11.display);
    x11.lock_func();
    ret = X_load_text_font(x11.display, 1, x11.window, config.X_font,
		       &font_width, &font_height);
    x11.unlock_func();
    use_bitmap_font = !ret;
  }
}
#endif /* X_SUPPORT */

int SDL_priv_init(void)
{
  /* The privs are needed for opening /dev/input/mice.
   * Unfortunately SDL does not support gpm.
   * Also, as a bonus, /dev/fb0 can be opened with privs. */
  PRIV_SAVE_AREA
  int ret;
#ifdef X_SUPPORT
  preinit_x11_support();
#endif
  enter_priv_on();
  ret = SDL_Init(SDL_INIT_VIDEO);
  leave_priv_setting();
  if (ret < 0) {
    error("SDL init: %s\n", SDL_GetError());
    config.exitearly = 1;
    init_failed = 1;
    return -1;
  }
  if (config.sdl_nogl) {
    v_printf("SDL: Disabling OpenGL framebuffer acceleration\n");
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");
  }
  return 0;
}

int SDL_init(void)
{
  Uint32 flags = SDL_WINDOW_HIDDEN;
  int remap_src_modes, bpp, features;
  Uint32 rm, gm, bm, am, pix_fmt;

  if (init_failed)
    return -1;

  if(config.X_lin_filt || config.X_bilin_filt) {
    v_printf("SDL: enabling scaling filter\n");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
  }
  if (config.X_fullscreen)
    flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
  else
    flags |= SDL_WINDOW_RESIZABLE;
  window = SDL_CreateWindow(config.X_title, SDL_WINDOWPOS_UNDEFINED,
    SDL_WINDOWPOS_UNDEFINED, 0, 0, flags);
  if (!window) {
    error("SDL window failed\n");
    init_failed = 1;
    return -1;
  }
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    error("SDL renderer failed\n");
    init_failed = 1;
    return -1;
  }

#ifdef X_SUPPORT
  init_x11_support(window);
#else
  use_bitmap_font = 1;
#endif

  if (config.X_fullscreen)
    toggle_grab();

  pix_fmt = SDL_GetWindowPixelFormat(window);
  if (pix_fmt == SDL_PIXELFORMAT_UNKNOWN) {
    error("SDL: unable to get pixel format\n");
    pix_fmt = SDL_PIXELFORMAT_RGB888;
  }
  SDL_PixelFormatEnumToMasks(pix_fmt, &bpp, &rm, &gm, &bm, &am);
  SDL_csd.bits = bpp;
  SDL_csd.bytes = (bpp + 7) >> 3;
  SDL_csd.r_mask = rm;
  SDL_csd.g_mask = gm;
  SDL_csd.b_mask = bm;
  color_space_complete(&SDL_csd);
  features = 0;
  if (use_bitmap_font)
    features |= RFF_BITMAP_FONT;
  remap_src_modes = remapper_init(1, 1, features, &SDL_csd);
  register_render_system(&Render_SDL);

  if(vga_emu_init(remap_src_modes, &SDL_csd)) {
    error("SDL: SDL_init: VGAEmu init failed!\n");
    config.exitearly = 1;
    return -1;
  }

  return 0;
}

void SDL_close(void)
{
  if (!initialized)
    return;
  remapper_done();
  vga_emu_done();
#ifdef X_SUPPORT
  if (x11.display && x11.window != None)
    X_close_text_display();
#endif
  SDL_DestroyRenderer(renderer);
  SDL_FreeSurface(surface);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

static void do_redraw(void)
{
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
  SDL_DestroyTexture(texture);
}

static void SDL_update(void)
{
  int i;
  pthread_mutex_lock(&mode_mtx);
  pthread_mutex_lock(&update_mtx);
  /* sdl manual says:
  ---
    The backbuffer should be considered invalidated after each present;
    do not assume that previous contents will exist between frames.
    You are strongly encouraged to call SDL_RenderClear() to initialize
    the backbuffer before starting each new frame's drawing, even if
    you plan to overwrite every pixel.
  ---
   * So by default we define it to 0. But it seems it works fine also
   * without clearing so we can as well try the update with rects, as
   * was in the old sdl1 code. If there are many rects to update, it
   * can easily be faster to just update the entire screen though. */
#if UPDATE_BY_RECTS
  if (sdl_rects.num > 0) {
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    for (i = 0; i < sdl_rects.num; i++) {
      const SDL_Rect *rec = &sdl_rects.rects[i];
      SDL_RenderCopy(renderer, texture, rec, rec);
    }
    SDL_RenderPresent(renderer);
    SDL_DestroyTexture(texture);
    sdl_rects.num = 0;
  }
  pthread_mutex_unlock(&update_mtx);
#else
  i = sdl_rects.num;
  sdl_rects.num = 0;
  pthread_mutex_unlock(&update_mtx);
  if (i > 0)
    do_redraw();
#endif
  pthread_mutex_unlock(&mode_mtx);
}

static void SDL_redraw(void)
{
#ifdef X_SUPPORT
  if (x11.display && !use_bitmap_font && vga.mode_class == TEXT) {
    redraw_text_screen();
    return;
  }
#endif
  pthread_mutex_lock(&mode_mtx);
  do_redraw();
  pthread_mutex_unlock(&mode_mtx);
}

static struct bitmap_desc lock_surface(void)
{
  pthread_mutex_lock(&mode_mtx);
  SDL_LockSurface(surface);
  return BMP(surface->pixels, surface->w, surface->h, surface->pitch);
}

static void unlock_surface(void)
{
  SDL_UnlockSurface(surface);
  pthread_mutex_unlock(&mode_mtx);
}

/* NOTE : Like X.c, the actual mode is taken via video_mode */
int SDL_set_videomode(int mode_class, int text_width, int text_height)
{
  int mode = video_mode;
  int x_res, y_res, wx_res, wy_res;

  v_printf("X: X_setmode: %svideo_mode 0x%x (%s), size %d x %d (%d x %d pixel)\n",
    mode_class != -1 ? "" : "re-init ",
    (int) mode, vga.mode_class ? "GRAPH" : "TEXT",
    vga.text_width, vga.text_height, vga.width, vga.height
  );

  get_mode_parameters(&x_res, &y_res, &wx_res, &wy_res);
  if (vga.mode_class == TEXT) {
    pthread_mutex_lock(&mode_mtx);
    if (use_bitmap_font) {
      SDL_change_mode(x_res, y_res, wx_res, wy_res);
    } else {
      SDL_change_mode(0, 0, vga.text_width * font_width,
			vga.text_height * font_height);
    }
    pthread_mutex_unlock(&mode_mtx);
  } else {
    pthread_mutex_lock(&mode_mtx);
    SDL_change_mode(x_res, y_res, wx_res, wy_res);
    pthread_mutex_unlock(&mode_mtx);
  }

  initialized = 1;

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

static void SDL_change_mode(int x_res, int y_res, int w_x_res, int w_y_res)
{
  v_printf("SDL: using mode %dx%d %dx%d %d\n", x_res, y_res, w_x_res, w_y_res,
    SDL_csd.bits);
  if (surface)
    SDL_FreeSurface(surface);
  if (x_res > 0 && y_res > 0) {
    surface = SDL_CreateRGBSurface(0, x_res, y_res, SDL_csd.bits,
	SDL_csd.r_mask, SDL_csd.g_mask, SDL_csd.b_mask, 0);
    if (!surface) {
      error("SDL surface failed\n");
      leavedos(99);
    }
    SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0, 0, 0));
  } else {
    surface = NULL;
  }

  if (config.X_fixed_aspect)
    SDL_RenderSetLogicalSize(renderer, w_x_res, w_y_res);
  SDL_SetWindowSize(window, w_x_res, w_y_res);
  set_resizable(use_bitmap_font || vga.mode_class == GRAPH, w_x_res, w_y_res);
  SDL_ShowWindow(window);
  m_x_res = w_x_res;
  m_y_res = w_y_res;
  pthread_mutex_lock(&update_mtx);
  /* forget about those rectangles */
  sdl_rects.num = 0;
  pthread_mutex_unlock(&update_mtx);
  if (vga.mode_class == GRAPH) {
    SDL_ShowCursor(SDL_DISABLE);
    m_cursor_visible = 0;
  } else {
    SDL_ShowCursor(SDL_ENABLE);
    m_cursor_visible = 1;
  }
}

int SDL_update_screen(void)
{
  int ret;
  if (init_failed || !initialized)
    return 1;
  if (render_is_updating())
    return 0;
#ifdef X_SUPPORT
  if (!use_bitmap_font && vga.mode_class == TEXT)
    return update_screen();
#endif
  /* if render is idle we start async blit (as of SDL_SYNCBLIT) and
   * then start the renderer. It will wait till async blit to finish. */
  SDL_update();
  ret = update_screen();
  return ret;
}

/* this only pushes the rectangle on a stack; updating is done later */
static void SDL_put_image(int x, int y, unsigned width, unsigned height)
{
#if UPDATE_BY_RECTS
  SDL_Rect *rect;
  pthread_mutex_lock(&update_mtx);
  if (sdl_rects.num >= sdl_rects.max) {
    sdl_rects.rects = realloc(sdl_rects.rects, (sdl_rects.max + 10) *
			      sizeof(*sdl_rects.rects));
    sdl_rects.max += 10;
  }
  rect = &sdl_rects.rects[sdl_rects.num];
  rect->x = x;
  rect->y = y;
  rect->w = width;
  rect->h = height;
#else
  pthread_mutex_lock(&update_mtx);
#endif
  sdl_rects.num++;
  pthread_mutex_unlock(&update_mtx);
}

static void toggle_grab(void)
{
  if(grab_active ^= 1) {
    v_printf("SDL: grab activated\n");
    if (!config.X_fullscreen)
      SDL_SetWindowGrab(window, SDL_TRUE);
    config.mouse.use_absolute = 0;
    v_printf("SDL: mouse grab activated\n");
    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    mouse_enable_native_cursor(1);
  }
  else {
    config.mouse.use_absolute = 1;
    v_printf("SDL: grab released\n");
    if (!config.X_fullscreen)
      SDL_SetWindowGrab(window, SDL_FALSE);
    if (m_cursor_visible)
      SDL_ShowCursor(SDL_ENABLE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    mouse_enable_native_cursor(0);
  }
  SDL_change_config(CHG_TITLE, NULL);
}

static void toggle_fullscreen_mode(void)
{
  config.X_fullscreen = !config.X_fullscreen;
  if (config.X_fullscreen) {
    v_printf("SDL: entering fullscreen mode\n");
    if (!grab_active) {
      toggle_grab();
      force_grab = 1;
    }
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
  } else {
    v_printf("SDL: entering windowed mode!\n");
    SDL_SetWindowFullscreen(window, 0);
    if (force_grab && grab_active) {
      toggle_grab();
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

  switch(item) {

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
//	si = unicode_string_to_charset(iconw, charset);
	v_printf("SDL: SDL_change_config: win_name = %s\n", sw);
	SDL_SetWindowTitle(window, sw);
	free(sw);
//	free(si);
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
      change_config (item, buf, grab_active, grab_active);
      break;

#ifdef X_SUPPORT
    case CHG_FONT: {
      if (!x11.display || x11.window == None || use_bitmap_font)
	break;
      x11.lock_func();
      X_load_text_font(x11.display, 1, x11.window, buf,
		       &font_width, &font_height);
      x11.unlock_func();
      if (surface->w != vga.text_width * font_width ||
            surface->h != vga.text_height * font_height) {
	  if(vga.mode_class == TEXT) {
	    pthread_mutex_lock(&mode_mtx);
	    SDL_change_mode(0, 0, vga.text_width * font_width,
		vga.text_height * font_height);
	    pthread_mutex_unlock(&mode_mtx);
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

static void SDL_handle_selection(XEvent *e)
{
  switch(e->type) {
  case SelectionClear:
  case SelectionNotify:
  case SelectionRequest:
  case ButtonRelease:
    if (x11.display && x11.window != None) {
      x11.lock_func();
      X_handle_selection(x11.display, x11.window, e);
      x11.unlock_func();
    }
    break;
  default:
    break;
  }
}

#endif /* CONFIG_SDL_SELECTION */

static void SDL_handle_events(void)
{
   SDL_Event event;
   if (!initialized)
     return;
  if (render_is_updating())
     return;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {

    case SDL_WINDOWEVENT:
      switch (event.window.event) {
      case SDL_WINDOWEVENT_FOCUS_GAINED:
        v_printf("SDL: focus in\n");
        render_gain_focus();
        if (config.X_background_pause && !dosemu_user_froze) unfreeze_dosemu ();
        break;
      case SDL_WINDOWEVENT_FOCUS_LOST:
        v_printf("SDL: focus out\n");
        render_lose_focus();
        if (config.X_background_pause && !dosemu_user_froze) freeze_dosemu ();
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
        SDL_redraw();
        break;
      case SDL_WINDOWEVENT_EXPOSED:
        SDL_redraw();
        break;
    }
    break;

     case SDL_KEYDOWN:
       {
	 SDL_Keysym keysym = event.key.keysym;
	 if ((keysym.mod & KMOD_CTRL) && (keysym.mod & KMOD_ALT)) {
	   if (keysym.sym == SDLK_HOME || keysym.sym == SDLK_k) {
	     force_grab = 0;
	     toggle_grab();
	     break;
	   } else if (keysym.sym == SDLK_f) {
	     toggle_fullscreen_mode();
	     break;
	   }
	 }
       }
#if CONFIG_SDL_SELECTION
       clear_if_in_selection();
#endif
#ifdef X_SUPPORT
       if (x11.display)
         SDL_process_key_xkb(x11.display, event.key);
       else
#endif
       SDL_process_key(event.key);
       break;
     case SDL_KEYUP:
#if CONFIG_SDL_SELECTION
       clear_if_in_selection();
#endif
#ifdef X_SUPPORT
       if (x11.display)
         SDL_process_key_xkb(x11.display, event.key);
       else
#endif
       SDL_process_key(event.key);
       break;
     case SDL_MOUSEBUTTONDOWN:
       {
	 int buttons = SDL_GetMouseState(NULL, NULL);
#if CONFIG_SDL_SELECTION
	 if (x11.display && vga.mode_class == TEXT && !grab_active) {
	   if (event.button.button == SDL_BUTTON_LEFT)
	     start_selection(x_to_col(event.button.x, m_x_res),
			     y_to_row(event.button.y, m_y_res));
	   else if (event.button.button == SDL_BUTTON_RIGHT)
	     start_extend_selection(x_to_col(event.button.x, m_x_res),
				    y_to_row(event.button.y, m_y_res));
	 }
#endif /* CONFIG_SDL_SELECTION */
	 mouse_move_buttons(buttons & SDL_BUTTON(1), buttons & SDL_BUTTON(2), buttons & SDL_BUTTON(3));
	 break;
       }
     case SDL_MOUSEBUTTONUP:
       {
	 int buttons = SDL_GetMouseState(NULL, NULL);
#if CONFIG_SDL_SELECTION
	 if (x11.display && vga.mode_class == TEXT && !grab_active) {
	   XEvent e;
	   e.type = ButtonRelease;
	   e.xbutton.button = 0;
	   if (event.button.button == SDL_BUTTON_LEFT)
	     e.xbutton.button = Button1;
	   else if (event.button.button == SDL_BUTTON_MIDDLE)
	     e.xbutton.button = Button2;
	   else if (event.button.button == SDL_BUTTON_RIGHT)
	     e.xbutton.button = Button3;
	   e.xbutton.time = CurrentTime;
	   SDL_handle_selection(&e);
	 }
#endif /* CONFIG_SDL_SELECTION */
	 mouse_move_buttons(buttons & SDL_BUTTON(1), buttons & SDL_BUTTON(2), buttons & SDL_BUTTON(3));
	 break;
       }

     case SDL_MOUSEMOTION:
#if CONFIG_SDL_SELECTION
       if (x11.display && x11.window != None)
	 extend_selection(x_to_col(event.motion.x, m_x_res),
			  y_to_row(event.motion.y, m_y_res));
#endif /* CONFIG_SDL_SELECTION */
       if (grab_active)
         mouse_move_relative(event.motion.xrel, event.motion.yrel,
            m_x_res, m_y_res);
       else
         mouse_move_absolute(event.motion.x, event.motion.y, m_x_res, m_y_res);
       break;
     case SDL_QUIT:
       leavedos(0);
       break;
#if CONFIG_SDL_SELECTION
     case SDL_SYSWMEVENT:
       if (x11.display)
	 SDL_handle_selection(&event.syswm.msg->msg.x11.event);
       break;
#endif /* CONFIG_SDL_SELECTION */
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
  mice->use_absolute = 1;
  mice->native_cursor = config.X_fullscreen;
  /* we have the X cursor, but if we start fullscreen, grab by default */
  m_printf("MOUSE: SDL Mouse being set\n");
  return TRUE;
}

static void SDL_show_mouse_cursor(int yes)
{
  m_cursor_visible = yes;
  SDL_ShowCursor((yes && !grab_active) ? SDL_ENABLE : SDL_DISABLE);
}

static void SDL_set_mouse_cursor(int action, int mx, int my, int x_range,
    int y_range)
{
  if (action & 2)
    SDL_show_mouse_cursor(action >> 1);
}

struct mouse_client Mouse_SDL =  {
  "SDL",          /* name */
  SDL_mouse_init, /* init */
  NULL,         /* close */
  NULL,         /* run */
  SDL_set_mouse_cursor /* set_cursor */
};

CONSTRUCTOR(static void init(void))
{
   register_video_client(&Video_SDL);
   register_keyboard_client(&Keyboard_SDL);
   register_mouse_client(&Mouse_SDL);
}

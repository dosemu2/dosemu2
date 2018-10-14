/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * Purpose: SDL video renderer for fb/kms console
 *
 * Authors: Emmanuel Jeandel, Bart Oldeman, Stas Sergeev
 */

#define SDL_C

#include <stdio.h>
#include <stdlib.h> /* for malloc & free */
#include <string.h> /* for memset */
#include <unistd.h>
#include <errno.h>
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
//#if SDL_VERSION_ATLEAST(1,2,10) && !defined(SDL_VIDEO_DRIVER_X11)
#undef X_SUPPORT
//#endif
#ifdef X_SUPPORT
#include "../X/screen.h"
#include "../X/X.h"
#endif
#include "vgaemu.h"
#include "vgatext.h"
#include "render.h"
#include "sdl.h"
#include "keyboard/keyb_clients.h"
#include "dos2linux.h"
#include "utilities.h"

static int SDL_priv_init(void);
static int SDL_init(void);
static void SDL_close(void);
static int SDL_set_videomode(struct vid_mode_params vmp);
static int SDL_update_screen(void);
static void SDL_put_image(int x, int y, unsigned width, unsigned height);
static void SDL_change_mode(int x_res, int y_res);

static void SDL_resize_image(unsigned width, unsigned height);
static void SDL_handle_events(void);
static int SDL_set_text_mode(int tw, int th, int w ,int h);

/* interface to xmode.exe */
static int SDL_change_config(unsigned, void *);
static void toggle_grab(void);
static struct bitmap_desc lock_surface(void);
static void unlock_surface(void);

struct video_system Video_SDL =
{
  SDL_priv_init,
  SDL_init,
  NULL,
  NULL,
  SDL_close,
  SDL_set_videomode,
  SDL_update_screen,
  SDL_change_config,
  SDL_handle_events,
  "sdl1"
};

struct render_system Render_SDL =
{
  .refresh_rect = SDL_put_image,
  .lock = lock_surface,
  .unlock = unlock_surface,
  .name = "sdl1",
};

static const SDL_VideoInfo *video_info;
static int remap_src_modes = 0;
static SDL_Surface* surface = NULL;

static Boolean is_mapped = FALSE;
static int exposure;

static int font_width, font_height;

static int w_x_res, w_y_res;			/* actual window size */
static int saved_w_x_res, saved_w_y_res;	/* saved normal window size */
static int initialized;
static int use_bitmap_font;
int no_mouse;

/* For graphics mode */
static ColorSpaceDesc SDL_csd;

static struct {
  int num, max;
  SDL_Rect *rects;
} sdl_rects;
pthread_mutex_t rect_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mode_mtx = PTHREAD_MUTEX_INITIALIZER;

static int force_grab = 0;
int grab_active = 0;
static int init_failed;

#ifdef X_SUPPORT
#ifdef USE_DL_PLUGINS
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
#ifdef CONFIG_SDL_SELECTION
  X_handle_selection = dlsym(handle, "X_handle_selection");
#endif
  X_pre_init();
#endif
}

static void init_x11_support(void)
{
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);
  if (SDL_GetWMInfo(&info) && info.subsystem == SDL_SYSWM_X11) {
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
    x11.display = info.info.x11.display;
    x11.lock_func = info.info.x11.lock_func;
    x11.unlock_func = info.info.x11.unlock_func;
  }
}

static void init_x11_window_font(int *x_res, int *y_res)
{
  int font_width, font_height, ret;
  /* called as soon as the window is active (first set video mode) */
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);
  if (SDL_GetWMInfo(&info) && info.subsystem == SDL_SYSWM_X11) {
    x11.window = info.info.x11.window;
    x11.lock_func();
    ret = X_load_text_font(x11.display, 1, x11.window, config.X_font,
		       &font_width, &font_height);
    x11.unlock_func();
    use_bitmap_font = !ret;
    *x_res = vga.text_width * font_width;
    *y_res = vga.text_height * font_height;
  }
}
#endif /* X_SUPPORT */

static int do_sdl_init(void)
{
  int ret;
  /* The privs are needed for opening /dev/input/mice.
   * Unfortunately SDL does not support gpm.
   * Also, as a bonus, /dev/fb0 can be opened with privs. */
  PRIV_SAVE_AREA
  enter_priv_on();
  ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
  if (ret < 0) {
    /* try w/o mouse */
    no_mouse = 1;
    setenv("SDL_NOMOUSE", "1", 0);
    ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
    if (ret == 0) {
      error("SDL mouse init failed. Make sure you are a member of group "
          "\"input\"\n");
    }
  }
  leave_priv_setting();
  return ret;
}

int SDL_priv_init(void)
{
  int ret;
#ifdef X_SUPPORT
  preinit_x11_support();
#endif
  setenv("SDL_VIDEODRIVER", "directfb", 1);
  ret = do_sdl_init();
  if (ret < 0) {
    warn("SDL: no directfb driver available\n");
    setenv("SDL_VIDEODRIVER", "fbcon", 1);
    ret = do_sdl_init();
    if (ret < 0)
      warn("SDL: no fbcon driver available\n");
  }
  if (ret < 0) {
    error("SDL: %s\n", SDL_GetError());
    if (access("/dev/fb0", F_OK | R_OK | W_OK) == -1 && errno == EACCES)
      error("@make sure your user is a member of group \"video\"\n");
    config.exitearly = 1;
    init_failed = 1;
    return -1;
  }
  return 0;
}

int SDL_init(void)
{
  SDL_Event evt;
  char driver[128];
  int have_true_color, features;

  if (init_failed)
    return -1;

  SDL_EnableUNICODE(1);
  SDL_VideoDriverName(driver, 8);
  v_printf("SDL: Using driver: %s\n", driver);

  /* Collect some data */
  video_info = SDL_GetVideoInfo();
  if (video_info->wm_available)
    SDL_change_config(CHG_TITLE, NULL);
  else
    config.X_fullscreen = 1;
  if (config.X_fullscreen)
    toggle_grab();

  SDL_csd.bits = video_info->vfmt->BitsPerPixel;
  SDL_csd.r_mask = video_info->vfmt->Rmask;
  SDL_csd.g_mask = video_info->vfmt->Gmask;
  SDL_csd.b_mask = video_info->vfmt->Bmask;
  color_space_complete(&SDL_csd);

//  set_remap_debug_msg(stderr);
  have_true_color = (video_info->vfmt->palette == NULL);
  features = 0;
  use_bitmap_font = 1;
  if (config.X_lin_filt)
    features |= RFF_LIN_FILT;
  if (config.X_bilin_filt)
    features |= RFF_BILIN_FILT;
  if (use_bitmap_font)
    features |= RFF_BITMAP_FONT;
  remap_src_modes = remapper_init(have_true_color, 1, features, &SDL_csd);
  register_render_system(&Render_SDL);

#ifdef X_SUPPORT
  init_x11_support();
#endif

  /* SDL_APPACTIVE event does not occur when an application window is first
   * created.
   * So we push that event into the queue */

  evt.type = SDL_ACTIVEEVENT;
  evt.active.type = SDL_ACTIVEEVENT;
  evt.active.gain = 1;
  evt.active.state = SDL_APPACTIVE;
  SDL_PushEvent(&evt);

  /* Same for focus event */
  evt.type = SDL_ACTIVEEVENT;
  evt.active.type = SDL_ACTIVEEVENT;
  evt.active.gain = 1;
  evt.active.state = SDL_APPINPUTFOCUS;
  SDL_PushEvent(&evt);

  /* enable repeat here (in the keyboard code it's too early) */
  SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

  return 0;
}

void SDL_close(void)
{
  v_printf("SDL: closing\n");
  remapper_done();
  vga_emu_done();
#ifdef X_SUPPORT
  if (x11.display && x11.window != None)
    X_close_text_display();
#endif
  SDL_Quit();
}

static void SDL_update(void)
{
  pthread_mutex_lock(&rect_mtx);
  if (sdl_rects.num == 0) {
    pthread_mutex_unlock(&rect_mtx);
    return;
  }
  SDL_UpdateRects(surface, sdl_rects.num, sdl_rects.rects);
  sdl_rects.num = 0;
  pthread_mutex_unlock(&rect_mtx);
}

static struct bitmap_desc lock_surface(void)
{
  pthread_mutex_lock(&mode_mtx);
  SDL_LockSurface(surface);
  return BMP(surface->pixels, w_x_res, w_y_res, surface->pitch);
}

static void unlock_surface(void)
{
  SDL_UnlockSurface(surface);
  pthread_mutex_unlock(&mode_mtx);
}

/*
 * Sync prev_screen & screen_adr.
 */
static void SDL_reset_redraw_text_screen(void)
{
  if(!is_mapped) return;
  reset_redraw_text_screen();
}

static void SDL_redraw_text_screen(void)
{
  if (!is_mapped) return;
#ifdef X_SUPPORT
  if (x11.display && !use_bitmap_font) {
    redraw_text_screen();
    return;
  }
#endif
  if (surface==NULL) return;
  redraw_text_screen();
}

/* NOTE : Like X.c, the actual mode is taken via video_mode */
int SDL_set_videomode(struct vid_mode_params vmp)
{
  int mode = video_mode;

  v_printf("X: X_setmode: %svideo_mode 0x%x (%s), size %d x %d (%d x %d pixel)\n",
    vmp.mode_class != -1 ? "" : "re-init ",
    (int) mode, vmp.mode_class ? "GRAPH" : "TEXT",
    vmp.text_width, vmp.text_height, vmp.x_res, vmp.y_res
  );

  w_x_res = vmp.w_x_res;
  w_y_res = vmp.w_y_res;
  if(vmp.mode_class == TEXT) {
    if (use_bitmap_font) {
      SDL_set_text_mode(vmp.text_width, vmp.text_height, vmp.x_res, vmp.y_res);
    } else {
      SDL_set_text_mode(vmp.text_width, vmp.text_height,
			vmp.text_width * font_width,
			vmp.text_height * font_height);
    }
    if (!grab_active) SDL_ShowCursor(SDL_ENABLE);
    if (is_mapped) SDL_reset_redraw_text_screen();
  } else {
    pthread_mutex_lock(&mode_mtx);
    SDL_change_mode(vmp.w_x_res, vmp.w_y_res);
    pthread_mutex_unlock(&mode_mtx);
  }

  initialized = 1;

  return 1;
}

void SDL_resize_image(unsigned width, unsigned height)
{
  v_printf("SDL: resize_image %d x %d\n", width, height);
  w_x_res = width;
  w_y_res = height;
  pthread_mutex_lock(&mode_mtx);
  SDL_change_mode(w_x_res, w_y_res);
  pthread_mutex_unlock(&mode_mtx);
}

static void SDL_redraw_resize_image(unsigned width, unsigned height)
{
  SDL_resize_image(width, height);
  render_blit(0, 0, width, height);
}

int SDL_set_text_mode(int tw, int th, int w ,int h)
{
  SDL_resize_image(w, h);
  SDL_set_mouse_text_cursor();
  /* that's all */
  return 0;
}

static void SDL_change_mode(int x_res, int y_res)
{
  Uint32 flags = SDL_HWPALETTE | SDL_HWSURFACE | SDL_ASYNCBLIT;
  saved_w_x_res = x_res;
  saved_w_y_res = y_res;
  if (!use_bitmap_font && vga.mode_class == TEXT) {
    if (config.X_fullscreen)
      flags |= SDL_FULLSCREEN;
  } else if (config.X_fullscreen) {
    SDL_Rect **modes;
    int i;

    modes=SDL_ListModes(video_info->vfmt, SDL_FULLSCREEN|SDL_HWSURFACE);
    if (modes == (SDL_Rect **) 0) {
      modes=SDL_ListModes(video_info->vfmt, SDL_FULLSCREEN);
    }
    if (modes == (SDL_Rect **) 0) {
      error("SDL: no video modes available\n");
      leavedos(5);
    }
#ifdef X_SUPPORT
    init_x11_window_font(x_res, y_res);
#endif
    if (modes != (SDL_Rect **) -1) {
      unsigned mw = 0;
      v_printf("Available Modes\n");
      for (i=0; modes[i]; ++i)
        v_printf("  %d x %d\n", modes[i]->w, modes[i]->h);
      i = 0;
      if (modes[1]) do {
	unsigned mh = 0;
	int factor;
	mw++;
	for (i = 0; modes[i] && modes[i]->w >= mw*vga.width; i++);
	if (i > 0) i--;
	do {
	  mh++;
	  while (modes[i]->h < mh*vga.height && i > 0) i--;
	  if (modes[i]) {
	    factor = modes[i]->h / vga.height;
	    y_res = vga.height * factor;
	  }
	} while (modes[i]->h - y_res > y_res/2);
	/* while the border is too high */
	factor = modes[i]->w / vga.width;
	x_res = vga.width * factor;
      } while (modes[i]->w - x_res > x_res/2);
      /* while the border is too wide */
      v_printf("SDL: using fullscreen mode: x=%d, y=%d\n",
	       modes[i]->w, modes[i]->h);
      x_res = modes[i]->w;
      y_res = modes[i]->h;
    }
    flags |= SDL_FULLSCREEN;
  } else {
    flags |= SDL_RESIZABLE;
  }
  w_x_res = x_res;
  w_y_res = y_res;
  v_printf("SDL: using mode %d %d %d\n", x_res, y_res, SDL_csd.bits);
#ifdef X_SUPPORT
  if (!x11.display) /* SDL may crash otherwise.. */
#endif
    SDL_ShowCursor(SDL_ENABLE);
  surface = SDL_SetVideoMode(x_res, y_res, SDL_csd.bits, flags);
  if (!surface) {
    error("SDL_SetVideoMode(%i %i) failed: %s\n", x_res, y_res,
	SDL_GetError());
    error("@Please try command 'fbset %ix%i-75'\n", x_res, y_res);
    error("@and adjust your /etc/fb.modes according to its output.\n");
    leavedos(23);
    return;
  }
  SDL_ShowCursor(SDL_DISABLE);
  pthread_mutex_lock(&rect_mtx);
  /* forget about those rectangles */
  sdl_rects.num = 0;
  pthread_mutex_unlock(&rect_mtx);
}

int SDL_update_screen(void)
{
  if (init_failed)
    return 1;
  if (!is_mapped)
    return 0;
  if (render_is_updating())
    return 0;
  /* if render is idle we start async blit (as of SDL_SYNCBLIT) and
   * then start the renderer. It will wait till async blit to finish. */
  SDL_update();
  return 0;
}

/* this only pushes the rectangle on a stack; updating is done later */
static void SDL_put_image(int x, int y, unsigned width, unsigned height)
{
  SDL_Rect *rect;
  pthread_mutex_lock(&rect_mtx);
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
  sdl_rects.num++;
  pthread_mutex_unlock(&rect_mtx);
}

static void toggle_grab(void)
{
  if(grab_active ^= 1) {
    v_printf("SDL: grab activated\n");
    if (!config.X_fullscreen)
      SDL_WM_GrabInput(SDL_GRAB_ON);
    v_printf("SDL: mouse grab activated\n");
    SDL_ShowCursor(SDL_DISABLE);
    mouse_enable_native_cursor(1);
  }
  else {
    v_printf("SDL: grab released\n");
    if (!config.X_fullscreen)
      SDL_WM_GrabInput(SDL_GRAB_OFF);
    if(vga.mode_class == TEXT)
      SDL_ShowCursor(SDL_ENABLE);
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
    SDL_redraw_resize_image(w_x_res, w_y_res);
  } else {
    v_printf("SDL: entering windowed mode!\n");
    SDL_redraw_resize_image(saved_w_x_res, saved_w_y_res);
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
        char *sw, *si;
        const char *charset;
	size_t iconlen = strlen(config.X_icon_name) + 1;
	wchar_t iconw[iconlen];
	const SDL_version *version = SDL_Linked_Version();

	if (mbstowcs(iconw, config.X_icon_name, iconlen) == -1)
	  iconlen = 1;
	iconw[iconlen-1] = 0;
	charset = "iso8859-1";
	if (SDL_VERSIONNUM(version->major, version->minor, version->patch) >=
	    SDL_VERSIONNUM(1,2,10))
	  charset = "utf8";
	sw = unicode_string_to_charset(buf, charset);
	si = unicode_string_to_charset(iconw, charset);
	v_printf("SDL: SDL_change_config: win_name = %s\n", sw);
	SDL_WM_SetCaption(sw, si);
	free(sw);
	free(si);
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
      if (!x11.display || x11.window == None)
	break;
      x11.lock_func();
      X_load_text_font(x11.display, 1, x11.window, buf,
		       &font_width, &font_height);
      x11.unlock_func();
      if (use_bitmap_font) {
        if(vga.mode_class == TEXT)
	  SDL_set_text_mode(vga.text_width, vga.text_height,
			    vga.width, vga.height);
      } else {
        if (w_x_res != vga.text_width * font_width ||
            w_y_res != vga.text_height * font_height) {
	  if(vga.mode_class == TEXT)
	    SDL_set_text_mode(vga.text_width, vga.text_height,
			      vga.text_width * font_width,
			      vga.text_height * font_height);
        }
      }
      if (!grab_active) SDL_ShowCursor(SDL_ENABLE);
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
     case SDL_ACTIVEEVENT: {
       if (event.active.state  == SDL_APPACTIVE) {
	 if (event.active.gain == 1) {
	   v_printf("SDL: Expose Event\n");
	   is_mapped = TRUE;
	   if (vga.mode_class == TEXT) {
	     if (!exposure) {
	       SDL_redraw_text_screen();
	       exposure = 1;
	     }
	   } else {
	     /* TODO */
	   }
	 } else {
	   /* TODO */
	 }
       } else if (event.active.state == SDL_APPINPUTFOCUS) {
	 if (event.active.gain == 1) {
	   v_printf("SDL: focus in\n");
	   if (vga.mode_class == TEXT) text_gain_focus();
	   if (config.X_background_pause && !dosemu_user_froze) unfreeze_dosemu ();
	 } else {
	   v_printf("SDL: focus out\n");
	   if (vga.mode_class == TEXT) text_lose_focus();
	   if (config.X_background_pause && !dosemu_user_froze) freeze_dosemu ();
	 }
       } else if (event.active.state == SDL_APPMOUSEFOCUS) {
	 if (event.active.gain == 1) {
	   v_printf("SDL: mouse focus in\n");
	 } else {
	   v_printf("SDL: mouse focus out\n");
	 }
       } else {
	 v_printf("SDL: other activeevent\n");
       }
       break;
     }
     case SDL_VIDEORESIZE:
       v_printf("SDL: videoresize event\n");
       SDL_redraw_resize_image(event.resize.w, event.resize.h);
       break;
     case SDL_VIDEOEXPOSE:
       v_printf("SDL: videoexpose event\n");
#ifdef X_SUPPORT
       if (!use_bitmap_font) {
          x11.lock_func();
          X_handle_text_expose();
          x11.unlock_func();
          SDL_redraw_text_screen();
       }
#endif
       break;
     case SDL_KEYDOWN:
       {
	 SDL_keysym keysym = event.key.keysym;
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
       SDL_process_key(event.key);
       break;
     case SDL_KEYUP:
#if CONFIG_SDL_SELECTION
       clear_if_in_selection();
#endif
       SDL_process_key(event.key);
       break;
     case SDL_MOUSEBUTTONDOWN:
       {
	 int buttons = SDL_GetMouseState(NULL, NULL);
#if CONFIG_SDL_SELECTION
	 if (x11.display && vga.mode_class == TEXT && !grab_active) {
	   if (event.button.button == SDL_BUTTON_LEFT)
	     start_selection(x_to_col(event.button.x, w_x_res),
			     y_to_row(event.button.y, w_y_res));
	   else if (event.button.button == SDL_BUTTON_RIGHT)
	     start_extend_selection(x_to_col(event.button.x, w_x_res),
				    y_to_row(event.button.y, w_y_res));
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
	 extend_selection(x_to_col(event.button.x, w_x_res),
			  y_to_row(event.button.y, w_y_res));
#endif /* CONFIG_SDL_SELECTION */
       SDL_mouse_move(event.motion.xrel, event.motion.yrel, w_x_res, w_y_res);
       break;
     case SDL_QUIT:
       leavedos(0);
       break;
#if CONFIG_SDL_SELECTION
     case SDL_SYSWMEVENT:
       if (x11.display)
	 SDL_handle_selection(&event.syswm.msg->event.xevent);
       break;
#endif /* CONFIG_SDL_SELECTION */
     default:
       v_printf("PAS ENCORE TRAITE\n");
       /* TODO */
       break;
     }
   }
}

CONSTRUCTOR(static void init(void))
{
   register_video_client(&Video_SDL);
   register_keyboard_client(&Keyboard_SDL);
   register_mouse_client(&Mouse_SDL);
}

/* Drive the guest's own mouse cursor, which follows relative motion and not
   the X pointer position: park it in a corner, then walk it out by a counted
   number of 8-unit steps and click.
   usage: xrel :7 NX NY [CLICKS]   -- NX steps of (8,0), then NY steps of (0,8) */
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
extern int XTestFakeMotionEvent(Display *, int, int, int, unsigned long);
extern int XTestFakeRelativeMotionEvent(Display *, int, int, unsigned long);
extern int XTestFakeButtonEvent(Display *, unsigned int, int, unsigned long);

static Display *d;

static void rel(int dx, int dy)
{
  XTestFakeRelativeMotionEvent(d, dx, dy, 0);
  XFlush(d);
  usleep(9000);
}

int main(int argc, char **argv)
{
  Window root, parent, *kids = NULL, win = 0;
  unsigned nkids = 0, i;
  int k, nx, ny, clicks, wx = 0, wy = 0;
  if (argc < 4) { fprintf(stderr, "usage: xrel :N NX NY [CLICKS]\n"); return 2; }
  d = XOpenDisplay(argv[1]);
  if (!d) { fprintf(stderr, "no display %s\n", argv[1]); return 1; }
  nx = atoi(argv[2]); ny = atoi(argv[3]);
  clicks = argc > 4 ? atoi(argv[4]) : 3;
  root = DefaultRootWindow(d);
  if (XQueryTree(d, root, &root, &parent, &kids, &nkids) && nkids) {
    for (i = 0; i < nkids; i++) {
      XWindowAttributes wa;
      if (XGetWindowAttributes(d, kids[i], &wa) && wa.map_state == IsViewable) {
        Window child;
        win = kids[i];
        XTranslateCoordinates(d, win, root, 0, 0, &wx, &wy, &child);
        XSetInputFocus(d, win, RevertToParent, CurrentTime);
        break;
      }
    }
  }
  /* keep the X pointer inside the window so dosemu keeps taking the motion */
  XTestFakeMotionEvent(d, -1, wx + 160, wy + 100, 0);
  XFlush(d);
  usleep(50000);
  for (k = 0; k < 40; k++)
    rel(-30, -30);               /* top left corner, where the guest clamps */
  for (k = 0; k < nx; k++)
    rel(8, 0);
  for (k = 0; k < ny; k++)
    rel(0, 8);
  usleep(600000);
  for (k = 0; k < clicks; k++) {
    XTestFakeButtonEvent(d, 1, True, 0);
    XFlush(d);
    usleep(40000);
    XTestFakeButtonEvent(d, 1, False, 0);
    XFlush(d);
    usleep(600000);
  }
  fprintf(stderr, "xrel nx=%d ny=%d clicks=%d\n", nx, ny, clicks);
  XCloseDisplay(d);
  return 0;
}

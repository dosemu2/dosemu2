/* Press a spot the way the game actually notices: sweep the pointer in from
   the corner in small steps, then three separate clicks.  A jump straight to
   the target plus a click is missed about half the time, and a long press is
   never counted as a click.
   usage: xsweep :7 X Y [X Y ...]   -- coordinates are inside the dosemu window */
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
  usleep(8000);
}

static void abso(int x, int y)
{
  XTestFakeMotionEvent(d, -1, x, y, 0);
  XFlush(d);
  usleep(30000);
}

int main(int argc, char **argv)
{
  Window root, parent, *kids = NULL, win = 0;
  unsigned nkids = 0, i;
  int a, wx = 0, wy = 0, k;
  d = XOpenDisplay(argv[1]);
  if (!d) { fprintf(stderr, "no display %s\n", argv[1]); return 1; }
  root = DefaultRootWindow(d);
  if (XQueryTree(d, root, &root, &parent, &kids, &nkids) && nkids) {
    for (i = 0; i < nkids; i++) {
      XWindowAttributes wa;
      if (XGetWindowAttributes(d, kids[i], &wa) && wa.map_state == IsViewable) {
        Window child;
        win = kids[i];
        XTranslateCoordinates(d, win, root, 0, 0, &wx, &wy, &child);
        XSetInputFocus(d, win, RevertToParent, CurrentTime);
        fprintf(stderr, "window 0x%lx at %d,%d size %dx%d\n",
                win, wx, wy, wa.width, wa.height);
        break;
      }
    }
  }
  XFlush(d);
  for (a = 2; a + 1 < argc; a += 2) {
    int tx = atoi(argv[a]), ty = atoi(argv[a + 1]);
    int steps = (tx > ty ? tx : ty) / 8 + 1;
    abso(wx + 5, wy + 5);
    for (k = 0; k < 30; k++)
      rel(-30, -30);                 /* park it in the corner for real */
    for (k = 0; k < steps; k++)
      rel(8, 8);                     /* walk back in so the game sees motion */
    abso(wx + tx, wy + ty);
    usleep(700000);
    for (k = 0; k < 3; k++) {
      XTestFakeButtonEvent(d, 1, True, 0);
      XFlush(d);
      usleep(40000);
      XTestFakeButtonEvent(d, 1, False, 0);
      XFlush(d);
      usleep(700000);
    }
    fprintf(stderr, "swept and clicked %d,%d (%d steps)\n", tx, ty, steps);
  }
  XCloseDisplay(d);
  return 0;
}

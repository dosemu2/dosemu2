/* nudge the pointer by a total delta in 8-unit relative steps: xnud :9 DX DY */
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
extern int XTestFakeRelativeMotionEvent(Display *, int, int, unsigned long);
int main(int argc, char **argv)
{
  Display *d = XOpenDisplay(argv[1]);
  int dx, dy, n, i, sx, sy;
  if (!d || argc < 4) { fprintf(stderr, "usage: xnud :N DX DY\n"); return 1; }
  dx = atoi(argv[2]); dy = atoi(argv[3]);
  n = (abs(dx) > abs(dy) ? abs(dx) : abs(dy) + 7) / 8 + 1;
  sx = dx / n; sy = dy / n;
  for (i = 0; i < n; i++) {
    XTestFakeRelativeMotionEvent(d, sx, sy, 0);
    XFlush(d);
    usleep(9000);
  }
  fprintf(stderr, "nudged %d,%d in %d steps of %d,%d\n", dx, dy, n, sx, sy);
  XCloseDisplay(d);
  return 0;
}

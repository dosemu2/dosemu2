
Some basic information about dosemu's X support.
Sometimes it's even sort of useable now.

What you should take care of:

-- Do a 'xmodmap -e "keycode 22 = 0xff08"' to get use of your backspace key.
-- Do a 'xmodmap -e "keycode 107 = 0xffff"' to get use of your delete key.

-- Make sure dosemu has X support compiled in. 
   (X_SUPPORT = 1 in the Makefile)

-- you need to have the vga font installed. See README.ncurses.

-- start dosemu with 'dos -X' from an X terminal window. 
   Or make a link to 'dos' named 'xdos' - when called by that name,
   dosemu will automatically assume -X. 
   There is also a new debug flag 'X' for X-related messages.
   To exit xdos, use 'exitemu' or select 'Close' aka 'Delete' 
   (better not 'Destroy') from the 'Window' menu.

-- there are some X-related configuration options for dosemu.conf.
   See examples/config.dist for details.

-- starting xdos in the background (like from a window manager menu)
   appears not to work for some reason.

-- Keyboard support in the dosemu window isn't perfect yet. It probably
   could be faster, some key combos still don't work (e.g. Ctrl-Fn), etc.
   However, input through the terminal window (i.e. the window you started
   dosemu from) is still supported. If you have problems, you *might* be
   better off putting your focus in there.

-- Keyboard support of course depends on your X keyboard mappings (xmodmap).
   If certain keys don't work (like Pause, Backspace,...), it *might* be 
   because you haven't defined them in your xmodmap, or defined to something
   other than dosemu expects.

-- using the provided icon (dosemu.xpm):
   * you need the xpm (pixmaps) package. If you're not sure, look for
     a file like /lib/libXpm.so.*
   * you also need a window manager which supports pixmaps. Fvwm is fine,
     but I can't tell you about others. Twm probaby won't do.
   * copy dosemu.xpm to where you usually keep your pixmap (not bitmap!) 
     files (perhaps /usr/include/X11/pixmaps)
   * tell your window manager to use it. For fvwm, add the following
     line to your fvwmrc file:
     
     Icon "xdos"   dosemu.xpm

     This assumes you have defined a PixmapPath. Otherwise, specify the
     entire pathname.
   * note that if you set a different icon name than "xdos" in your
     dosemu.conf, you will also have to change the fvwmrc entry.
   * restart the window manager. There's usually an option in the
     root menu to do so.
   
   Now you should see the icon whenever you iconify xdos.

   Note that the xdos program itself does not include the icon - that's
   why you have to tell the window manager. I chose to do it this way
   to avoid xdos *requiring* the Xpm library.

-- If anything else does not work as expected, don't panic :-)
   Remember the thing is still under construction.
   However, if you think it's a real bug, please tell me.

important changes to previous version (pre53_17):

- fixed focus handling at startup
- support for 21/28/43/50 line modes!
  (43+50 look a bit funny, though... I use the 8x16 font for all modes)
- .xpm icon
- fixed startup error handling (won't hang now if display not found)
- limited window size

Rainer Zimmermann
(zimmerm@mathematik.uni-marburg.de)

--------------------------------------------------------------------
Status of X support:   (Sept 5, 1994)


Done
----

X_update_screen    (video output)
implement cursor
fix cursor/scrolling bugs
fix Scroll (video/terminal.c) (?)
fix banner message (initialization) (works after video cleanup, dunno why :)
check video memory dirty bit
X event handling   (close, expose, focus etc.)
fixed cursor initialization
cleaned up cursor handling
added 'xdos' calling method
disable 'mouse' serial ports in X mode
write direct scroll routine (not used yet, though)
care about int10 calls  -ok?
Handle close ("delete") window event - (copied from xloadimage)
X keyboard support (pcemu code, heavily modified)
Mouse support
X configuration (display, updatefreq, updatelines,... what else?)
int10 video mode switches (resize window)
Window SizeHints  (fixed size or max size?)
create icon :-)

ToDo (in no special order)
--------------------------

xor cursor? blinking cursor?
use mark's scroll detector
jump scroll?
fine-tune X_update_screen
graphics support?
allow non-standard font heights via bios
cut & paste

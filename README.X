
Here's the first version of the dosemu X support.
It's very preliminary, but looks somewhat nice already so I thought
someone could like to try ;)

What you should take care of:

-- uncomment the "X_SUPPORT = 1" line in the makefile and rebuild
   dosemu.
   this affects only emu.c, dyndeb.c and video/terminal.c, so you
   don't have to recompile all if you just delete the related .o files.

-- you need to have the vga font installed. See README.ncurses.

-- start dosemu with 'dos -X' from an X terminal window. 
   Or make a link to 'dos' named 'xdos' - when called by that name,
   dosemu will automatically assume -X. 
   There is also a new debug flag 'X' for X-related messages.

-- Keyboard input yet runs through the terminal routines, so your
   input focus must be in the window you started dosemu from, not
   in the dosemu window! 
   This of course also means that only the key combinations 
   supported in terminal mode will work. (this will hopefully 
   be changed soon.)

-- video modes other than 80x25 color don't work correctly.

-- dosemu currently doesn't honor the DISPLAY variable, nor any
   standard command line options like -geometry etc.
   DISPLAY is hardcoded to ":0", if you need another setting, you
   have to change video/X.c.

-- If anything else does not work as expected, don't panic :-)
   Remember the thing is still under construction.
   However, if you think it's a real bug, please tell me.

important changes to previous version (pre53_9):

- serial mouse ports are now REALLY automatically disabled
  (removed my silly changes to the config parser)
- added 'xdos' calling method
- attempt to implement keyboard
- implemented mouse support (X part works, but the internal mouse
  driver is VERY buggy)
- handle delete window message

Rainer Zimmermann
(zimmerm@mathematik.uni-marburg.de)

--------------------------------------------------------------------
Status of X support:   (Aug 3, 1994)


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
X keyboard support (use xdos code? pcemu code?)
Mouse support

ToDo (in no special order)
--------------------------

xor cursor? blinking cursor?
use mark's scroll detector
jump scroll?
int10 video mode switches (resize window)
Window SizeHints - fixed size or max size?
fine-tune X_update_screen
X configuration (display, updatefreq, updatelines,... what else?)
graphics support?
create icon :-)

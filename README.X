
Here's the first version of the dosemu X support.
It's very preliminary, but looks somewhat nice already so I thought
someone could like to try ;)

What you should take care of:

-- Make sure dosemu has X support compiled in. 
   (X_SUPPORT = 1 in the Makefile)

-- you need to have the vga font installed. See README.ncurses.

-- start dosemu with 'dos -X' from an X terminal window. 
   Or make a link to 'dos' named 'xdos' - when called by that name,
   dosemu will automatically assume -X. 
   There is also a new debug flag 'X' for X-related messages.

-- Keyboard support in the dosemu window isn't perfect yet. It's somewhat
   slow, Alt-xx keys don't work, etc...
   However, input through the terminal window (i.e. the window you started
   dosemu from) is still supported. If you have problems, better put
   your focus in there.

-- Keyboard support of course depends on your X keyboard mappings (xmodmap).
   If certain keys don't work (like Pause, Backspace,...), it *might* be 
   because you haven't defined them in your xmodmap, or defined to something
   other than dosemu expects.

-- video modes other than 80x25 color don't work correctly.

-- If anything else does not work as expected, don't panic :-)
   Remember the thing is still under construction.
   However, if you think it's a real bug, please tell me.

important changes to previous version (pre53_13):

- fixed mouse bug which blanked screen in dosshell
- latin1 national character support for keyboard (please let 
  me know if anything is wrong here!)
- minor keyboard improvements (Backspace, Fn keys)

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
X configuration (display, updatefreq, updatelines,... what else?)

ToDo (in no special order)
--------------------------

xor cursor? blinking cursor?
use mark's scroll detector
jump scroll?
int10 video mode switches (resize window)
Window SizeHints - fixed size or max size?
fine-tune X_update_screen
graphics support?
create icon :-)


ARRRGGHH! I actually managed to kill my only copy of the main
X source file... and of course (Murphy...), no way to undelete it.
Had to hack it all up again. But here it is:

Here's the first version of the dosemu X support.
It's very preliminary, but looks somewhat nice already so I thought
someone could like to try ;)

What you should take care of:

-- uncomment the "X_SUPPORT = 1" line in the makefile and rebuild
   dosemu.
   this affects only emu.c, dyndeb.c and video/terminal.c, so you
   don't have to recompile all if you just delete the related .o files.

-- you need to have the vga font installed. See README.ncurses.

-- comment out all mouse-related statements from your dosemu.conf.
   Otherwise dosemu might grab the mouse under X... not good.

-- start dosemu with 'dos -X' from an X terminal window. There is
   also a new debug flag 'X' for X-related messages.

-- Keyboard input yet runs through the terminal routines, so your
   input focus must be in the window you started dosemu from, not
   in the dosemu window! 
   This of course also means that only the key combinations supported
   in terminal mode will work. (this will hopefully be changed soon.)
   I also had the problem that RETURN gave 0xA not 0xD, which
   confused many programs. I don't know if this is my or the terminal
   routines' fault.

-- video modes other than 80x25 color don't work correctly.

-- dosemu currently doesn't honor the DISPLAY variable, nor any
   standard command line options like -geometry etc.
   DISPLAY is hardcoded to ":0", if you need another setting, you
   have to change video/X.c.

-- If anything else does not work as expected, don't panic :-)
   Remember the thing is still under construction.
   However, if you think it's a real bug, please tell me.

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


ToDo
----

xor cursor? blinking cursor?
fix banner message (initialization)
scroll X window directly (possible? - yes: XCopyArea, XClearArea)
jump scroll?
care about int10 calls
check video memory dirty bit
int10 video mode switches (resize window)
X event handling   (close, expose, focus etc.)
X keyboard support (use xdos code?)
Window SizeHints
Mouse support (use xdos code?)
fine-tune X_update_screen
X configuration (display,... what else?)
graphics support?

Please read all of this for a more complete X information ;-)

Latest Info from Uwe Bonnes <bon@elektron.ikp.physik.th-darmstadt.de>:

xdos from dosemu version pre-0.60.4.2 with the patch from my last messages
should now be more capable.
In particular it should now understand the keys from the keypad-area (the
keys the most right on a MF-Keyboard) and numlock and keyevents in the range
of the latin characters, even when you run xdos from a remote X-terminal. 

If it dosen't work for you as expected, please check out the following:
- Don't specify "keycode" for X-support.
The keycode-option is something specific for a special X-server, here
XFree86, and so isn't portable and I disencourage its use.
- Check out whether your key sends a sensible ksym,with e.g."xev". If it sends
something sensible which you think should be mapped by xdos, please let me
know details.
- If you are running xdos on XFree86, version 3.1.1 and newer and the keys
of the keypad-area don't work, you have two options:
-- Comment out the line "    ServerNumLock " in /usr/X11R6/lib/X11/XFConfig
-- Feed the following file to xmodmap. From what I understand from the docs,
XFree-3.1.1 should do that intrinsicly, but for me it didn't. This is a part
of the file /usr/X11R6/lib/X11/etc/xmodmap.std 


! When using ServerNumLock in your XF86Config, the following codes/symbols
! are available in place of 79-81, 83-85, 87-91
keycode  136 = KP_7
keycode  137 = KP_8
keycode  138 = KP_9
keycode  139 = KP_4
keycode  140 = KP_5
keycode  141 = KP_6
keycode  142 = KP_1
keycode  143 = KP_2
keycode  144 = KP_3
keycode  145 = KP_0
keycode  146 = KP_Decimal
keycode  147 = Home
keycode  148 = Up
keycode  149 = Prior
keycode  150 = Left
keycode  151 = Begin
keycode  152 = Right
keycode  153 = End
keycode  154 = Down
keycode  155 = Next
keycode  156 = Insert
keycode  157 = Delete

-- 
Uwe Bonnes                bon@elektron.ikp.physik.th-darmstadt.de

-----------------------------------------------------------------------
Some basic information about dosemu's X support.
Sometimes it's even sort of useable now.

What you should take care of:

-- Do a 'xmodmap -e "keycode 22 = 0xff08"' to get use of your backspace key.
-- Do a 'xmodmap -e "keycode 107 = 0xffff"' to get use of your delete key.

-- Make sure dosemu has X support compiled in. 
   (X_SUPPORT = 1 in the Makefile)

-- you should have the vga font installed. See README.ncurses.

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

------------------------------------------------------------------------------

November 13, 1995
Erik Mouw (J.A.K.Mouw@et.tudelft.nl)
Arjan Filius (I.A.Filius@et.tudelft.nl)

We've made some major changes in X.c that enables X to run graphics modes. 
Unfortunately, this disables the cut-and-paste support, but we think the
graphics stuff is much more fun (after things have established, we'll put
the cut-and-paste stuff back). The graphics is done through vgaemu, the VGA
emulator. Status of the work:

vgaemu:
- Video memory. 1 Mb is allocated. It is mapped with mmap() in the VGA 
  memory region of dosemu (0xa00000-0xbfffff) to support bank switching.
  This is very i386-Linux specific, don't be surprised if it doesn't work
  under NetBSD or another Linux flavour (Alpha/Sparc/MIPS/etc).
- The DAC (Digital to Analog Converter). The DAC is completely emulated,
  except for the pelmask. This is not difficult to implement, but it is
  terribly slow because a change in the pelmask requires a complete redraw
  of the screen. Fortunately, the pelmask changes aren't used often so
  nobody will notice ;-)
- The attribute controller is partially emulated. (Actually, only reads and
  writes to the ports are emulated)
- The working modes are 0x13 (320x200x256) and some other 256 color modes.
- To do (in no particular order): font support in graphics modes (8x8, 8x16,
  9x16, etc), text mode support, 16, 4 and 2 color support, better bank
  switching, write the X code out of vgaemu to get it more generic.

vesa:
- VESA set/get mode, get information and bankswitch functions work.
- All VESA 256 color (640x480, 800x600, 1024x768) modes work, but due to bad
  bank switch code in vgaemu they won't display right.
- A VESA compatible video BIOS is mapped at 0xc00000. It's very small, but
  in future it's a good place to store the BIOS fonts (8x8, 8x16) in.
- To do: implement the other VESA functions.

X:
- Added own colormap support for the 256 color modes.
- Support for vgaemu.
- Some cleanups.
- To do: remove text support and let vgaemu do the text modes, put back the 
  cut-and-paste stuff, more cleanups.
- NOTE: we've developed on X servers with 8 bit pixel depths (XF86_SVGA) so 
  we don't know how our code behaves on other pixel depths. We don't even
  know if it works.

As stated before, this code was written for Linux (tested with 1.2.13 and
1.3.39) and we don't know if it works under NetBSD. The mmap() of
/proc/self/mem and mprotect() magic in vgaemu are very (i386) Linux
specific.


Erik

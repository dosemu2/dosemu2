# This script is made by Mark D. Rejhon to automatically bring up DOSEMU
# in an xterm (or one of its cousins: color_xterm, ansi_xterm, or rxvt).
# It automatically detects the IBM VGA font, and the best xterm to run,
# and then runs the xterm with the proper parameters required to run DOSEMU!
#
# Any improvements or comments, please Email to Mark Rejhon at this address:
# ag115@freenet.carleton.ca

# The following detects the existence of the VGA font.
if ! [ -r $X11ROOTDIR/lib/X11/fonts/misc/vga.pcf* ]; then
	echo "Hey...I could not find the IBM VGA font for xwindows!"
	echo "To install the VGA font, go into the DOSEMU source directory and type:"
	echo "sh xinstallvgafont"
	echo ""
	echo "Starting xdosemu with default font.  If not using the VGA font, you should use"
	echo "the latin character set instead of the PC character set in /etc/dosemu.conf."
	echo ""

	#Search for various xterm-related clients and choose the best or
	#fastest one to run.
	if [ -x $X11ROOTDIR/bin/ansi_xterm ]; then
		echo "Found ansi_xterm.  (Hopefully it's at least version 2.0!)"
		echo "Starting ansi_xterm 2.0 DOSEMU without IBM font..."
		ansi_xterm -geometry 80x25 -bg black -fg gray -T DOSEMU -n DOSEMU -e dos $*
	elif [ -x $X11ROOTDIR/bin/rxvt ]; then
		echo "Found rxvt."
		echo "Starting regular rxvt DOSEMU without IBM font..."
		rxvt -geometry 80x25 -bg black -fg gray -T DOSEMU -n DOSEMU -e dos $*
	elif [ -x $X11ROOTDIR/bin/xterm ]; then
		echo "Found xterm."
		echo "Starting regular xterm DOSEMU without IBM font..."
		xterm -geometry 80x25 -bg black -fg gray -T DOSEMU -n DOSEMU -e dos $*
	else
		echo "ERROR: No xterm programs found in $X11ROOTDIR/bin"
        	echo "Perhaps you don't have Xwindows installed that way?" 
	        echo ""
	fi
else
	# The IBM font was found, now find the best xterm to start.
	if [ -x $X11ROOTDIR/bin/ansi_xterm ]; then
		echo "Found ansi_xterm.  (Hopefully it's at least version 2.0!)"
		echo "Congratulations for choosing the best xterm for DOSEMU."
		echo "Starting ansi_xterm DOSEMU with IBM font..."
		ansi_xterm -font vga -fb vga -geometry 80x25 -bg black -fg gray -T DOSEMU -n DOSEMU -e dos $*
	elif [ -x $X11ROOTDIR/bin/rxvt ]; then
		echo "Found rxvt."
		echo "Starting regular rxvt DOSEMU with IBM font..."
		rxvt -font vga -fb vga -geometry 80x25 -bg black -fg gray -T DOSEMU -n DOSEMU -e dos $*
	elif [ -x $X11ROOTDIR/bin/xterm ]; then
		echo "Found xterm."
		echo "Starting regular xterm DOSEMU with IBM font..."
		xterm -font vga -fb vga -geometry 80x25 -bg black -fg gray -T DOSEMU -n DOSEMU -e dos $*
	else
		echo "ERROR: No xterm programs found in $X11ROOTDIR/bin"
        	echo "Perhaps you don't have Xwindows installed on your system?" 
	        echo ""
	fi
fi

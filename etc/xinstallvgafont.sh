# This script installs the Xwindows VGA font. This should be run as root 
# if the Xwindows directories are writeable only by root.
#
# Please mail comments or improvements to Mark Rejhon at this Email address:
# ag115@freenet.carleton.ca

# Test for existence of X11
if ! [ -e $X11ROOTDIR/lib/X11 ]; then
	echo "This system doesn't have a $X11ROOTDIR/lib/X11 directory!"
	echo "Perhaps you don't have Xwindows installed?"

# Test for existence of vga.pcf in current directory
elif ! [ -r ./vga.pcf ]; then
	echo "Where's the VGA font file, 'vga.pcf'?  It's not in the current directory!"
	echo "Please run this script in the DOSEMU source directory."

# Test the existence of $X11ROOTDIR/lib/X11/fonts/misc
elif ! [ -e $X11ROOTDIR/lib/X11/fonts/misc ]; then
	echo "There is no $X11ROOTDIR/lib/X11/fonts/misc directory to install the VGA font in!"

# Test for writeability of $X11ROOTDIR/lib/X11/fonts/misc
elif ! [ -w $X11ROOTDIR/lib/X11/fonts/misc ]; then
	echo "The $X11ROOTDIR/lib/X11/fonts/misc directory is not writeable by you!"
	echo "This script should be executed as root, to install the VGA font file."

# Else, go ahead and try to install the font.
else
	echo "Installing VGA font for Xwindows..."
	install -m 644 vga.pcf $X11ROOTDIR/lib/X11/fonts/misc/
	cd $X11ROOTDIR/lib/X11/fonts/misc
	echo "Recompiling font table with 'mkfontdir'..."
	mkfontdir
	if ! [ $? -eq 0 ]; then
		echo "ERROR: Could not execute 'mkfontdir'.  VGA font not installed."
	else
		echo ""
		echo "If you are in X now, you need to reset or restart X windows to recognize"
		echo "the newly added font file."
	fi
fi

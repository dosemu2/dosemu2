#!/bin/sh

srcdir=`pwd`
echo "Generating toplevel configure script..."
if ! autoreconf --install --force --warnings=all ; then
	echo "Failure!"
	exit 1
fi
automake -a 2>/dev/null
if [ ! -f config.sub -o ! -f config.guess ]; then
	echo "automake failure!"
	exit 1
fi
./mkpluginhooks
for dir in `cat plugin_configure`; do
	echo "Generating configure script for $dir..."
	cd "${srcdir}/${dir}"
	if ! autoreconf --install --force --warnings=all ; then
		rm -f ./configure
		echo "Warning: failure generating configure for $dir"
	fi
done
cd $srcdir
echo
echo "Done, now run ./default-configure"

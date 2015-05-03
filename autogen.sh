#!/bin/sh

srcdir=`pwd`
echo "Generating toplevel configure script..."
if ! autoreconf --install --force --warnings=all ; then
	echo "Failure!"
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

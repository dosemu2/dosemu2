#!/bin/sh

srcdir=`pwd`
echo "Regenerating toplevel configure script..."
if ! autoreconf --install --force --warnings=all ; then
	echo "Failure!"
	exit 1
fi
./mkpluginhooks
echo "Regenerating plugin configure scripts..."
for dir in `cat plugin_configure`; do
	echo "Regenerating configure script for $dir..."
	cd "${srcdir}/${dir}"
	if ! autoreconf --install --force --warnings=all ; then
		rm -f ./configure
		echo "Failed generating configure for $dir"
	fi
done
cd $srcdir

#!/bin/sh

echo "Generating toplevel configure script..."
rm -f aclocal.m4
if ! autoreconf -I m4 --install --force ; then
	echo "Failure!"
	exit 1
fi
echo
echo "Done, now run $(dirname $0)/default-configure"

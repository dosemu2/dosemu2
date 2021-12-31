#!/bin/sh

DIR=$(dirname $0 | xargs realpath)
echo "Generating toplevel configure script in $DIR..."
rm -f aclocal.m4
if ! autoreconf -I m4 --install --force $DIR; then
	echo "Failure!"
	exit 1
fi
echo
echo "Done, now run $DIR/default-configure"

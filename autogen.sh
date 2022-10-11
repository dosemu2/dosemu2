#!/bin/sh

DIR=$(dirname $0 | xargs realpath)
echo "Generating toplevel configure script in $DIR..."

check_scr() {
  [ -f $DIR/$1 ] || cp $DIR/scripts/$1 $DIR/$1
}

rm -f aclocal.m4
if ! autoreconf -v -I m4 --install --force $DIR; then
	echo "Failure!"
	exit 1
fi
check_scr config.sub
check_scr config.guess
check_scr install-sh
echo
echo "Done, now run $DIR/default-configure"

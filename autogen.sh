#!/bin/sh

REQ_FILES="configure.ac getversion VERSION m4 install-sh Makefile autogen.sh \
git-rev.sh"

if [ -n "$1" -a ! -f configure.ac ]; then
    for i in $REQ_FILES; do
	ln -sf "$1"/$i $i
    done
    mkdir -p src/include
    if [ -f "$1"/configure ]; then
	echo "cp -fp "$1"/configure configure"
	cp -fp "$1"/configure configure
	exit $?
    fi
fi
echo "Generating toplevel configure script..."
rm -f aclocal.m4
if ! autoreconf -I m4 --install --force ; then
	echo "Failure!"
	exit 1
fi
echo
echo "Done, now run $(dirname $0)/default-configure"

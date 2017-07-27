#!/bin/sh

cd $(dirname $0)
srcdir=`pwd`
echo "Generating toplevel configure script..."
rm -f aclocal.m4
if ! autoreconf -I m4 --install --force ; then
	echo "Failure!"
	exit 1
fi
./mkpluginhooks
for dir in `cat plugin_configure`; do
	echo "Generating configure script for $dir..."
	cd "${srcdir}/${dir}"
	if [ -d m4 ]; then
		AC_F="-I m4 -I ${srcdir}/m4"
	else
		AC_F="-I ${srcdir}/m4"
	fi
	if ! autoreconf --install --force $AC_F ; then
		rm -f ./configure
		echo "Warning: failure generating configure for $dir"
	fi
done
cd $srcdir
echo
echo "Done, now run $(dirname $0)/default-configure"

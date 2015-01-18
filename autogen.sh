#!/bin/sh
set -e
srcdir=`realpath "$(dirname $0)"`
cd $srcdir
autoreconf --install --force --warnings=all
./default-configure
# plugin_configure is created by top-level configure
echo "Regenerating plugin configure scripts..."
for dir in `cat plugin_configure`; do
	cd "${srcdir}/${dir}"
	autoreconf --install --force --warnings=all
	./configure
done

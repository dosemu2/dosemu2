#!/bin/sh
set -e
srcdir=`realpath "$(dirname $0)"`
for dir in . src/plugin/kbd_unicode src/plugin/sdl; do
	cd "${srcdir}/${dir}"
	autoreconf --install --force --warnings=all
done
cd $srcdir
sh ./default-configure

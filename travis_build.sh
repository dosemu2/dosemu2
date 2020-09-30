#!/bin/sh

set -e

LOCALFDPP="localfdpp.git"
LOCALFDPPINST="$(pwd)/localfdpp"
FDPPBRANCH=""

test -d ${LOCALFDPP} && exit 1

git clone --depth 1 --no-single-branch https://github.com/stsp/fdpp.git ${LOCALFDPP}
(
  cd ${LOCALFDPP} || exit 2
  [ -z "$FDPPBRANCH" ] || git checkout "$FDPPBRANCH"
  git tag tmp -m "make git-describe happy"

  echo "DEBUG_MODE = 1"  >  local.mak
  echo "EXTRA_DEBUG = 1" >> local.mak
  echo "USE_UBSAN = 1" >> local.mak

  make clean all install PREFIX=${LOCALFDPPINST}
)

export PKG_CONFIG_PATH=${LOCALFDPPINST}/lib/pkgconfig
./default-configure -d
make
make -C src/tests test-i386.exe

# Install the FAT mount helper
sudo cp test/dosemu_fat_mount.sh /bin/.
sudo chown root.root /bin/dosemu_fat_mount.sh
sudo chmod 755 /bin/dosemu_fat_mount.sh

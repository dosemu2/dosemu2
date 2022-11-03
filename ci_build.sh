#!/bin/sh

set -e

LOCALFDPP="localfdpp.git"
LOCALFDPPINST="$(pwd)/localfdpp"
FDPPBRANCH=""

test -d ${LOCALFDPP} && exit 1

if [ "${TRAVIS}" = "true" ] ; then
  echo "Travis seems to have some old version of clang in its local directory"
  PATH=$(echo ${PATH} | sed 's,:/usr/local/clang-7.0.0/bin,,g')
  export PATH
  echo PATH is ${PATH}
fi

git clone --depth 1 --no-single-branch https://github.com/dosemu2/fdpp.git ${LOCALFDPP}
(
  cd ${LOCALFDPP} || exit 2
  [ -z "$FDPPBRANCH" ] || git checkout "$FDPPBRANCH"
  git config user.email "cibuild@example.com"
  git config user.name "CI build"
  git tag tmp -m "make git-describe happy"

  echo "DEBUG_MODE = 1"  >  local.mak
  echo "EXTRA_DEBUG = 1" >> local.mak
  echo "USE_UBSAN = 1" >> local.mak

  # Install the build dependancies based FDPP's debian/control file
  mk-build-deps --install --root-cmd sudo

  # Seems to miss this, perhaps the optional dependency confuses things
  if [ "${TRAVIS}" = "true" ] ; then
    sudo apt install binutils
  fi

  make clean all install PREFIX=${LOCALFDPPINST}
)

# Install the build dependancies based Dosemu's debian/control file
mk-build-deps --install --root-cmd sudo

export PKG_CONFIG_PATH=${LOCALFDPPINST}/lib/pkgconfig
if [ "${SUBTYPE}" = "asan" ] ; then
  sed -i 's/asan off/asan on/g' compiletime-settings.devel
fi
CC=clang ./default-configure -d
make

# Install the FAT mount helper
sudo cp test/dosemu_fat_mount.sh /bin/.
sudo chown root.root /bin/dosemu_fat_mount.sh
sudo chmod 755 /bin/dosemu_fat_mount.sh

# Install the TAP helper
sudo cp test/dosemu_tap_interface.sh /bin/.
sudo chown root.root /bin/dosemu_tap_interface.sh
sudo chmod 755 /bin/dosemu_tap_interface.sh

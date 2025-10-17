#!/bin/sh

set -e

if [ "${RUNTYPE}" = "packaged" ] ; then
  sudo add-apt-repository -y -c main -c main/debug ppa:dosemu2/ppa
  sudo apt install -y \
    dosemu2 \
    dosemu2-dbgsym \
    fdpp \
    fdpp-dbgsym
  exit 0
fi

# Build dosemu2 and fdpp locally
LOCALFDPP="localfdpp.git"
LOCALFDPPINST="$(pwd)/localfdpp"
FDPPBRANCH=""

test -d ${LOCALFDPP} && exit 1

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
  if [ "${BLDTYPE}" = "asan" ] ; then
    echo "USE_ASAN = 1" >> local.mak
  fi

  # Install the build dependancies based FDPP's debian/control file
  sudo add-apt-repository ppa:stsp-0/nasm-segelf
  sudo add-apt-repository ppa:stsp-0/thunk-gen
  sudo apt update -q
  mk-build-deps --install --root-cmd sudo

  make
  sudo make install
)

# Install the build dependancies based Dosemu's debian/control file
sudo add-apt-repository -y -c main -c main/debug ppa:dosemu2/ppa
mk-build-deps --install --root-cmd sudo
sudo apt remove -y fdpp

if [ "${BLDTYPE}" = "asan" ] ; then
  sed -i 's/asan off/asan on/g' compiletime-settings.devel
fi
./default-configure -d
make
sudo make install

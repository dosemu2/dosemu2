#!/bin/sh

set -e

if [ "${BLDTYPE}" = "packaged" ] ; then
  echo "Adding dosemu2 PPA..."
  sudo add-apt-repository -y -c main -c main/debug ppa:dosemu2/ppa
  sudo apt install -y -f \
    dosemu2 dosemu2-dbgsym \
    libfdldr35 libfdldr35-dbgsym \
    libfdpp35 libfdpp35-dbgsym \

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

  echo "Configuring PPAs..."
  sudo add-apt-repository -y --no-update ppa:stsp-0/thunk-gen
  sudo apt update -q
  sudo apt install -f

  # Install the build dependancies based FDPP's debian/control file
  mk-build-deps --install --root-cmd sudo || sudo apt install ./fdpp-build-deps*.deb --simulate

  make
  sudo make install
)

sudo add-apt-repository -y --no-update -c main -c main/debug ppa:dosemu2/ppa
sudo apt update -q
sudo apt install -f

# Remove fdpp-dev from debian build dependencies (use bash as padding)
sed -i -e 's/fdpp-dev,/bash,/' debian/control

# Install the build dependancies based Dosemu's debian/control file
mk-build-deps --install --root-cmd sudo || sudo apt install ./dosemu2-build-deps*.deb --simulate

if [ "${BLDTYPE}" = "asan" ] ; then
  sed -i 's/asan off/asan on/g' compiletime-settings.devel
fi
./default-configure -d
make
sudo make install

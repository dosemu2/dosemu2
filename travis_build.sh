#!/bin/sh

LOCALFDPP="localfdpp.git"
LOCALFDPPINST="$(pwd)/localfdpp"

test -d ${LOCALFDPP} && exit 1

git clone --depth 1 https://github.com/stsp/fdpp.git ${LOCALFDPP}
(
  cd ${LOCALFDPP} || exit 2
  git tag tmp -m "make git-describe happy"

  echo "DEBUG_MODE = 1"  >  local.mak
  echo "EXTRA_DEBUG = 1" >> local.mak
  echo "USE_UBSAN = 1" >> local.mak

  make clean all install PREFIX=${LOCALFDPPINST}
)

env PKG_CONFIG_PATH=${LOCALFDPPINST}/lib/pkgconfig ./default-configure -d && make
make -C src/tests test-i386.exe

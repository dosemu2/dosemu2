#!/bin/sh

# Usage: ./buildwithfdpp.sh /path/to/fdpp.git --matchapi

# Notes:
#    1/ Copy this script to the top directory of the dosemu source,
#       as it won't exist in earlier versions and will be removed
#       during bisect checkouts.
#    2/ This script has been tested with dosemu2 / fdpp combinations to
#       API 26..13 tags okay to configure, build and run FDPP plugin.
#       API 12..11 tags okay to configure, build and run but with FS issues
#       API  <= 10 tags not tested

if [ ! -d src ] ; then
  echo Run this script from the top directory of the dosemu2 source
  exit 3
fi

FDPP_LIBRARY_SOURCE=$(realpath "$1")
if [ ! -d ${FDPP_LIBRARY_SOURCE} ] ; then
  echo Local FDPP git directory \"${FDPP_LIBRARY_SOURCE}\" does\'t exist
  exit 2
fi

FDPP_PLUGIN_SOURCE=src/plugin/fdpp/fdpp.c
if [ ! -f ${FDPP_PLUGIN_SOURCE} ] ; then
  echo FDPP plugin was not implemented at this version of Dosemu2
  exit 1
fi

FDPP_PLUGIN_VER=$(awk -F= '/FDPP_API_VER !=/{printf("%d", $2);}' ${FDPP_PLUGIN_SOURCE})

if [ -f "${FDPP_LIBRARY_SOURCE}/fdpp/thunks.h" ] ; then
  THUNKS="${FDPP_LIBRARY_SOURCE}/fdpp/thunks.h"
else
  THUNKS="${FDPP_LIBRARY_SOURCE}/include/fdpp/thunks.h"
fi
FDPP_LIBRARY_VER=$(awk -F" " '/#define FDPP_API_VER/{printf("%d", $3);}' "${THUNKS}")

if [ "${FDPP_PLUGIN_VER}" != "${FDPP_LIBRARY_VER}" ] ; then
  if [ "$2" != "--matchapi" ] ; then
    echo FDPP version mismatch in "${FDPP_LIBRARY_SOURCE}"
    echo need API"${FDPP_PLUGIN_VER}" but have API"${FDPP_LIBRARY_VER}"
    exit 1
  fi
  echo Building FDPP at API${FDPP_PLUGIN_VER}
  (cd ${FDPP_LIBRARY_SOURCE} && git checkout -f API${FDPP_PLUGIN_VER} && make clean all)
fi

# make distclean

if grep -q fdpp-build-path default-configure ; then
  ./default-configure -d fdpp-build-path=${FDPP_LIBRARY_SOURCE}/fdpp \
                         fdpp-include-path=${FDPP_LIBRARY_SOURCE}/include && make
else
  [ -d pkgconfig ] || mkdir pkgconfig
  sed \
    -e "s,^fdpplibdir=.*$,fdpplibdir=${FDPP_LIBRARY_SOURCE}/fdpp,g" \
    -e "s,^fdppincdir=.*$,fdppincdir=${FDPP_LIBRARY_SOURCE},g" \
    -e "s,^includedir=.*$,includedir=${FDPP_LIBRARY_SOURCE},g" \
    ${FDPP_LIBRARY_SOURCE}/fdpp/fdpp.pc > pkgconfig/fdpp.pc

  env PKG_CONFIG_PATH=`pwd`/pkgconfig ./default-configure -d && make
fi

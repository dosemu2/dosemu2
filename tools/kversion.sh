#! /bin/sh
#
# Well, we need this to generate a kernel version number,
# that can be easyly used whith #if.
#
# We do *not* use /proc/version, because we don't rely on the version of the
# loaded kernel, but on the /usr/src/linux/include..., which is used
# for compilation of dosemu. Ok?
#
# Don't laugh on the 3 digits for each version level,
# we'll never know how long Linus is staying on each level.
#

VERSION=""

function zeropad() {
  if [ $(($1)) -lt 10 ]; then VERSION="${VERSION}00$1"
  else
    if [ $(($1)) -lt 100 ]; then VERSION="${VERSION}0$1"
    else VERSION="${VERSION}$1"; fi
  fi
}


if [ -z "$1" ]; then
  KERNELSRC="/usr/src/linux"
else
  KERNELSRC=$1
fi

if [ -z "$2" ]; then
  DOSEMUSRC="../"
else
  DOSEMUSRC=$2
fi

if [ -f ${KERNELSRC}/include/linux/version.h ]; then
  VERSIONFILE="${KERNELSRC}/include/linux/version.h"
else
  VERSIONFILE="${KERNELSRC}/tools/version.h"
  if [ ! -f ${VERSIONFILE} ]; then
    echo "missing ${VERSIONFILE}, giving up!"
    exit 1
  fi
fi


ASCII_VERSION=`grep UTS_RELEASE ${VERSIONFILE} |cut '-d"' -f2`
VERSION=`echo "${ASCII_VERSION}" |cut '-d.' -f1` 
zeropad `echo "${ASCII_VERSION}" |cut '-d.' -f2` 
zeropad `echo "${ASCII_VERSION}" |cut '-d.' -f3` 
echo "#ifndef KERNEL_VERSION" > ${DOSEMUSRC}/include/kversion.h
echo "#define KERNEL_VERSION ${VERSION}" >> ${DOSEMUSRC}/include/kversion.h
echo "#endif " >> ${DOSEMUSRC}/include/kversion.h

#! /usr/local/bin/bash
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
  if [ $1 -lt 10 ]; then VERSION="${VERSION}00$1"
  else
    if [ $1 -lt 100 ]; then VERSION="${VERSION}0$1"
    else VERSION="${VERSION}$1"; fi
  fi
}

if [ -z "$1" ]; then
  KERNELSRC="-find"
else
  KERNELSRC=$1
fi

if [ "$KERNELSRC" = "-find" ]; then
  KERNELSRC=""
  if [ -d /usr/include/linux ]; then
    xxxx=`(cd /usr/include/linux; set -P; pwd)`
    KERNELSRC=`(cd $xxxx/../..; pwd)`
  else
    if [ -d /usr/src/linux ]; then
      KERNELSRC=`(cd /usr/src/linux; set -P; pwd)`
    else
      if [ -d /linux ]; then
        KERNELSRC=`(cd /linux; set -P; pwd)`
      else
        echo "kversion.sh: cannot find any of the standard linux trees, giving up"
        echo "You have to edit LINUX_KERNEL in the main Makefile so that it"
        echo "points to your Linux source tree, which at least must contain ./include/*"
        echo 'In addition (if your ./linux/include lacks the version.h file):'
        echo "You may edit and uncomment KERNEL_VERSION in the main Makefile,"
        echo "but this is not recommended !"
        echo "The format is:"
        echo "  KERNEL_VERSION=x0yy0zz meaning Linux version x.yy.zz"
        echo "Example:"
        echo "  KERNEL_VERSION=1002002 meaning Linux version 1.2.2"
        exit 1
      fi
    fi
  fi
fi

if [ "$2" = "-print" ]; then
  echo "$KERNELSRC"
  exit 0
fi

if [ -z "$2" ]; then
  DOSEMUSRC="../"
else
  DOSEMUSRC=$2
fi

if [ -f ${KERNELSRC}/sys/param.h ]; then
  VERSIONFILE="${KERNELSRC}/sys/param.h"
else
  echo "missing ${VERSIONFILE}, giving up!"
  exit 1
fi


VERSION=`grep -w '^#define NetBSD' ${VERSIONFILE} |awk '{print $3}'`
zeropad `grep NetBSD1_0 ${VERSIONFILE} |awk '{print $3}'`
echo "#ifndef NETBSD_VERSION" > ${DOSEMUSRC}/include/kversion.h
echo "#define NETBSD_VERSION ${VERSION}" >> ${DOSEMUSRC}/include/kversion.h
echo "#endif " >> ${DOSEMUSRC}/include/kversion.h

#! /bin/sh
#
# Well, we need this to generate a kernel version number,
# that can be easily used whith #if.
#
# We do *not* use /proc/version, because we don't rely on the version of the
# loaded kernel, but on the /usr/src/linux/include..., which is used
# for compilation of dosemu. Ok?
#
# Don't laugh on the 3 digits for each version level,
# we'll never know how long Linus is staying on each level.
#

VERSION=""

# "set -P" does not work on bash 1.14.2, and 1.14.3 is buggy
nolinks=1
export nolinks

function zeropad() {
  if [ $1 -lt 10 ]; then VERSION="${VERSION}00$1"
  else
    if [ $1 -lt 100 ]; then VERSION="${VERSION}0$1"
    else VERSION="${VERSION}$1"; fi
  fi
}

if [ "$2" = "-dotsOK" ]; then
  VV=`grep '#define KERNEL_VERSION' ${1}/include/kversion.h  |awk '{print $3}'`
#  VV=`expr 0 + $VV`
  if [ "$VV" = "1003040" ]; then
    echo ""
    echo '  - CAUTION, you compiled for Linux-1.3.40'
    echo '    This requires to mount MSDOS-FS with option dotsOK=no like this:'
    echo '    mount -t msdos -o dotsOK=no /dev/... /mnt'
  fi
  exit 0
fi


if [ -z "$1" ]; then
  KERNELSRC="-find"
else
  KERNELSRC=$1
fi

if [ "$KERNELSRC" = "-find" ]; then
  KERNELSRC=""
  if [ -d /usr/include/linux ]; then
    xxxx=`(cd /usr/include/linux; pwd)`
    KERNELSRC=`(cd $xxxx/../..; pwd)`
  else
    if [ -d /usr/src/linux ]; then
      KERNELSRC=`(cd /usr/src/linux; pwd)`
    else
      if [ -d /linux ]; then
        KERNELSRC=`(cd /linux; pwd)`
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

if [ -f ${KERNELSRC}/include/linux/version.h ]; then
  VERSIONFILE="${KERNELSRC}/include/linux/version.h"
else
  echo "Sorry, you have an incomplete or uncompiled kernel source tree"
  echo "Unable to find linux/version.h, giving up!"
  exit 1
fi


ASCII_VERSION=`grep UTS_RELEASE ${VERSIONFILE} |cut '-d"' -f2`
VERSION=`echo "${ASCII_VERSION}" |cut '-d.' -f1` 
zeropad `echo "${ASCII_VERSION}" |cut '-d.' -f2` 
zeropad `echo "${ASCII_VERSION}" |cut '-d.' -f3` 
echo "#ifndef KERNEL_VERSION" > ${DOSEMUSRC}/include/kversion.h
echo "#define KERNEL_VERSION ${VERSION}" >> ${DOSEMUSRC}/include/kversion.h
echo "#endif " >> ${DOSEMUSRC}/include/kversion.h

# know we create a version stamp for the parent Makefile
BINPATH=`(cd ${DOSEMUSRC}/../bin; pwd)`/..
grep UTS_RELEASE ${VERSIONFILE} > ${BINPATH}/kversion.stamp

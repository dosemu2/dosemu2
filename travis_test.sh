#!/bin/sh

#env FDPP_KERNEL_DIR=`pwd`/localfdpp/share/fdpp bin/dosemu.bin \
#	-n -f test-imagedir/dosemu.conf -o test.log \
#	--Fimagedir `pwd`/test-imagedir \
#	--Flibdir `pwd`/test-libdir

mkdir -p ${HOME}/.dosemu/run
touch ${HOME}/.dosemu/disclaimer

# Set FDPP_KERNEL_DIR to non-standard location beforehand
nosetests -v test/test_dos.py

for i in test_dos.*.*.log ; do
  test -f $i || exit 0

  echo =======================================================================
  echo $i
  cat $i
  j=$(basename $i .log).xpt
  echo =======================================================================
  echo $j
  cat $j
done

exit 1

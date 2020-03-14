#!/bin/sh

#env FDPP_KERNEL_DIR=`pwd`/localfdpp/share/fdpp bin/dosemu.bin \
#	-n -f test-imagedir/dosemu.conf -o test.log \
#	--Fimagedir `pwd`/test-imagedir \
#	--Flibdir `pwd`/test-libdir

mkdir -p ${HOME}/.dosemu/run
touch ${HOME}/.dosemu/disclaimer

# Get any test binaries we need
TBINS="test-binaries"
THOST="http://www.spheresystems.co.uk/test-binaries"
if [ ! -d ${TBINS} ] ; then
  mkdir ${TBINS}
  (
    cd ${TBINS}
    [ -f FR-DOS-1.20.tar ] || wget ${THOST}/FR-DOS-1.20.tar
  )
fi

# Set FDPP_KERNEL_DIR to non-standard location beforehand
python3 test/test_dos.py
# single DOS example
# python3 test/test_dos.py FRDOS120TestCase
# single test example
# python3 test/test_dos.py FRDOS120TestCase.test_mfs_fcb_rename_wild_1

for i in test_dos.*.*.log ; do
  test -f $i || exit 0
done

exit 1

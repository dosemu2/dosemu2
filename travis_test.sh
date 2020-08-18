#!/bin/sh

set -e

# Get any test binaries we need
TBINS="test-binaries"
THOST="http://www.spheresystems.co.uk/test-binaries"
if [ ! -d ${TBINS} ] ; then
  mkdir ${TBINS}
  (
    cd ${TBINS}
    [ -f DR-DOS-7.01.tar ] || wget ${THOST}/DR-DOS-7.01.tar
    [ -f FR-DOS-1.20.tar ] || wget ${THOST}/FR-DOS-1.20.tar
    [ -f MS-DOS-6.22.tar ] || wget ${THOST}/MS-DOS-6.22.tar
  )
fi

if [ "${TRAVIS_BRANCH}" != "devel" ] ; then
  export SKIP_CLASS_THRESHOLD="1"
fi

# Set FDPP_KERNEL_DIR to non-standard location beforehand
echo
echo "====================================================="
echo "=        Tests run on various flavours of DOS       ="
echo "====================================================="
python3 test/test_dos.py
# single DOS example
# python3 test/test_dos.py FRDOS120TestCase
# single test example
# python3 test/test_dos.py FRDOS120TestCase.test_mfs_fcb_rename_wild_1

for i in test_*.*.*.log ; do
  test -f $i || exit 0
done

exit 1

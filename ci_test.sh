#!/bin/bash

if [ "${GITHUB_ACTIONS}" = "true" ] ; then
  # CI is already set
  export CI_BRANCH="$(echo ${GITHUB_REF} | cut -d/ -f3)"
  if [ "${GITHUB_EVENT_NAME}" = "push" ] && [ "${GITHUB_REPOSITORY_OWNER}" = "dosemu2" ] && [ "${CI_BRANCH}" = "devel" ] ; then
    export RUNTYPE="simple"
  fi
fi

TBINS="test-binaries"
if [ "${CI}" = "true" ] ; then
  [ -d "${HOME}"/cache ] || mkdir "${HOME}"/cache
  [ -h "${TBINS}" ] || ln -s "${HOME}"/cache "${TBINS}"
else
  [ -d "${TBINS}"] || mkdir "${TBINS}"
fi
python3 test/test_dos.py --get-test-binaries

# Make cpu tests here so that we see any failures
make -C test/cpu clean all

echo
echo "====================================================="
echo "=        Tests run on various flavours of DOS       ="
echo "====================================================="
# all DOS flavours, all tests
# python3 test/test_dos.py
# single DOS example
# python3 test/test_dos.py FRDOS120TestCase
# single test example
# python3 test/test_dos.py FRDOS120TestCase.test_mfs_fcb_rename_wild_1

case "${RUNTYPE}" in
  "full")
    export NO_FAILFAST=1
    python3 test/test_dos.py PPDOSGITTestCase
    python3 test/test_dos.py MSDOS622TestCase
    python3 test/test_dos.py FRDOS130TestCase
    python3 test/test_dos.py DRDOS701TestCase
    ;;
  "normal")
    export SKIP_UNCERTAIN=1
    python3 test/test_dos.py PPDOSGITTestCase
    python3 test/test_dos.py MSDOS622TestCase
    ;;
  "simple")
    export SKIP_EXPENSIVE=1
    export SKIP_UNCERTAIN=1
    python3 test/test_dos.py PPDOSGITTestCase
    ;;
esac

for i in test_*.*.*.log ; do
  test -f $i || exit 0
done

exit 1

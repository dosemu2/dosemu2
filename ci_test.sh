#!/bin/bash

set -e

# Get any test binaries we need
TBINS="test-binaries"
THOST="http://www.spheresystems.co.uk/test-binaries"

if [ "${TRAVIS}" = "true" ] ; then
  export CI="true"
  export CI_BRANCH="${TRAVIS_BRANCH}"
  if [ "${TRAVIS_EVENT_TYPE}" = "cron" ] ; then
    export CI_EVENT="cron"
  else
    export CI_EVENT="normal"
  fi

elif [ "${GITHUB_ACTIONS}" = "true" ] ; then
  # CI is already set
  export CI_BRANCH="$(echo ${GITHUB_REF} | cut -d/ -f3)"
  if [ "${GITHUB_EVENT_NAME}" = "schedule" ] ; then
    export CI_EVENT="cron"
  elif [ "${GITHUB_EVENT_NAME}" = "push" ] && [ "${GITHUB_REPOSITORY_OWNER}" = "dosemu2" ] && [ "${CI_BRANCH}" = "devel" ] ; then
    export CI_EVENT="simple"
  else
    export CI_EVENT="normal"
  fi
fi

if [ "${CI}" = "true" ] ; then
  [ -d "${HOME}"/cache ] || mkdir "${HOME}"/cache
  [ -h "${TBINS}" ] || ln -s "${HOME}"/cache "${TBINS}"
else
  [ -d "${TBINS}"] || mkdir "${TBINS}"
fi

(
  cd "${TBINS}" || exit 1
  [ -f DR-DOS-7.01.tar ] || wget ${THOST}/DR-DOS-7.01.tar
  [ -f FR-DOS-1.20.tar ] || wget ${THOST}/FR-DOS-1.20.tar
  [ -f MS-DOS-6.22.tar ] || wget ${THOST}/MS-DOS-6.22.tar

  [ -f VARIOUS.tar ] || wget ${THOST}/VARIOUS.tar
  [ -f TEST_EMM286.tar ] || wget ${THOST}/TEST_EMM286.tar
)

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

if [ "${CI_EVENT}" = "cron" ] ; then
  if [ "${TRAVIS}" = "true" ] ; then
    python3 test/test_dos.py --require-attr=cputest PPDOSGITTestCase MSDOS622TestCase FRDOS120TestCase
  else
    python3 test/test_dos.py
  fi
else
  if [ "${CI_EVENT}" = "simple" ] ; then
    python3 test/test_dos.py PPDOSGITTestCase
  else
    python3 test/test_dos.py PPDOSGITTestCase MSDOS622TestCase
  fi
fi

for i in test_*.*.*.log ; do
  test -f $i || exit 0
done

exit 1

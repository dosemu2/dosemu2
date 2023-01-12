#!/bin/bash

. ./ci_test_prereq.sh

if [ "${TRAVIS}" = "true" ] ; then
  export CI="true"
  export CI_BRANCH="${TRAVIS_BRANCH}"
  if [ "${TRAVIS_EVENT_TYPE}" = "cron" ] ; then
    export RUNTYPE="full"
  else
    export RUNTYPE="simple"
  fi

elif [ "${GITHUB_ACTIONS}" = "true" ] ; then
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

if [ "${TRAVIS}" = "true" ] ; then
  ARGS="--require-attr=cputest"
else
  ARGS=""
fi

case "${RUNTYPE}" in
  "full")
    ARGS="${ARGS} PPDOSGITTestCase MSDOS622TestCase FRDOS130TestCase DRDOS701TestCase"
    ;;
  "normal")
    ARGS="${ARGS} PPDOSGITTestCase MSDOS622TestCase"
    export SKIP_UNCERTAIN=1
    ;;
  "simple")
    ARGS="${ARGS} PPDOSGITTestCase"
    export SKIP_EXPENSIVE=1
    export SKIP_UNCERTAIN=1
    ;;
esac

# CC is set on Travis and can confuse compilation during tests
unset CC

# Make CPU/FPU tests here so that we see any failures
make -C test/cpu clean all
make -C test/fpu clean all

python3 test/test_dos.py ${ARGS}

for i in test_*.*.*.log ; do
  test -f $i || exit 0
done

exit 1

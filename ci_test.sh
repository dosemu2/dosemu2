#!/bin/bash

set -eo pipefail

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
python3 test/test_dosemu.py --get-test-binaries

export PYTHONUNBUFFERED=1
export TEST_DOSEMU=/usr/local/bin/dosemu
export TEST_CMDDIR=/usr/local/share/dosemu/commands

if [ "${BLDTYPE}" = "packaged" ] ; then
  export SKIP_UNCERTAIN=1
  if [ "${OS}" = "ubuntu-22.04" ] ; then
    export SKIP_NATIVE_DPMI=1
  fi
  export TEST_DOSEMU=/usr/bin/dosemu
  export TEST_CMDDIR=/usr/share/dosemu/dosemu2-cmds-0.3
fi

if [ "${RUNTYPE}" = "simple" ] ; then
  export SKIP_EXPENSIVE=1
  export SKIP_UNCERTAIN=1
elif [ "${RUNTYPE}" = "normal" ] ; then
  export SKIP_UNCERTAIN=1
else
  export NO_FAILFAST=1
fi

cat >&2 << EOF
=====================================================
=         Tests run on KVM and emulated CPU         =
=====================================================
EOF
env NO_FAILFAST=1 python3 test/test_processor.py

cat >&2 << EOF2
=====================================================
=        Tests run on various flavours of DOS       =
=====================================================
EOF2

case "${RUNTYPE}" in
  "full")
    python3 test/test_dosemu.py PPDOSGITTestCase
    python3 test/test_dosemu.py MSDOS622TestCase
    python3 test/test_dosemu.py FRDOS130TestCase
    python3 test/test_dosemu.py DRDOS701TestCase
    ;;
  "normal")
    python3 test/test_dosemu.py PPDOSGITTestCase
    python3 test/test_dosemu.py MSDOS622TestCase
    ;;
  "simple")
    python3 test/test_dosemu.py PPDOSGITTestCase
    ;;
esac

for i in test_*.*.*.log ; do
  test -f $i || exit 0
done

# If we get here, then we've failed so copy various system logs, give them
# a name that is picked up by the artefact uploaded.
[ -f /var/log/audit/audit.log ] && sudo cp /var/log/audit/audit.log test_audit.log
[ -f /var/log/syslog ] && sudo cp /var/log/syslog test_syslog.log
[ -f /var/log/auth.log ] && sudo cp /var/log/auth.log test_auth.log

sudo chmod 644 test_*.log

exit 1

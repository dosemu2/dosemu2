#!/bin/bash

if [ "${GITHUB_ACTIONS}" != "true" ] ; then
  echo "Run this script only under Github Actions"
  exit 1
fi

set -eo pipefail

export PYTHONUNBUFFERED=1
export TEST_DOSEMU=/usr/local/bin/dosemu
export TEST_CMDDIR=/usr/local/share/dosemu/commands

if [ "${BLDTYPE}" = "packaged" ] ; then
  if [ "${OS}" = "ubuntu-22.04" ] ; then
    export SKIP_NATIVE_DPMI=1
  fi
  export TEST_DOSEMU=/usr/bin/dosemu
  export TEST_CMDDIR=/usr/share/dosemu/dosemu2-cmds-0.3
elif [ "${BLDTYPE}" = "asan" ] ; then
  export DEFAULT_TIMEOUT=45
fi

if [ "${RUNTYPE}" = "simple" ] ; then
  export SKIP_EXPENSIVE=1
elif [ "${RUNTYPE}" = "full" ] ; then
  export NO_FAILFAST=1
fi

set +e

cat >&2 << EOF
=====================================================
=      Tests run on emulated CPU, KVM and VM86      =
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
sudo -- journalctl -b -k > test_dmesg.log
sudo -- journalctl -b -o short-precise > test_syslog.log
sudo -- journalctl -b -o short-precise _TRANSPORT=audit > test_audit.log
sudo -- journalctl -b -o short-precise SYSLOG_FACILITY=4 SYSLOG_FACILITY=10 > test_auth.log

exit 1

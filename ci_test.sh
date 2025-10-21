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

# Make cpu tests here so that we see any failures
make -C test/cpu clean all

cat >&2 << EOF
=====================================================
=        Tests run on various flavours of DOS       =
=====================================================
EOF
# all DOS flavours, all tests
# python3 test/test_dosemu.py
# single DOS example
# python3 test/test_dosemu.py FRDOS120TestCase
# single test example
# python3 test/test_dosemu.py FRDOS120TestCase.test_mfs_fcb_rename_wild_1

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

if [ "${BLDTYPE}" != "packaged" ] ; then  # Only makes sense if we are building the source
  is_primary() {
    [ "${GITHUB_REPOSITORY:-}" = "dosemu2/dosemu2" ]
  }

  is_devel() {
    [ "$(git branch --show-current)" = "devel" ]
  }

  is_merge() {
    git log -1 HEAD | grep -Fq 'Merge pull request'
  }

  branch_has_kvmoff() {
    git log ${BASE_SHA:-}..HEAD | grep -Fq '[kvmoff ci]'
  }

  head_has_kvmoff() {
    git log -1 HEAD | grep -Fq '[kvmoff ci]'
  }

  merge_has_kvmoff() {
    git log HEAD ^HEAD^1 | grep -Fq '[kvmoff ci]'
  }

  branch_has_emulator_changes() {
    [ "$(git diff --name-only ${BASE_SHA:-}..HEAD -- src/base/emu-i386/simx86 | wc -l)" != "0" ]
  }

  last5_has_emulator_changes() {
    [ "$(git diff --name-only HEAD~5 -- src/base/emu-i386/simx86 | wc -l)" != "0" ]
  }

  merge_has_emulator_changes() {
    [ "$(git diff --name-only HEAD ^HEAD^1 -- src/base/emu-i386/simx86 | wc -l)" != "0" ]
  }

  if is_primary ; then
    if is_devel ; then # could be push direct to devel, or merge commit
      if (is_merge && (merge_has_kvmoff || merge_has_emulator_changes)) ||
          head_has_kvmoff || last5_has_emulator_changes ; then
        export NO_KVM=1
      fi
    else # could be test merge for a PR (I tested), or a topic branch for dosemu2
      if branch_has_kvmoff || branch_has_emulator_changes ; then
        export NO_KVM=1
      fi
    fi

  else # someone else's repo, default or topic branch prior to PR (I tested)
    # Can't assume anything about repo, main branch name, whether it's up to date, etc.
    if head_has_kvmoff || last5_has_emulator_changes ; then
      export NO_KVM=1
    fi
  fi
fi

case "${RUNTYPE}" in
  "full")
    export NO_FAILFAST=1
    python3 test/test_dosemu.py PPDOSGITTestCase
    python3 test/test_dosemu.py MSDOS622TestCase
    python3 test/test_dosemu.py FRDOS130TestCase
    python3 test/test_dosemu.py DRDOS701TestCase
    ;;
  "normal")
    export SKIP_UNCERTAIN=1
    python3 test/test_dosemu.py PPDOSGITTestCase
    python3 test/test_dosemu.py MSDOS622TestCase
    ;;
  "simple")
    export SKIP_EXPENSIVE=1
    export SKIP_UNCERTAIN=1
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

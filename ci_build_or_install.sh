#!/bin/sh

set -e

# Launchpad drops the PPA signing key lookup often enough to matter, and
# add-apt-repository has no retry of its own: getSigningKeyData() raises and
# the script is gone.  Seen as HTTP 504 and as HTTP 500 with the body
# GPGKeyTemporarilyNotFoundError, which says in as many words that the key
# is coming back.  It happens before anything is built, so a whole run is
# spent on a lookup that would have worked a minute later.
add_apt_repository()
{
  attempt=1
  while : ; do
    status=0
    sudo add-apt-repository "$@" || status=$?
    if [ ${status} -eq 0 ] ; then
      return 0
    fi
    if [ ${attempt} -ge 3 ] ; then
      echo "add-apt-repository $*: attempt ${attempt} failed with status" \
        "${status}, giving up" >&2
      return ${status}
    fi
    delay=$((attempt * 15))
    echo "add-apt-repository $*: attempt ${attempt} failed with status" \
      "${status}, retrying in ${delay}s" >&2
    sleep ${delay}
    attempt=$((attempt + 1))
  done
}

# apt does not fail when a repository's index cannot be fetched: it prints
# "Err:" and "old ones used instead" and exits 0.  A 503 from Launchpad on
# the PPA index therefore surfaces a minute and a half later, as
# mk-build-deps failing to install fdpp-build-deps or as a dependency on
# thunk-gen that cannot be satisfied, which reads like a packaging problem
# and is not one.  Retry while an index is missing, and otherwise answer
# exactly as the bare call would: a fetch error that does not clear is
# carried on with, as apt intends, while a failure of another kind -- a
# held dpkg lock, a sources.list that does not parse -- keeps its non-zero
# status and stops the script on its own step.
apt_update()
{
  attempt=1
  while : ; do
    status=0
    out="$(sudo apt-get update -q 2>&1)" || status=$?
    printf '%s\n' "${out}"
    if ! printf '%s\n' "${out}" | grep -q '^Err:' ; then
      return ${status}
    fi
    if [ ${attempt} -ge 3 ] ; then
      echo "apt-get update: attempt ${attempt} still reports a fetch error," \
        "going on with the indexes we have" >&2
      return ${status}
    fi
    delay=$((attempt * 15))
    echo "apt-get update: attempt ${attempt} could not fetch an index," \
      "retrying in ${delay}s" >&2
    sleep ${delay}
    attempt=$((attempt + 1))
  done
}

if [ "${BLDTYPE}" = "packaged" ] ; then
  echo "Adding dosemu2 PPA..."
  add_apt_repository -y -c main -c main/debug ppa:dosemu2/ppa
  apt_update
  sudo apt install -y \
    dosemu2 \
    dosemu2-dbgsym \
    fdpp \
    fdpp-dbgsym
  exit 0
fi

# Build dosemu2 and fdpp locally
LOCALFDPP="localfdpp.git"
LOCALFDPPINST="$(pwd)/localfdpp"
FDPPBRANCH=""

test -d ${LOCALFDPP} && exit 1

git clone --depth 1 --no-single-branch https://github.com/dosemu2/fdpp.git ${LOCALFDPP}
(
  cd ${LOCALFDPP} || exit 2
  [ -z "$FDPPBRANCH" ] || git checkout "$FDPPBRANCH"
  git config user.email "cibuild@example.com"
  git config user.name "CI build"
  git tag tmp -m "make git-describe happy"

  echo "DEBUG_MODE = 1"  >  local.mak
  echo "EXTRA_DEBUG = 1" >> local.mak
  echo "USE_UBSAN = 1" >> local.mak
  if [ "${BLDTYPE}" = "asan" ] ; then
    echo "USE_ASAN = 1" >> local.mak
  fi

  echo "Configuring PPAs..."
  # Install the build dependancies based FDPP's debian/control file
  add_apt_repository ppa:stsp-0/thunk-gen
  apt_update
  mk-build-deps --install --root-cmd sudo

  make
  sudo make install
)

# Install the build dependancies based Dosemu's debian/control file
add_apt_repository -y -c main -c main/debug ppa:dosemu2/ppa
apt_update
mk-build-deps --install --root-cmd sudo
sudo apt remove -y fdpp

if [ "${BLDTYPE}" = "asan" ] ; then
  sed -i 's/asan off/asan on/g' compiletime-settings.devel
fi
./default-configure -d
make
sudo make install

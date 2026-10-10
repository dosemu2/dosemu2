#!/bin/sh

set -e

# Retry fetches, shorten apt's two-minute socket timeout, and never let dpkg
# stop to ask: nobody is watching the runner.
export DEBIAN_FRONTEND=noninteractive
sudo tee /etc/apt/apt.conf.d/99ci-retries >/dev/null <<'EOF'
Acquire::Retries "3";
Acquire::http::Timeout "15";
Acquire::https::Timeout "15";
APT::Get::Assume-Yes "true";
DPkg::Options { "--force-confdef"; "--force-confold"; };
EOF

# add-apt-repository has no retry of its own, and Launchpad's key lookup
# fails often enough to lose a run over it.
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

# apt prints "Err:" and exits 0 when an index cannot be fetched, so a 503 on
# the PPA surfaces much later as an unsatisfiable build dep.  Only Launchpad
# hosts count: a runner carries a dozen sources this build has nothing to do
# with.  Other failures print no Err: line and keep their own status.
apt_update()
{
  attempt=1
  while : ; do
    status=0
    out="$(sudo apt-get update -q 2>&1)" || status=$?
    printf '%s\n' "${out}"
    if ! printf '%s\n' "${out}" | grep -Eq '^Err:.*launchpad' ; then
      return ${status}
    fi
    if [ ${attempt} -ge 3 ] ; then
      echo "apt-get update: attempt ${attempt} still could not fetch a PPA" \
        "index, giving up rather than installing whatever else answers to" \
        "these package names" >&2
      return 1
    fi
    delay=$((attempt * 15))
    echo "apt-get update: attempt ${attempt} could not fetch a PPA index," \
      "retrying in ${delay}s" >&2
    sleep ${delay}
    attempt=$((attempt + 1))
  done
}

# apt does not care where a name comes from, so check that the version it
# picked is one our PPA offers before installing it.
require_from_ppa()
{
  for pkg in "$@" ; do
    ver="$(apt-cache policy "${pkg}" | sed -n 's/^  Candidate: //p')"
    if [ -z "${ver}" ] || [ "${ver}" = "(none)" ] ; then
      echo "${pkg}: apt offers no candidate version at all" >&2
      return 1
    fi
    if ! apt-cache madison "${pkg}" | grep -F "| ${ver} |" | \
         grep -q 'ppa\.launchpadcontent\.net/dosemu2/' ; then
      echo "${pkg}: apt would install ${ver}, which our PPA does not" \
        "offer; refusing to test a package that came from somewhere else" >&2
      apt-cache policy "${pkg}" >&2
      return 1
    fi
  done
}

if [ "${BLDTYPE}" = "packaged" ] ; then
  echo "Adding dosemu2 PPA..."
  add_apt_repository -n -y -c main -c main/debug ppa:dosemu2/ppa
  apt_update
  require_from_ppa dosemu2 dosemu2-dbgsym fdpp fdpp-dbgsym
  sudo apt-get install -y \
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
  add_apt_repository -n ppa:stsp-0/thunk-gen
  apt_update
  mk-build-deps --install --root-cmd sudo

  make
  sudo make install
)

# Install the build dependancies based Dosemu's debian/control file
add_apt_repository -n -y -c main -c main/debug ppa:dosemu2/ppa
apt_update
mk-build-deps --install --root-cmd sudo
sudo apt-get remove -y fdpp

if [ "${BLDTYPE}" = "asan" ] ; then
  sed -i 's/asan off/asan on/g' compiletime-settings.devel
fi
./default-configure -d
make
sudo make install

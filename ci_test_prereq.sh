#!/bin/sh

set -e

# Ensure the test-binaries link points to ~/cache
[ -h "test-binaries" ] || ln -s "${HOME}"/cache "test-binaries"

echo "Configuring PPAs..."
sudo add-apt-repository -y ppa:jwt27/djgpp-toolchain
sudo add-apt-repository -y ppa:stsp-0/gcc-ia16
sudo apt update -q

sudo apt install -y \
  libaa-bin \
  acl \
  comcom32 \
  comcom64 \
  cpu-checker \
  nasm \
  python3-cpuinfo \
  python3-pexpect \
  python3-pyte \
  mtools \
  ncurses-term \
  gcc-djgpp \
  djgpp-dev \
  qemu-system-common \
  gdb \
  valgrind \
  gcc-ia16-elf \
  libi86-ia16-elf \
  libi86-testsuite-ia16-elf \
  gcc-multilib \
  dos2unix \
  bridge-utils \
  libvirt-daemon \
  libvirt-daemon-system \
  xvfb \
  xdotool

sudo apt install -y \
  dj64-dbgsym \
  djdev64-dbgsym

# The terminal tests read blink and the bright colours off pyte's cells,
# which pyte only reports from 0.8.2 on, and 24.04 packages 0.8.0.  Step in
# with pip only while the distribution is behind it, so a later runner that
# packages 0.8.2 or newer is left alone rather than downgraded.
if ! python3 - <<'EOF'
import sys
from importlib.metadata import version

try:
    have = tuple(int(n) for n in version("pyte").split(".")[:3])
except Exception:
    sys.exit(1)
sys.exit(0 if have >= (0, 8, 2) else 1)
EOF
then
  sudo pip install --break-system-packages pyte==0.8.2
fi

# Install the FAT mount helper
sudo cp test/dosemu_fat_mount.sh /bin/.
sudo chown root:root /bin/dosemu_fat_mount.sh
sudo chmod 755 /bin/dosemu_fat_mount.sh

# Install the TAP helper
sudo cp test/dosemu_tap_interface.sh /bin/.
sudo chown root:root /bin/dosemu_tap_interface.sh
sudo chmod 755 /bin/dosemu_tap_interface.sh

#!/bin/sh

set -e

sudo add-apt-repository -y ppa:jwt27/djgpp-toolchain
sudo add-apt-repository -y ppa:stsp-0/gcc-ia16
if [ "${RUNTYPE}" = "packaged" ] ; then
  if ! $(grep -q '^Ubuntu 20' /etc/issue) ; then
    sudo add-apt-repository -y -c main -c main/debug ppa:dosemu2/ppa
  else
    sudo add-apt-repository -y ppa:dosemu2/ppa
  fi
fi

sudo apt update -q

sudo apt install -y \
  acl \
  comcom64 \
  dj64-dbgsym \
  djdev64-dbgsym \
  nasm \
  python3-cpuinfo \
  python3-pexpect \
  mtools \
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
  libvirt-daemon-system

if [ "${RUNTYPE}" = "packaged" ] ; then
  sudo apt install -y \
    dosemu2 \
    fdpp
  if ! $(grep -q '^Ubuntu 20' /etc/issue) ; then
    sudo apt install -y \
      dosemu2-dbgsym \
      fdpp-dbgsym
  fi
fi

# Install the FAT mount helper
sudo cp test/dosemu_fat_mount.sh /bin/.
sudo chown root:root /bin/dosemu_fat_mount.sh
sudo chmod 755 /bin/dosemu_fat_mount.sh

# Install the TAP helper
sudo cp test/dosemu_tap_interface.sh /bin/.
sudo chown root:root /bin/dosemu_tap_interface.sh
sudo chmod 755 /bin/dosemu_tap_interface.sh

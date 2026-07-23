#!/bin/sh

set -e

# Install djgpp and ia16 compilers / libs.
sudo add-apt-repository -y --no-update ppa:jwt27/djgpp-toolchain
sudo add-apt-repository -y --no-update ppa:stsp-0/gcc-ia16
is_amd64v3=$(apt-config dump | grep -q 'Variants.*amd64v3' && echo true || echo false) # APT::Architecture-Variants "amd64v3"
if [ "$is_amd64v3" = "true" ] ; then
  # Since there are no host libs to link against we can use amd64 arch on arm64v3
  sudo sed -i '/^URIs:/i Architectures: amd64' /etc/apt/sources.list.d/jwt27-ubuntu-djgpp-toolchain-resolute.sources
  sudo sed -i '/^URIs:/i Architectures: amd64' /etc/apt/sources.list.d/stsp-0-ubuntu-gcc-ia16-resolute.sources
fi
sudo apt update -q

sudo apt install -y \
  acl \
  comcom64 \
  cpu-checker \
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

sudo apt install -y -f \
  dj64-dbgsym \
  libdjdev64-0-dbgsym

# Install the FAT mount helper
sudo cp test/dosemu_fat_mount.sh /bin/.
sudo chown root:root /bin/dosemu_fat_mount.sh
sudo chmod 755 /bin/dosemu_fat_mount.sh

# Install the TAP helper
sudo cp test/dosemu_tap_interface.sh /bin/.
sudo chown root:root /bin/dosemu_tap_interface.sh
sudo chmod 755 /bin/dosemu_tap_interface.sh

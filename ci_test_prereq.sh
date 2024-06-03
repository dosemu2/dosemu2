#!/bin/sh

sudo add-apt-repository -y ppa:jwt27/djgpp-toolchain
sudo add-apt-repository -y ppa:tkchia/build-ia16

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

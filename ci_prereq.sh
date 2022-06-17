#!/bin/sh

sudo add-apt-repository -y ppa:dosemu2/ppa
sudo add-apt-repository -y ppa:jwt27/djgpp-toolchain
sudo add-apt-repository -y ppa:tkchia/build-ia16

sudo apt update -q

sudo apt install -y \
  acl \
  devscripts \
  equivs \
  comcom32 \
  git \
  bash \
  nasm \
  python3-cpuinfo \
  python3-pexpect \
  gcc-djgpp \
  qemu-system-common \
  gdb \
  valgrind \
  gcc-ia16-elf \
  libi86-ia16-elf \
  gcc-multilib \
  dos2unix

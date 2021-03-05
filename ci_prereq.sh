#!/bin/sh

sudo add-apt-repository ppa:dosemu2/ppa
sudo add-apt-repository ppa:jwt27/djgpp-toolchain
sudo add-apt-repository ppa:tkchia/build-ia16

sudo apt update -q

sudo apt install -y \
  acl \
  comcom32 \
  git \
  bash \
  autoconf \
  autotools-dev \
  automake \
  coreutils \
  linuxdoc-tools \
  bison \
  flex \
  gawk \
  sed \
  libbsd-dev \
  libx11-dev \
  libxext-dev \
  libslang2-dev \
  xfonts-utils \
  libgpm-dev \
  libasound2-dev \
  libsdl2-dev \
  libsdl2-ttf-dev \
  libfontconfig1-dev \
  libsdl1.2-dev \
  ladspa-sdk \
  libfluidsynth-dev \
  libao-dev \
  libieee1284-3-dev \
  libreadline-dev \
  libjson-c-dev \
  libslirp-dev \
  binutils-dev \
  pkg-config \
  clang \
  nasm \
  python3-cpuinfo \
  python3-pexpect \
  gcc-djgpp \
  qemu-system-common \
  gdb \
  valgrind \
  gcc-ia16-elf \
  gcc-multilib \
  dos2unix

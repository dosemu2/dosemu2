#!/bin/bash

sudo add-apt-repository ppa:dosemu2/ppa -y
sudo apt-get update -q
sudo apt-get install -y fdpp comcom32\
                        git bash autoconf autotools-dev automake\
                        coreutils linuxdoc-tools bison flex gawk sed\
                        libx11-dev libxext-dev libslang2-dev xfonts-utils\
                        libgpm-dev libasound2-dev libsdl2-dev\
                        libsdl1.2-dev ladspa-sdk libfluidsynth-dev\
                        libao-dev libvdeplug-dev libreadline-dev\
                        binutils-dev pkg-config\
                        python-nose python-pexpect python-pkgconfig

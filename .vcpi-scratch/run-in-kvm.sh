#!/bin/bash
# Local stand launcher, reconstructed from the one in /mnt/project-files
# (that mount went to I/O errors on 2026-09-24 07:24).
# Runs the given script inside a virtme-ng VM that has nested KVM.
[ -n "$1" ] || { echo "usage: $0 <script>"; exit 2; }
exec vng --qemu /opt/qemu-vme/bin/qemu-system-x86_64 \
  -r /boot/vmlinuz-6.8.0-139-generic \
  --disable-microvm --disable-kvm --cpus 4 --memory 6G \
  --rwdir=/tmp \
  --exec "modprobe kvm_amd; $1" \
  --qemu-opts="-cpu max"

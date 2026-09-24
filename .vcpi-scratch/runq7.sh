#!/bin/bash
# Control: the same key bursts with the keyboard fix switched off, so DOS's
# own int 9 fills the BIOS buffer that the game polls with AH=0Bh.
exec > /tmp/claude-0/runq7.log 2>&1
free_stand () { for i in $(seq 1 150); do [ "$(ps -eo cmd | grep -c '[q]emu-vme/bin/qemu-system-x86_64')" -eq 0 ] && return; sleep 10; done; echo busy; }
run () { echo "=== $1 starting $(date -Is) ==="; free_stand; sleep 15; bash /mnt/project-files/kvm-in-session/run-in-kvm.sh "$1"; echo "=== $1 done $(date -Is) ==="; }
cd /home/user/dosemu2 || exit 1
make -j"$(nproc)" > /tmp/claude-0/build-kbdswitch.log 2>&1
echo "make exit=$?"
run /tmp/sc-ctl.sh
echo "runq7 done $(date -Is)"

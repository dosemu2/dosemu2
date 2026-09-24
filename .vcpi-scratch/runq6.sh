#!/bin/bash
exec > /tmp/claude-0/runq6.log 2>&1
free_stand () { for i in $(seq 1 150); do [ "$(ps -eo cmd | grep -c '[q]emu-vme/bin/qemu-system-x86_64')" -eq 0 ] && return; sleep 10; done; echo busy; }
run () { echo "=== $1 starting $(date -Is) ==="; free_stand; sleep 15; bash /mnt/project-files/kvm-in-session/run-in-kvm.sh "$1"; echo "=== $1 done $(date -Is) ==="; }
run /tmp/sc-probe.sh
echo "runq6 done $(date -Is)"

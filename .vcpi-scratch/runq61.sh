#!/bin/bash
exec > /tmp/claude-0/runq61.log 2>&1
for i in $(seq 1 400); do
  [ "$(ps -eo cmd | grep -c '[q]emu-vme/bin/qemu-system-x86_64')" -eq 0 ] && break
  sleep 10
done
sleep 10
echo "=== sc: click PLAY, gdb on the stall $(date -Is)"
timeout 1800 bash /mnt/project-files/kvm-in-session/run-in-kvm.sh /tmp/sc-gdb2.sh
echo "=== wrapper rc=$? $(date -Is)"
echo "runq61 done $(date -Is)"

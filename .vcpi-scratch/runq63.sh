#!/bin/bash
exec > /tmp/claude-0/runq63.log 2>&1
for i in $(seq 1 400); do
  [ "$(ps -eo cmd | grep -c '[q]emu-vme/bin/qemu-system-x86_64')" -eq 0 ] && break
  sleep 10
done
sleep 20
echo "=== sc: gdb on the stall, take 2 $(date -Is)"
timeout 1800 bash /mnt/project-files/kvm-in-session/run-in-kvm.sh /tmp/sc-gdb3.sh
echo "=== wrapper rc=$? $(date -Is)"
echo "runq63 done $(date -Is)"

#!/bin/bash
exec > /tmp/claude-0/runq64.log 2>&1
# one other stand may be running; start as soon as there is room for a second
for i in $(seq 1 400); do
  [ "$(ps -eo cmd | grep -c '[q]emu-vme/bin/qemu-system-x86_64')" -le 4 ] && break
  sleep 10
done
sleep 5
echo "=== sc: gdb on the stall, take 2 $(date -Is)"
timeout 1800 bash /mnt/project-files/kvm-in-session/run-in-kvm.sh /tmp/sc-gdb3.sh
echo "=== wrapper rc=$? $(date -Is)"
echo "runq64 done $(date -Is)"

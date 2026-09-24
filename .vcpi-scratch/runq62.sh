#!/bin/bash
exec > /tmp/claude-0/runq62.log 2>&1
for i in $(seq 1 400); do
  [ "$(ps -eo cmd | grep -c '[q]emu-vme/bin/qemu-system-x86_64')" -eq 0 ] && break
  sleep 10
done
sleep 20
echo "=== sc: into the air via TRAINING MISSION $(date -Is)"
timeout 1800 bash /mnt/project-files/kvm-in-session/run-in-kvm.sh /tmp/sc-fly5.sh
echo "=== wrapper rc=$? $(date -Is)"
grep -a '^t=\|launcher\|click TRAIN\|md5 after\|dosemu exit\|^=== ' /tmp/scfly5.log | tail -60
echo "runq62 done $(date -Is)"

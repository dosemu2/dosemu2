#!/bin/bash
exec > /tmp/claude-0/runq65.log 2>&1
for i in $(seq 1 400); do
  [ "$(ps -eo cmd | grep -c '[q]emu-vme/bin/qemu-system-x86_64')" -eq 0 ] && break
  sleep 10
done
sleep 15
echo "=== building with the HMA watch $(date -Is)"
cd /home/user/dosemu2 && make -j"$(nproc)" > /tmp/claude-0/build65.log 2>&1
rc=$?
echo "build rc=$rc $(date -Is)"
[ $rc -ne 0 ] && { tail -30 /tmp/claude-0/build65.log; echo "runq65 done $(date -Is)"; exit 1; }
echo "=== sc: gdb on the stall + HMA watch $(date -Is)"
timeout 1800 bash /mnt/project-files/kvm-in-session/run-in-kvm.sh /tmp/sc-gdb3.sh
echo "=== wrapper rc=$? $(date -Is)"
echo "=== XHMA lines: $(grep -ac XHMA /tmp/claude-0/scgdb3/sc.log)"
grep -a XHMA /tmp/claude-0/scgdb3/sc.log | head -5
echo "runq65 done $(date -Is)"

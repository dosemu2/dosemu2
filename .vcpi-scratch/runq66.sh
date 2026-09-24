#!/bin/bash
exec > /tmp/claude-0/runq66.log 2>&1
for i in $(seq 1 400); do
  [ "$(ps -eo cmd | grep -c '[q]emu-vme/bin/qemu-system-x86_64')" -eq 0 ] && break
  sleep 10
done
sleep 5
echo "=== sc: gdb on the stall + HMA watch $(date -Is)"
timeout 1800 bash /tmp/claude-0/run-in-kvm.sh /tmp/sc-gdb3.sh
echo "=== wrapper rc=$? $(date -Is)"
echo "=== XHMA lines: $(grep -ac XHMA /tmp/claude-0/scgdb3/sc.log 2>/dev/null)"
grep -a XHMA /tmp/claude-0/scgdb3/sc.log 2>/dev/null | head -5
echo "runq66 done $(date -Is)"

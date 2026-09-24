#!/bin/bash
exec > /tmp/claude-0/runq69.log 2>&1
# run alongside the one stand that is already up
for i in $(seq 1 400); do
  [ "$(ps -eo cmd | grep -c '[q]emu-vme/bin/qemu-system-x86_64')" -le 4 ] && break
  sleep 10
done
sleep 5
echo "=== sc with DOS=HIGH, watching FFFF:0010 $(date -Is)"
timeout 1800 bash /tmp/claude-0/run-in-kvm.sh /tmp/sc-hma.sh
echo "=== wrapper rc=$? $(date -Is)"
L=/tmp/claude-0/schma/sc.log
echo "=== XHMA lines: $(grep -ac XHMA $L 2>/dev/null)"
grep -a XHMA $L 2>/dev/null | head -6
echo "=== screen trace"; grep -a '^t=' /tmp/schma.log 2>/dev/null | tail -8
echo "runq69 done $(date -Is)"

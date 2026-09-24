#!/bin/bash
exec > /tmp/claude-0/runq68.log 2>&1
# wait for runq67's rebuild to finish, then for room for a second stand
for i in $(seq 1 400); do
  grep -q "build rc=" /tmp/claude-0/runq67.log 2>/dev/null && break
  sleep 10
done
grep -q "build rc=0" /tmp/claude-0/runq67.log || { echo "build failed, not running"; echo "runq68 done $(date -Is)"; exit 1; }
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
echo "=== XPR totals:"; grep -ao 'XPR [a-z0-9 -]*' $L 2>/dev/null | sort | uniq -c | sort -rn | head -5
echo "runq68 done $(date -Is)"

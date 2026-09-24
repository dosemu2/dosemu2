#!/bin/bash
exec > /tmp/claude-0/runq67.log 2>&1
for i in $(seq 1 400); do
  [ "$(ps -eo cmd | grep -c '[q]emu-vme/bin/qemu-system-x86_64')" -eq 0 ] && break
  sleep 10
done
sleep 10
echo "=== building with the kvm_post_run counters $(date -Is)"
cd /home/user/dosemu2 && make -j"$(nproc)" > /tmp/claude-0/build67.log 2>&1
rc=$?
echo "build rc=$rc $(date -Is)"
[ $rc -ne 0 ] && { grep -n "error:" /tmp/claude-0/build67.log | head -20; echo "runq67 done $(date -Is)"; exit 1; }
echo "=== sc: who keeps kvm_post_run returning 0 $(date -Is)"
timeout 1800 bash /tmp/claude-0/run-in-kvm.sh /tmp/sc-gdb3.sh
echo "=== wrapper rc=$? $(date -Is)"
L=/tmp/claude-0/scgdb3/sc.log
echo "=== XPR totals:"; grep -ao 'XPR [a-z0-9 -]*' $L 2>/dev/null | sort | uniq -c | sort -rn | head
echo "=== last XPR lines:"; grep -a XPR $L 2>/dev/null | tail -4
echo "=== XHMA: $(grep -ac XHMA $L 2>/dev/null)"
echo "runq67 done $(date -Is)"

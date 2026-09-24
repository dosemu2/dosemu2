#!/bin/bash
exec >> /tmp/claude-0/runq70.log 2>&1
echo "=== sc: DOS=HIGH + XHMA + XPR $(date -Is)"
bash /tmp/claude-0/run-in-kvm.sh /tmp/sc-hma.sh
echo "=== runq70 done $(date -Is)"
tail -40 /tmp/schma.log

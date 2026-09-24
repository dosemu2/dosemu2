#!/bin/bash
exec >> /tmp/claude-0/runq72.log 2>&1
echo "=== sc: cursor grid onto the menu $(date -Is)"
bash /tmp/claude-0/run-in-kvm.sh /tmp/sc-fly7.sh
echo "=== runq72 done $(date -Is)"
grep -a 'launcher\|attempt\|after \|grid done\|=== ' /tmp/scfly7.log | tail -60

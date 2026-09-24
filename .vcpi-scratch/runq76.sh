#!/bin/bash
exec >> /tmp/claude-0/runq76.log 2>&1
echo "=== sc: mouse against the death dialog $(date -Is)"
bash /tmp/claude-0/run-in-kvm.sh /tmp/sc-fly10.sh
echo "=== runq76 done $(date -Is)"
grep -a 'death dialog\|round \|=== ' /tmp/scfly10.log | tail -40

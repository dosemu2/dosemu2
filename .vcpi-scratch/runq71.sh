#!/bin/bash
exec >> /tmp/claude-0/runq71.log 2>&1
echo "=== sc: sweep onto TRAINING MISSION $(date -Is)"
bash /tmp/claude-0/run-in-kvm.sh /tmp/sc-fly6.sh
echo "=== runq71 done $(date -Is)"
tail -40 /tmp/scfly6.log

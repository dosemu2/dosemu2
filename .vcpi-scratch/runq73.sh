#!/bin/bash
exec >> /tmp/claude-0/runq73.log 2>&1
echo "=== sc: menu-aware TRAINING MISSION $(date -Is)"
bash /tmp/claude-0/run-in-kvm.sh /tmp/sc-fly8.sh
echo "=== runq73 done $(date -Is)"
grep -a 'launcher\|menu up\|still on\|menu gone\|TOOK\|=== ' /tmp/scfly8.log | tail -60

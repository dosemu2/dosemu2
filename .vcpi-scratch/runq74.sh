#!/bin/bash
exec >> /tmp/claude-0/runq74.log 2>&1
echo "=== sc: fly through the stub, HMA occupied $(date -Is)"
bash /tmp/claude-0/run-in-kvm.sh /tmp/sc-fly9.sh
echo "=== runq74 done $(date -Is)"
tail -30 /tmp/scfly9.log

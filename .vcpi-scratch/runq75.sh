#!/bin/bash
exec >> /tmp/claude-0/runq75.log 2>&1
echo "=== sc: does the mouse reach the client $(date -Is)"
bash /tmp/claude-0/run-in-kvm.sh /tmp/sc-mdbg.sh
echo "=== runq75 done $(date -Is)"
tail -60 /tmp/scmdbg.log

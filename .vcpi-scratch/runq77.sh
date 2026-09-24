#!/bin/bash
exec >> /tmp/claude-0/runq77.log 2>&1
echo "=== sc: catch the wedge with the full dump $(date -Is)"
bash /tmp/claude-0/run-in-kvm.sh /tmp/sc-fly10.sh
echo "=== runq77 done $(date -Is)"
grep -a 'death dialog\|round \|=== ' /tmp/scfly10.log | tail -40

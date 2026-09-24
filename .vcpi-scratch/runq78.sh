#!/bin/bash
exec >> /tmp/claude-0/runq78.log 2>&1
echo "=== sc: into the air through the real menu $(date -Is)"
bash /tmp/claude-0/run-in-kvm.sh /tmp/sc-fly11.sh
echo "=== runq78 done $(date -Is)"
grep -a 'launcher\|menu up\|menu closed\|off the menu\|YOU HAVE DIED\|=== ' /tmp/scfly11.log | tail -40

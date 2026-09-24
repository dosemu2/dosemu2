#!/bin/bash
# Find out what the guest's mouse driver actually sees.  PROBE6 samples int33
# AX=3 ten times a second for ninety seconds; the harness injects known motions
# on a fixed schedule, so the recording says how X input turns into guest
# coordinates.
exec > /tmp/claude-0/mcal.log 2>&1
export DOSEMU2_COMCOM_DIR=/usr/local/share/comcom32
D=/home/user/dosemu2/bin/dosemu
O=/tmp/claude-0/mcal
rm -rf $O; mkdir -p $O
cat > $O/rc <<'RC'
$_cpu_vm = "emulated"
$_cpu_vm_dpmi = "emulated"
$_X_mitshm = (off)
RC
mkdir -p $O/c
cp /tmp/claude-0/probe6dir/PROBE6.COM $O/c/
printf 'PROBE6.COM\r\n' > $O/c/PM.BAT
rm -f /tmp/.X9-lock
Xvfb :9 -screen 0 1024x768x24 >$O/xvfb.log 2>&1 &
XP=$!
for i in $(seq 1 30); do [ -e /tmp/.X11-unix/X9 ] && break; sleep 1; done
export DISPLAY=:9
timeout 200 $D -s -X -f $O/rc -o $O/d.log -K $O/c -E "PM.BAT" </dev/null > $O/d.out 2>&1 &
DP=$!
sleep 18
echo "--- t=20 fifty steps right"
/tmp/claude-0/xnud :9 400 0
sleep 6
echo "--- t=30 fifty steps down"
/tmp/claude-0/xnud :9 0 50
sleep 6
echo "--- t=40 absolute 500,300"
/tmp/claude-0/xclick :9 500 300
sleep 6
echo "--- t=50 absolute 100,50"
/tmp/claude-0/xclick :9 100 50
sleep 6
echo "--- t=60 corner"
/tmp/claude-0/xnud :9 -1800 -1800
sleep 6
echo "--- t=70 twentyfive steps right"
/tmp/claude-0/xnud :9 200 0
sleep 6
echo "--- t=80 twentyfive steps down"
/tmp/claude-0/xnud :9 0 200
sleep 30
wait $DP; echo "dosemu exit=$?"
ls -la $O/c/
kill $XP 2>/dev/null
echo done

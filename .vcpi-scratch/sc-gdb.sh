#!/bin/bash
# Catch the wedge: when the guest stops making VCPI switches, take a full
# backtrace of every dosemu thread, three times, so we see what the main
# thread is stuck on.
exec > /tmp/scgdb.log 2>&1
export DOSEMU2_COMCOM_DIR=/usr/local/share/comcom32
D=/home/user/dosemu2/bin/dosemu
O=/tmp/claude-0/scgdb
rm -rf $O; mkdir -p $O
cat > $O/rc <<'RC'
$_cpu_vm = "kvm"
$_cpu_vm_dpmi = "kvm"
$_ems = (8192)
$_jemm = (off)
$_vcpi = (on)
$_ext_mem = (6144)
$_ems_frame = (0xe000)
$_ems_conv_pages = (0)
$_X_mitshm = (off)
$_sound = (off)
RC
mount -t tmpfs none /tmp/.X11-unix 2>/dev/null
rm -f /tmp/.X7-lock
Xvfb :7 -screen 0 1024x768x24 >/tmp/xvfb7.log 2>&1 &
XP=$!
for i in $(seq 1 30); do [ -e /tmp/.X11-unix/X7 ] && break; sleep 1; done
export DISPLAY=:7
cd /tmp/claude-0/sc
cp -f /tmp/claude-0/sc-pristine-cfg/SC.CFG /tmp/claude-0/sc-pristine-cfg/SCTO1.CFG . 2>/dev/null
for c in SC.CFG SCTO1.CFG; do grep -a '^=' $c > $c.tmp; printf '\r\n' >> $c.tmp; mv -f $c.tmp $c; done
for g in OPTTEST MKTERR STRIKE; do cp -f $g.SAV $g.EXE; done
cp -f MENU.SAV MENU.EXE
rm -f ranm.txt RANM.TXT ranx.txt RANX.TXT
touch -a -d '2020-01-01 00:00' *.EXE *.OVL
printf 'SCCD.EXE\r\n' > PD.BAT
timeout 900 $D -s -X -f $O/rc -o $O/sc.log -D+E -K . -E "PD.BAT" </dev/null > $O/sc.out 2>&1 &
DP=$!
sleep 5
PID=$(ps -eo pid,cmd | grep '[d]osemu2/bin/dosemu' | awk '{print $1}' | head -1)
echo "dosemu pid=$PID (wrapper $DP)"
cnt () { grep -ac 'VCPI: switch to PM' $O/sc.log 2>/dev/null; }
last=-1; same=0; dumped=0
while [ $SECONDS -lt 870 ]; do
  sleep 15
  kill -0 $DP 2>/dev/null || { echo "dosemu gone at ${SECONDS}s"; break; }
  c=$(cnt)
  if [ "$c" = "$last" ]; then same=$((same+1)); else same=0; fi
  last=$c
  echo "t=${SECONDS}s de0c=$c same=$same ovf=$(grep -ac 'callback queue overflow' $O/sc.log)"
  if [ $same -ge 4 ] && [ $dumped -lt 3 ] && [ "$c" -gt 100 ]; then
    dumped=$((dumped+1))
    echo "=== WEDGED, backtrace #$dumped at ${SECONDS}s ==="
    timeout 120 gdb -p "$PID" -batch \
      -ex "set pagination off" \
      -ex "thread apply all bt 25" 2>&1 | sed 's/^/  /'
    echo "=== end backtrace #$dumped ==="
    same=0
  fi
done
kill $DP 2>/dev/null; kill $XP 2>/dev/null
echo "=== de0c: $(cnt)"
echo "=== pic overflow: $(grep -ac 'callback queue overflow' $O/sc.log)"
echo "=== tail of log:"; grep -av 'callback queue overflow' $O/sc.log | tail -12

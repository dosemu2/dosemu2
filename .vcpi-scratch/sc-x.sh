#!/bin/bash
exec > /tmp/scx.log 2>&1
export DOSEMU2_COMCOM_DIR=/usr/local/share/comcom32
D=/home/user/dosemu2/bin/dosemu
O=/tmp/claude-0/sckvm
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
RC
mount -t tmpfs none /tmp/.X11-unix || echo "tmpfs mount failed"
rm -f /tmp/.X7-lock
Xvfb :7 -screen 0 1024x768x24 >/tmp/xvfb7.log 2>&1 &
XP=$!
for i in $(seq 1 30); do [ -e /tmp/.X11-unix/X7 ] && break; sleep 1; done
export DISPLAY=:7
cd /tmp/claude-0/sc
cp -f /tmp/claude-0/sc-pristine-cfg/SC.CFG /tmp/claude-0/sc-pristine-cfg/SCTO1.CFG . 2>/dev/null
timeout 900 $D -s -X -f $O/rc -o $O/sc.log -D+E -K . -E "GO.BAT" </dev/null > $O/sc.out 2>&1 &
DP=$!
shot () {
  local n=$1 id
  id=$(xwininfo -root -children 2>/dev/null | grep -oE '0x[0-9a-f]+' | sed -n 2p)
  [ -n "$id" ] && xwd -id "$id" -silent > $O/win-$n.xwd 2>/dev/null
  echo "shot $n id=$id de0c=$(grep -ac 'VCPI: switch to PM' $O/sc.log)"
}
for t in 60 75 90 105 120 140 160 190 220 260 300 360 420 500 600 720 870; do
  while [ $SECONDS -lt $t ]; do sleep 3; done
  kill -0 $DP 2>/dev/null || break
  shot $t
  n=$(grep -ac 'VCPI: switch to PM' $O/sc.log)
  if [ $t -ge 150 ] && [ "$n" = "0" ]; then
    echo "ABORT: no VCPI switch by ${t}s, the stand is wedged"
    kill $DP 2>/dev/null; break
  fi
  if [ "$n" = "$prev_n" ]; then stall=$((stall+1)); else stall=0; fi
  prev_n=$n
  if [ $stall -ge 6 ]; then
    echo "ABORT: switch count stuck at $n for six samples"
    kill $DP 2>/dev/null; break
  fi
done
wait $DP; echo "dosemu exit=$?"
echo "=== de0c ==="; grep -ac "VCPI: switch to PM" $O/sc.log
echo "=== mcb ==="; grep -ac "MCB corruption" $O/sc.log
echo "=== fatal ==="; grep -a "leavedos\|TRIPLE\|SHUTDOWN\|fatal" $O/sc.log | tail -5
kill $XP 2>/dev/null

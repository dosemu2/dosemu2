#!/bin/bash
# The cockpit is up on the runway and the frame never changes because the
# aircraft is parked: nothing so far has pushed the throttle.  Drive it.
exec > /tmp/scfly.log 2>&1
export DOSEMU2_COMCOM_DIR=/usr/local/share/comcom32
D=/home/user/dosemu2/bin/dosemu
O=/tmp/claude-0/scfly
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
cp -f MD-MENU.COM MENU.EXE
rm -f ranm.txt RANM.TXT ranx.txt RANX.TXT
touch -a -d '2020-01-01 00:00' *.EXE *.OVL
printf 'SCCD.EXE\r\n' > PD.BAT
timeout 900 $D -s -X -f $O/rc -o $O/sc.log -D+E -K . -E "PD.BAT" </dev/null > $O/sc.out 2>&1 &
DP=$!
grab () {
  local id
  id=$(xwininfo -root -children 2>/dev/null | grep -oE '0x[0-9a-f]+' | sed -n 2p)
  [ -n "$id" ] && xwd -id "$id" -silent > $O/cur.xwd 2>/dev/null
  md5sum $O/cur.xwd 2>/dev/null | cut -c1-12
}
shot () {
  local m
  m=$(grab)
  cp -f $O/cur.xwd $O/f-$1-$SECONDS.xwd 2>/dev/null
  echo "  shot $1 t=${SECONDS}s md5=$m"
}
keys () { /tmp/claude-0/xkey2 :7 "$@" 2>&1 | sed 's/^/  xkey: /'; }
wait_to () { while [ $SECONDS -lt $1 ]; do sleep 2; done; }

# let the intro run and the cockpit come up
for t in 120 200 260 300 320; do
  wait_to $t
  kill -0 $DP 2>/dev/null || { echo "dosemu gone before ${t}s"; break; }
  shot idle
done

echo "=== flying it now"
for step in "KP_Add:80:20 throttle-keypad" \
            "plus:80:20 throttle-plus" \
            "equal:80:20 throttle-equal" \
            "Tab:5000 afterburner" \
            "Down:2000 stick-down" \
            "Up:2000 stick-up" \
            "b:200:3 brakes" \
            "g:200 gear" \
            "1:200 view1" ; do
  set -- $step
  kill -0 $DP 2>/dev/null || { echo "dosemu gone at ${SECONDS}s"; break; }
  echo "--- $2 ($1)"
  keys "$1"
  sleep 4
  shot "$2"
done

while [ $SECONDS -lt 860 ]; do
  kill -0 $DP 2>/dev/null || { echo "dosemu gone at ${SECONDS}s"; break; }
  keys "KP_Add:80:5" "plus:80:5" "Tab:3000"
  shot tail
  sleep 8
done
wait $DP; echo "dosemu exit=$?"
echo "=== de0c: $(grep -ac 'VCPI: switch to PM' $O/sc.log)"
echo "=== faults: $(grep -ac 'exit_reason = 0x8' $O/sc.log)"
kill $XP 2>/dev/null

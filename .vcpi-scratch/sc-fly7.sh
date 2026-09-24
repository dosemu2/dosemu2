#!/bin/bash
# The guest's cursor follows relative motion, not the X pointer, so the only
# way to put it on a menu row is to park it in the corner and walk it out by a
# counted number of steps.  The step-to-pixel ratio is unknown, so walk a fixed
# distance across and sweep the vertical count; every attempt is photographed,
# which gives the calibration even when it misses.
exec > /tmp/scfly7.log 2>&1
export DOSEMU2_COMCOM_DIR=/usr/local/share/comcom32
D=/home/user/dosemu2/bin/dosemu
O=/tmp/claude-0/scfly7
MENU=cbdb02e32e1e
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
cp -f MENU.SAV MENU.EXE
rm -f ranm.txt RANM.TXT ranx.txt RANX.TXT
touch -a -d '2020-01-01 00:00' *.EXE *.OVL
printf 'SCCD.EXE\r\n' > PD.BAT
timeout 1750 $D -s -X -f $O/rc -o $O/sc.log -D+E -K . -E "PD.BAT" </dev/null > $O/sc.out 2>&1 &
DP=$!
grab () {
  local id
  id=$(xwininfo -root -children 2>/dev/null | grep -oE '0x[0-9a-f]+' | sed -n 2p)
  [ -n "$id" ] && xwd -id "$id" -silent > $O/cur.xwd 2>/dev/null
  md5sum $O/cur.xwd 2>/dev/null | cut -c1-12
}
shot () { cp -f $O/cur.xwd "$1" 2>/dev/null; }
played=0; t0=0; n=0
NYS="4 8 12 16 20 24 28 32 36 40 44 48 22 26 30 18 14 34"
while [ $SECONDS -lt 1720 ]; do
  kill -0 $DP 2>/dev/null || { echo "dosemu gone at ${SECONDS}s"; break; }
  m=$(grab)
  if [ "$m" = "$MENU" ] && [ $played -eq 0 ]; then
    shot $O/launcher.xwd
    echo "launcher at ${SECONDS}s, PLAY"
    /tmp/claude-0/xsweep :7 348 88 2>&1 | sed 's/^/  /'
    played=1; t0=$SECONDS
  fi
  if [ $played -eq 1 ]; then
    d=$((SECONDS - t0))
    if [ $d -lt 360 ]; then
      /tmp/claude-0/xkey :7 Escape >/dev/null 2>&1
    else
      for ny in $NYS; do
        kill -0 $DP 2>/dev/null || break
        n=$((n+1))
        grab >/dev/null; shot $O/a$(printf %02d $n)-ny$ny-pre.xwd
        echo "attempt $n at ${SECONDS}s: nx=20 ny=$ny"
        /tmp/claude-0/xrel :7 20 $ny 3 2>&1 | sed 's/^/    /'
        sleep 3
        a=$(grab); shot $O/a$(printf %02d $n)-ny$ny-post.xwd
        echo "    after $a"
        sleep 12
      done
      echo "grid done at ${SECONDS}s, watching"
      while [ $SECONDS -lt 1720 ] && kill -0 $DP 2>/dev/null; do
        n=$((n+1)); grab >/dev/null; shot $O/w$(printf %02d $n)-${SECONDS}s.xwd
        echo "w t=${SECONDS}s md5=$(md5sum $O/cur.xwd | cut -c1-12)"
        sleep 20
      done
      break
    fi
  fi
  if [ $((SECONDS % 40)) -lt 4 ]; then
    echo "t=${SECONDS}s de0c=$(grep -ac 'VCPI: switch to PM' $O/sc.log) md5=$m"
  fi
  sleep 4
done
wait $DP; echo "dosemu exit=$?"
echo "=== de0c: $(grep -ac 'VCPI: switch to PM' $O/sc.log)"
echo "=== XHMA: $(grep -ac XHMA $O/sc.log)"
kill $XP 2>/dev/null
for f in $O/*.xwd; do
  [ -e "$f" ] || continue
  xwdtopnm < "$f" 2>/dev/null | pnmtopng > "${f%.xwd}.png" 2>/dev/null
done
rm -f $O/*.xwd
echo "done"

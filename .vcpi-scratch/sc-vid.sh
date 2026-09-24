#!/bin/bash
# The menu frame is byte-identical between runs (md5 cbdb02e32e1e), so watch
# for it and click "PLAY STRIKE COMMANDER" the moment it is on screen.
exec > /tmp/scvid.log 2>&1
export DOSEMU2_COMCOM_DIR=/usr/local/share/comcom32
D=/home/user/dosemu2/bin/dosemu
O=/tmp/claude-0/scvid
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
timeout 900 $D -s -X -f $O/rc -o $O/sc.log -D+Ev -K . -E "PD.BAT" </dev/null > $O/sc.out 2>&1 &
DP=$!
grab () {
  local id
  id=$(xwininfo -root -children 2>/dev/null | grep -oE '0x[0-9a-f]+' | sed -n 2p)
  [ -n "$id" ] && xwd -id "$id" -silent > $O/cur.xwd 2>/dev/null
  md5sum $O/cur.xwd 2>/dev/null | cut -c1-12
}
clicked=0
while [ $SECONDS -lt 870 ]; do
  kill -0 $DP 2>/dev/null || { echo "dosemu gone at ${SECONDS}s"; break; }
  m=$(grab)
  if [ "$m" = "$MENU" ] && [ $clicked -lt 6 ]; then
    cp -f $O/cur.xwd $O/menu-$SECONDS.xwd
    echo "MENU on screen at ${SECONDS}s, clicking"
    /tmp/claude-0/xclick :7 348 88 2>&1 | sed 's/^/  /'
    clicked=$((clicked+1))
    sleep 2
    cp -f $O/cur.xwd $O/pre-$SECONDS.xwd 2>/dev/null
    for k in 1 2 3 4 5 6 7 8 9 10; do
      sleep 3
      cp -f $O/cur.xwd $O/x.xwd 2>/dev/null
      echo "  after+$((k*3))s de0c=$(grep -ac 'VCPI: switch to PM' $O/sc.log) md5=$(grab)"
      cp -f $O/cur.xwd $O/after-$SECONDS-$k.xwd
    done
  fi
  [ $((SECONDS % 20)) -lt 2 ] && echo "t=${SECONDS}s de0c=$(grep -ac 'VCPI: switch to PM' $O/sc.log) md5=$m"
  sleep 2
done
wait $DP; echo "dosemu exit=$?"
echo "=== de0c: $(grep -ac 'VCPI: switch to PM' $O/sc.log)"
echo "=== faults: $(grep -ac 'exit_reason = 0x8' $O/sc.log)"
echo "=== pic overflow: $(grep -ac 'callback queue overflow' $O/sc.log)"
kill $XP 2>/dev/null
for f in $O/*.xwd; do
  [ -e "$f" ] || continue
  xwdtopnm < "$f" 2>/dev/null | pnmtopng > "${f%.xwd}.png" 2>/dev/null
done

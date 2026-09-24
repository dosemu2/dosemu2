#!/bin/bash
# Decide whether the mouse reaches the game under the VCPI monitor.  The
# "YOU HAVE DIED" box is a three button dialog that comes up by itself a few
# minutes after take-off, so it is a far better target than the attract loop's
# menu: it stays on screen and it is the only thing the game is waiting on.
# The mouse log is on, the VCPI log is off, so the journal stays small.
exec > /tmp/scfly10.log 2>&1
export DOSEMU2_COMCOM_DIR=/usr/local/share/comcom32
D=/home/user/dosemu2/bin/dosemu
O=/tmp/claude-0/scfly10
DIED=c85415b100083b3f40d23d451742dc5a
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
timeout 2000 $D -s -X -f $O/rc -o $O/sc.log -D+m -K . -E "PD.BAT" </dev/null > $O/sc.out 2>&1 &
DP=$!
grab () {
  local id
  id=$(xwininfo -root -children 2>/dev/null | grep -oE '0x[0-9a-f]+' | sed -n 2p)
  [ -n "$id" ] && xwd -id "$id" -silent > $O/cur.xwd 2>/dev/null
  md5sum $O/cur.xwd 2>/dev/null | cut -c1-12
}
dlgfp () {
  xwdtopnm < $O/cur.xwd 2>/dev/null | pamcut -left 150 -top 95 -width 340 -height 30 2>/dev/null | md5sum | cut -d' ' -f1
}
mcnt () { grep -ac MOUSE $O/sc.log; }
keys () { /tmp/claude-0/xkey2 :7 "$@" >/dev/null 2>&1; }
n=0; phase=fly; round=0
while [ $SECONDS -lt 1950 ]; do
  kill -0 $DP 2>/dev/null || { echo "dosemu gone at ${SECONDS}s"; break; }
  m=$(grab)
  if [ "$phase" = "fly" ]; then
    [ $SECONDS -gt 300 ] && { keys "plus:80:10"; keys "Tab:3000:1"; keys "Down:1200:1"; keys "Up:1200:1"; }
    if [ "$(dlgfp)" = "$DIED" ]; then
      phase=dialog
      echo "=== death dialog up at ${SECONDS}s, mouse lines so far $(mcnt)"
      cp -f $O/cur.xwd $O/dialog.xwd
      continue
    fi
    n=$((n+1))
    cp -f $O/cur.xwd $O/f$(printf %03d $n)-${SECONDS}s.xwd 2>/dev/null
    echo "fly t=${SECONDS}s md5=$m mouse=$(mcnt)"
    sleep 25
  else
    round=$((round+1))
    a=$(mcnt)
    case $round in
      1) what="move only";      /tmp/claude-0/xnud :7 400 200 ;;
      2) what="click CONTINUE"; /tmp/claude-0/xclick :7 465 170 ;;
      3) what="click CONTINUE"; /tmp/claude-0/xclick :7 465 170 ;;
      4) what="sweep CONTINUE"; /tmp/claude-0/xsweep :7 465 170 ;;
      5) what="click RESTART";  /tmp/claude-0/xclick :7 175 170 ;;
      6) what="key Return";     /tmp/claude-0/xkey :7 Return ;;
      7) what="key c";          /tmp/claude-0/xkey :7 c ;;
      *) what="idle" ;;
    esac
    sleep 3
    b=$(mcnt); grab >/dev/null; f=$(dlgfp)
    cp -f $O/cur.xwd $O/d$(printf %02d $round)-${SECONDS}s.xwd 2>/dev/null
    if [ "$f" = "$DIED" ]; then st="dialog still up"; else st="DIALOG GONE"; fi
    echo "round $round ($what) at ${SECONDS}s: mouse $a -> $b (delta $((b-a))), $st"
    [ "$f" != "$DIED" ] && { echo "=== the dialog answered on round $round"; sleep 20; }
    [ $round -ge 9 ] && break
    sleep 12
  fi
done
echo "=== mouse lines total: $(mcnt)"
echo "=== what the mouse code said while the dialog was up:"
grep -a MOUSE $O/sc.log | tail -60
kill -TERM $DP 2>/dev/null
sleep 3
kill $XP 2>/dev/null
for f in $O/*.xwd; do
  [ -e "$f" ] || continue
  xwdtopnm < "$f" 2>/dev/null | pnmtopng > "${f%.xwd}.png" 2>/dev/null
done
rm -f $O/*.xwd
echo "done"

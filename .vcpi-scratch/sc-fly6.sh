#!/bin/bash
# Get into the air: click PLAY on the launcher, Escape through the intro, then
# press TRAINING MISSION on the game's own menu.  A plain jump-and-click is
# missed, so the pointer is swept in from the corner in small steps first
# (xsweep); every other attempt uses the keyboard instead, in case the menu
# does not take the mouse at all.
exec > /tmp/scfly6.log 2>&1
export DOSEMU2_COMCOM_DIR=/usr/local/share/comcom32
D=/home/user/dosemu2/bin/dosemu
O=/tmp/claude-0/scfly6
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
timeout 1650 $D -s -X -f $O/rc -o $O/sc.log -D+E -K . -E "PD.BAT" </dev/null > $O/sc.out 2>&1 &
DP=$!
grab () {
  local id
  id=$(xwininfo -root -children 2>/dev/null | grep -oE '0x[0-9a-f]+' | sed -n 2p)
  [ -n "$id" ] && xwd -id "$id" -silent > $O/cur.xwd 2>/dev/null
  md5sum $O/cur.xwd 2>/dev/null | cut -c1-12
}
opens () { grep -ac 'open file' $O/sc.log; }
clicked=0; t0=0; shot=0; lastclick=0; try=0
while [ $SECONDS -lt 1620 ]; do
  kill -0 $DP 2>/dev/null || { echo "dosemu gone at ${SECONDS}s"; break; }
  m=$(grab)
  if [ "$m" = "$MENU" ] && [ $clicked -eq 0 ]; then
    cp -f $O/cur.xwd $O/launcher.xwd
    echo "launcher menu at ${SECONDS}s, sweeping onto PLAY"
    /tmp/claude-0/xsweep :7 348 88 2>&1 | sed 's/^/  /'
    clicked=1; t0=$SECONDS
  fi
  if [ $clicked -eq 1 ]; then
    d=$((SECONDS - t0))
    if [ $d -lt 180 ]; then
      /tmp/claude-0/xkey :7 Escape >/dev/null 2>&1
    elif [ $((SECONDS - lastclick)) -ge 25 ]; then
      lastclick=$SECONDS; try=$((try+1))
      cp -f $O/cur.xwd $O/pre-${SECONDS}s.xwd 2>/dev/null
      if [ $((try % 2)) -eq 1 ]; then
        echo "try $try at ${SECONDS}s: sweep onto TRAINING MISSION (md5 $m)"
        /tmp/claude-0/xsweep :7 325 185 2>&1 | sed 's/^/    /'
      else
        echo "try $try at ${SECONDS}s: keyboard Down Down Down Return (md5 $m)"
        for k in Down Down Down; do /tmp/claude-0/xkey :7 $k >/dev/null 2>&1; sleep 1; done
        /tmp/claude-0/xkey :7 Return >/dev/null 2>&1
      fi
      sleep 4
      echo "    md5 after $(grab), opens=$(opens)"
      cp -f $O/cur.xwd $O/post-${SECONDS}s.xwd 2>/dev/null
    fi
  fi
  if [ $((SECONDS % 20)) -lt 4 ]; then
    shot=$((shot+1))
    cp -f $O/cur.xwd $O/f$(printf %03d $shot)-${SECONDS}s.xwd 2>/dev/null
    echo "t=${SECONDS}s de0c=$(grep -ac 'VCPI: switch to PM' $O/sc.log) md5=$m"
  fi
  sleep 4
done
wait $DP; echo "dosemu exit=$?"
echo "=== de0c: $(grep -ac 'VCPI: switch to PM' $O/sc.log)"
echo "=== XHMA: $(grep -ac XHMA $O/sc.log)"
grep -a XHMA $O/sc.log | head -10
echo "=== XPR:"
grep -ao 'XPR [a-z0-9 -]*' $O/sc.log | sed 's/ [0-9]*$//' | sort | uniq -c | sort -rn | head
kill $XP 2>/dev/null
for f in $O/*.xwd; do
  [ -e "$f" ] || continue
  xwdtopnm < "$f" 2>/dev/null | pnmtopng > "${f%.xwd}.png" 2>/dev/null
done
rm -f $O/*.xwd
echo "done"

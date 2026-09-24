#!/bin/bash
# Into the air through the game's own menu.  What was missing before was not
# the coordinates and not the mouse: a plain absolute click does reach the
# game (it answered the death dialog on the first try).  What was missing was
# waiting for the menu to actually be on screen -- it is only up for part of
# the attract cycle.  So: watch for the menu's fingerprint, click TRAINING
# MISSION, then fly.
exec > /tmp/scfly11.log 2>&1
export DOSEMU2_COMCOM_DIR=/usr/local/share/comcom32
D=/home/user/dosemu2/bin/dosemu
O=/tmp/claude-0/scfly11
LAUNCH=cbdb02e32e1e
GMENU=7d19864d329d9f9da191cf6578df5426
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
cp -f MENU.SAV MENU.EXE
rm -f ranm.txt RANM.TXT ranx.txt RANX.TXT
touch -a -d '2020-01-01 00:00' *.EXE *.OVL
printf 'SCCD.EXE\r\n' > PD.BAT
timeout 2600 $D -s -X -f $O/rc -o $O/sc.log -D+E -K . -E "PD.BAT" </dev/null > $O/sc.out 2>&1 &
DP=$!
grab () {
  local id
  id=$(xwininfo -root -children 2>/dev/null | grep -oE '0x[0-9a-f]+' | sed -n 2p)
  [ -n "$id" ] && xwd -id "$id" -silent > $O/cur.xwd 2>/dev/null
  md5sum $O/cur.xwd 2>/dev/null | cut -c1-12
}
fp () {
  xwdtopnm < $O/cur.xwd 2>/dev/null | pamcut -left "$1" -top "$2" -width "$3" -height "$4" 2>/dev/null | md5sum | cut -d' ' -f1
}
cnt () { grep -ac 'VCPI: switch to PM' $O/sc.log; }
keys () { /tmp/claude-0/xkey2 :7 "$@" >/dev/null 2>&1; }
played=0; t0=0; n=0; took=0; tries=0
while [ $SECONDS -lt 2550 ]; do
  kill -0 $DP 2>/dev/null || { echo "dosemu gone at ${SECONDS}s"; break; }
  m=$(grab)
  if [ $played -eq 0 ]; then
    if [ "$m" = "$LAUNCH" ]; then
      echo "launcher at ${SECONDS}s, click PLAY"
      cp -f $O/cur.xwd $O/launcher.xwd
      /tmp/claude-0/xclick :7 348 88 >/dev/null 2>&1
      played=1; t0=$SECONDS
    elif [ $SECONDS -gt 200 ]; then
      # the launcher screen is short and easy to miss, and some runs go
      # straight into the intro; after this long the game is running anyway
      echo "no launcher by ${SECONDS}s, carrying on as if PLAY was pressed"
      played=1; t0=$SECONDS
    fi
    sleep 3; continue
  fi
  if [ $took -eq 0 ]; then
    if [ $((SECONDS - t0)) -lt 240 ]; then
      /tmp/claude-0/xkey :7 Escape >/dev/null 2>&1
      sleep 3; continue
    fi
    if [ "$(fp 95 45 460 30)" = "$GMENU" ]; then
      tries=$((tries+1))
      echo "menu up at ${SECONDS}s, try $tries: click TRAINING MISSION"
      cp -f $O/cur.xwd $O/menu-$tries.xwd
      /tmp/claude-0/xclick :7 325 185 >/dev/null 2>&1
      sleep 6
      grab >/dev/null
      cp -f $O/cur.xwd $O/after-$tries.xwd
      if [ "$(fp 95 45 460 30)" != "$GMENU" ]; then
        echo "  menu closed, watching what comes up"
        for k in 1 2 3 4 5 6; do
          sleep 10; grab >/dev/null
          cp -f $O/cur.xwd $O/w$tries-$k.xwd
          echo "    +$((k*10))s md5=$(md5sum $O/cur.xwd | cut -c1-12)"
        done
        grab >/dev/null
        if [ "$(fp 95 45 460 30)" != "$GMENU" ]; then
          took=1; th=$SECONDS
          echo "=== off the menu at ${SECONDS}s after $tries tries"
        else
          echo "  attract loop came back, trying again"
        fi
      else
        echo "  still on the menu"
        sleep 8
      fi
      continue
    fi
    sleep 4
    [ $((SECONDS % 60)) -lt 5 ] && echo "t=${SECONDS}s de0c=$(cnt) md5=$m"
    continue
  fi
  # flying phase
  n=$((n+1))
  keys "plus:80:10"; keys "Tab:3000:1"; keys "Down:1200:1"; keys "Up:1200:1"
  grab >/dev/null
  cp -f $O/cur.xwd $O/g$(printf %03d $n)-${SECONDS}s.xwd 2>/dev/null
  d=$(fp 150 95 340 30)
  if [ "$d" = "$DIED" ]; then
    echo "=== YOU HAVE DIED at ${SECONDS}s, $((SECONDS-th))s after the menu"
    /tmp/claude-0/xclick :7 465 170 >/dev/null 2>&1
    sleep 6
  fi
  echo "g t=${SECONDS}s de0c=$(cnt) md5=$(md5sum $O/cur.xwd | cut -c1-12)"
  sleep 20
done
wait $DP 2>/dev/null; echo "dosemu exit=$?"
echo "=== de0c: $(cnt)"
echo "=== XEXC: $(grep -ac XEXC $O/sc.log)"
grep -a XEXC $O/sc.log | head -4
echo "=== XHMA: $(grep -ac XHMA $O/sc.log)"
kill $XP 2>/dev/null
for f in $O/*.xwd; do
  [ -e "$f" ] || continue
  xwdtopnm < "$f" 2>/dev/null | pnmtopng > "${f%.xwd}.png" 2>/dev/null
done
rm -f $O/*.xwd
echo "done"

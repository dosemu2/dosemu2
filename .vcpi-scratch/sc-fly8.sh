#!/bin/bash
# Press TRAINING MISSION for real.  Two things make this work where the earlier
# stands failed: the menu is only up for part of the attract cycle, so every
# frame is fingerprinted by a crop of the menu box before anything is pressed;
# and the guest cursor follows relative motion, so it is parked in the corner
# and walked out by a counted number of steps.  From an earlier measurement one
# step of 8 units is about 3.2 game pixels across and 0.4 down, which puts the
# TRAINING MISSION row near ny=29; the list walks outwards from there.
exec > /tmp/scfly8.log 2>&1
export DOSEMU2_COMCOM_DIR=/usr/local/share/comcom32
D=/home/user/dosemu2/bin/dosemu
O=/tmp/claude-0/scfly8
LAUNCH=cbdb02e32e1e
GMENU=7d19864d329d9f9da191cf6578df5426
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
timeout 2700 $D -s -X -f $O/rc -o $O/sc.log -D+E -K . -E "PD.BAT" </dev/null > $O/sc.out 2>&1 &
DP=$!
grab () {
  local id
  id=$(xwininfo -root -children 2>/dev/null | grep -oE '0x[0-9a-f]+' | sed -n 2p)
  [ -n "$id" ] && xwd -id "$id" -silent > $O/cur.xwd 2>/dev/null
  md5sum $O/cur.xwd 2>/dev/null | cut -c1-12
}
menufp () {
  xwdtopnm < $O/cur.xwd 2>/dev/null | pamcut -left 95 -top 45 -width 460 -height 30 2>/dev/null | md5sum | cut -d' ' -f1
}
shot () { cp -f $O/cur.xwd "$1" 2>/dev/null; }
played=0; t0=0; n=0; hit=0
NYS="29 28 30 27 31 26 32 25 33 24 34 23 35 22 36 21 37 20 38"
set -- $NYS
while [ $SECONDS -lt 2650 ]; do
  kill -0 $DP 2>/dev/null || { echo "dosemu gone at ${SECONDS}s"; break; }
  m=$(grab)
  if [ "$m" = "$LAUNCH" ] && [ $played -eq 0 ]; then
    shot $O/launcher.xwd
    echo "launcher at ${SECONDS}s, PLAY"
    /tmp/claude-0/xsweep :7 348 88 2>&1 | sed 's/^/  /'
    played=1; t0=$SECONDS
    sleep 4
    continue
  fi
  if [ $played -eq 1 ] && [ $hit -eq 0 ]; then
    if [ $((SECONDS - t0)) -lt 240 ]; then
      /tmp/claude-0/xkey :7 Escape >/dev/null 2>&1
    elif [ "$(menufp)" = "$GMENU" ] && [ $# -gt 0 ]; then
      ny=$1; shift
      n=$((n+1))
      shot $O/m$(printf %02d $n)-ny$ny-menu.xwd
      echo "menu up at ${SECONDS}s, attempt $n: nx=20 ny=$ny"
      /tmp/claude-0/xrel :7 20 $ny 3 2>&1 | sed 's/^/    /'
      sleep 4
      grab >/dev/null
      f=$(menufp)
      shot $O/m$(printf %02d $n)-ny$ny-after.xwd
      if [ "$f" != "$GMENU" ]; then
        sleep 6; grab >/dev/null; f2=$(menufp)
        shot $O/m$(printf %02d $n)-ny$ny-after2.xwd
        echo "    menu gone (fp $f then $f2) -- candidate hit"
        if [ "$f2" != "$GMENU" ]; then
          hit=1; th=$SECONDS
          echo "=== TOOK ny=$ny at ${SECONDS}s"
        fi
      else
        echo "    still on the menu"
      fi
    fi
  fi
  if [ $hit -eq 1 ]; then
    d=$((SECONDS - th))
    n=$((n+1))
    shot $O/h$(printf %03d $n)-${SECONDS}s.xwd
    echo "h t=${SECONDS}s d=${d}s md5=$m fp=$(menufp)"
    # briefing screens want a click or a key; then try the throttle and stick
    if [ $((d % 60)) -lt 10 ]; then
      /tmp/claude-0/xrel :7 20 29 2 >/dev/null 2>&1
      /tmp/claude-0/xkey :7 Return >/dev/null 2>&1
    else
      /tmp/claude-0/xkey2 :7 plus:200:20 >/dev/null 2>&1
      /tmp/claude-0/xkey2 :7 Tab:3000:1 >/dev/null 2>&1
      /tmp/claude-0/xkey2 :7 Down:1500:1 >/dev/null 2>&1
    fi
    sleep 8
    continue
  fi
  if [ $((SECONDS % 40)) -lt 4 ]; then
    echo "t=${SECONDS}s de0c=$(grep -ac 'VCPI: switch to PM' $O/sc.log) md5=$m fp=$(menufp | cut -c1-8)"
  fi
  sleep 4
done
wait $DP; echo "dosemu exit=$?"
echo "=== de0c: $(grep -ac 'VCPI: switch to PM' $O/sc.log)"
echo "=== XHMA: $(grep -ac XHMA $O/sc.log)"
grep -a XHMA $O/sc.log | head -5
kill $XP 2>/dev/null
for f in $O/*.xwd; do
  [ -e "$f" ] || continue
  xwdtopnm < "$f" 2>/dev/null | pnmtopng > "${f%.xwd}.png" 2>/dev/null
done
rm -f $O/*.xwd
echo "done"

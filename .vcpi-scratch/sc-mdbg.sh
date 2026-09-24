#!/bin/bash
# Does the mouse reach the game at all under the VCPI monitor?  Same stand as
# before, but with the mouse log on: when the menu is up, click where the row
# is and then read what int33 was asked and what it answered.
exec > /tmp/scmdbg.log 2>&1
export DOSEMU2_COMCOM_DIR=/usr/local/share/comcom32
D=/home/user/dosemu2/bin/dosemu
O=/tmp/claude-0/scmdbg
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
rm -f /tmp/.X8-lock
Xvfb :8 -screen 0 1024x768x24 >/tmp/xvfb8.log 2>&1 &
XP=$!
for i in $(seq 1 30); do [ -e /tmp/.X11-unix/X8 ] && break; sleep 1; done
export DISPLAY=:8
cd /tmp/claude-0/sc
cp -f /tmp/claude-0/sc-pristine-cfg/SC.CFG /tmp/claude-0/sc-pristine-cfg/SCTO1.CFG . 2>/dev/null
for c in SC.CFG SCTO1.CFG; do grep -a '^=' $c > $c.tmp; printf '\r\n' >> $c.tmp; mv -f $c.tmp $c; done
for g in OPTTEST MKTERR STRIKE; do cp -f $g.SAV $g.EXE; done
cp -f MENU.SAV MENU.EXE
rm -f ranm.txt RANM.TXT ranx.txt RANX.TXT
touch -a -d '2020-01-01 00:00' *.EXE *.OVL
printf 'SCCD.EXE\r\n' > PD.BAT
timeout 1700 $D -s -X -f $O/rc -o $O/sc.log -D+E+m -K . -E "PD.BAT" </dev/null > $O/sc.out 2>&1 &
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
mcnt () { grep -ac 'mouse' $O/sc.log; }
played=0; t0=0; n=0
while [ $SECONDS -lt 1650 ]; do
  kill -0 $DP 2>/dev/null || { echo "dosemu gone at ${SECONDS}s"; break; }
  m=$(grab)
  if [ "$m" = "$LAUNCH" ] && [ $played -eq 0 ]; then
    echo "launcher at ${SECONDS}s, PLAY (mouse lines so far: $(mcnt))"
    /tmp/claude-0/xclick :8 348 88 >/dev/null 2>&1
    /tmp/claude-0/xclick :8 348 88 >/dev/null 2>&1
    played=1; t0=$SECONDS
    sleep 4; continue
  fi
  if [ $played -eq 1 ]; then
    if [ $((SECONDS - t0)) -lt 240 ]; then
      /tmp/claude-0/xkey :8 Escape >/dev/null 2>&1
    elif [ "$(menufp)" = "$GMENU" ]; then
      n=$((n+1))
      a=$(mcnt)
      echo "menu up at ${SECONDS}s, round $n: mouse lines before $a"
      cp -f $O/cur.xwd $O/r$(printf %02d $n)-before.xwd 2>/dev/null
      /tmp/claude-0/xclick :8 325 185 >/dev/null 2>&1
      sleep 2
      b=$(mcnt)
      echo "  after move+click: mouse lines $b (delta $((b-a)))"
      grab >/dev/null; cp -f $O/cur.xwd $O/r$(printf %02d $n)-after.xwd 2>/dev/null
      echo "  fp now $(menufp | cut -c1-8)"
      if [ $n -ge 6 ]; then
        echo "=== six rounds done at ${SECONDS}s"
        break
      fi
      sleep 20
    fi
  fi
  if [ $((SECONDS % 60)) -lt 4 ]; then
    echo "t=${SECONDS}s de0c=$(grep -ac 'VCPI: switch to PM' $O/sc.log) mouse=$(mcnt) md5=$m"
  fi
  sleep 4
done
echo "=== mouse lines total: $(mcnt)"
echo "=== distinct mouse messages:"
grep -ao 'mouse[^0-9]*' $O/sc.log | sed 's/[ \t]*$//' | sort | uniq -c | sort -rn | head -25
echo "=== int33 traffic sample:"
grep -a 'int33\|Mouse: \|mouse: ' $O/sc.log | tail -40
kill -TERM $DP 2>/dev/null
sleep 3
kill $XP 2>/dev/null
for f in $O/*.xwd; do
  [ -e "$f" ] || continue
  xwdtopnm < "$f" 2>/dev/null | pnmtopng > "${f%.xwd}.png" 2>/dev/null
done
rm -f $O/*.xwd
echo "done"

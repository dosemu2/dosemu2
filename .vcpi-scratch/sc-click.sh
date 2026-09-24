#!/bin/bash
# The stand has been launching the game through MD-MENU.COM, a 512 byte stub
# that prints PSP/MAXFREE and EXECs STRIKE.EXE straight away -- the real
# 165k MENU.EXE never runs, so no pilot and no mission are ever chosen.
# This run keeps the real menu and drives it from the keyboard.
exec > /tmp/scclick.log 2>&1
export DOSEMU2_COMCOM_DIR=/usr/local/share/comcom32
D=/home/user/dosemu2/bin/dosemu
O=/tmp/claude-0/scclick
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
cp -f MENU.SAV MENU.EXE          # the REAL menu, not the stub
ls -l MENU.EXE STRIKE.EXE
rm -f ranm.txt RANM.TXT ranx.txt RANX.TXT
touch -a -d '2020-01-01 00:00' *.EXE *.OVL
printf 'SCCD.EXE\r\n' > PD.BAT
timeout 900 $D -s -X -f $O/rc -o $O/sc.log -D+E -K . -E "PD.BAT" </dev/null > $O/sc.out 2>&1 &
DP=$!
shot () {
  local n=$1 id
  id=$(xwininfo -root -children 2>/dev/null | grep -oE '0x[0-9a-f]+' | sed -n 2p)
  [ -n "$id" ] && xwd -id "$id" -silent > $O/win-$n.xwd 2>/dev/null
  echo "shot $n de0c=$(grep -ac 'VCPI: switch to PM' $O/sc.log) md5=$(md5sum $O/win-$n.xwd 2>/dev/null | cut -c1-12)"
}
click () { /tmp/claude-0/xclick :7 "$@" 2>&1 | sed "s/^/  xclick: /"; }
keys () { /tmp/claude-0/xkey :7 "$@" 2>&1 | sed 's/^/  xkey: /'; }
wait_to () { while [ $SECONDS -lt $1 ]; do sleep 1; done; }

for t in 60 90 120 150 180; do
  wait_to $t
  kill -0 $DP 2>/dev/null || { echo "dosemu gone before ${t}s"; break; }
  shot $t
done
# drive whatever menu is on screen
# beat on every key the intro might listen to
# the menu comes up around 290 s and holds some 45 s before timing out
# back to the intro.  "PLAY STRIKE COMMANDER" sits at about 348,88.
for t in $(seq 200 10 270); do
  wait_to $t; kill -0 $DP 2>/dev/null || { echo "gone before ${t}s"; break; }
  shot $t
done
for t in $(seq 275 5 360); do
  wait_to $t; kill -0 $DP 2>/dev/null || { echo "gone before ${t}s"; break; }
  shot $t
  click 348 88
done
for t in $(seq 370 10 880); do
  wait_to $t; kill -0 $DP 2>/dev/null || { echo "gone before ${t}s"; break; }
  shot $t
done
wait $DP; echo "dosemu exit=$?"
echo "=== de0c: $(grep -ac 'VCPI: switch to PM' $O/sc.log)"
echo "=== faults: $(grep -ac 'exit_reason = 0x8' $O/sc.log)"
kill $XP 2>/dev/null
for f in $O/win-*.xwd; do
  [ -e "$f" ] || continue
  xwdtopnm < "$f" 2>/dev/null | pnmtopng > "${f%.xwd}.png" 2>/dev/null
done
echo "=== pngs ==="; md5sum $O/*.png 2>/dev/null
cd /tmp/claude-0/sc && cp -f MENU.SAV MENU.EXE
echo "=== XIO ==="; grep -a "^ERROR: XIO" $O/sc.out | head -60
echo "=== XKB ==="; grep -a "^XKB" $O/sc.out | tail -3
echo "=== XFAULT ==="; grep -a 'XFAULT' /tmp/claude-0/scclick/sc.out /tmp/claude-0/scmenu2/sc.log 2>/dev/null

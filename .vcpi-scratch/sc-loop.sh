#!/bin/bash
exec > /tmp/scloop.log 2>&1
export DOSEMU2_COMCOM_DIR=/usr/local/share/comcom32
D=/home/user/dosemu2/bin/dosemu
O=/tmp/claude-0/scloop
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
for c in SC.CFG SCTO1.CFG; do grep -a '^=' $c > $c.tmp; printf '\r\n' >> $c.tmp; mv -f $c.tmp $c; done
for g in OPTTEST MKTERR STRIKE; do cp -f $g.SAV $g.EXE; done
cp -f MD-MENU.COM MENU.EXE
rm -f ranm.txt RANM.TXT ranx.txt RANX.TXT
touch -a -d '2020-01-01 00:00' *.EXE *.OVL
trap 'cd /tmp/claude-0/sc; cp -f MENU.SAV MENU.EXE' EXIT
printf 'SCCD.EXE\r\n' > PD.BAT
timeout 400 $D -s -X -f $O/rc -o $O/sc.log -D+D -K . -E "PD.BAT" </dev/null > $O/sc.out 2>&1 &
DP=$!
shot () {
  local n=$1 id
  id=$(xwininfo -root -children 2>/dev/null | grep -oE '0x[0-9a-f]+' | sed -n 2p)
  [ -n "$id" ] && xwd -id "$id" -silent > $O/win-$n.xwd 2>/dev/null
  echo "shot $n de0c=$(grep -ac 'VCPI: switch to PM' $O/sc.log)"
}
for t in 110 280 360; do
  while [ $SECONDS -lt $t ]; do sleep 2; done
  kill -0 $DP 2>/dev/null || { echo "dosemu gone before ${t}s"; break; }
  shot $t
done
wait $DP; echo "dosemu exit=$?"
echo "=== de0c: $(grep -ac 'VCPI: switch to PM' $O/sc.log)"
echo "=== faults: $(grep -ac 'exit_reason = 0x8' $O/sc.log)"
echo "=== guard: $(grep -ac 'halfway-out guard hit' $O/sc.log)"
tail -5 $O/sc.out
kill $XP 2>/dev/null
for f in $O/win-*.xwd; do
  [ -e "$f" ] || continue
  xwdtopnm < "$f" 2>/dev/null | pnmtopng > "${f%.xwd}.png" 2>/dev/null
done
echo "=== pngs ==="; ls -la $O/*.png 2>/dev/null
echo "=== what the game asks DOS for in its steady state (last 400 int21) ==="
grep -a "INT21" $O/sc.log | tail -400 | sed -E "s/.*(AX=[0-9a-f]{4}).*/\1/" | sort | uniq -c | sort -rn | head -15

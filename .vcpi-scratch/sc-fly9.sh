#!/bin/bash
# Fly it under an occupied HMA.  MENU.EXE is replaced by the stub that goes
# straight to the cockpit, so the attract loop and the mouse-driven menu are
# out of the way and the run is about the flight itself.
exec > /tmp/scfly9.log 2>&1
export DOSEMU2_COMCOM_DIR=/usr/local/share/comcom32
D=/home/user/dosemu2/bin/dosemu
O=/tmp/claude-0/scfly9
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
if grep -qi 'DOS=HIGH' /root/.dosemu/drive_c/userhook.sys 2>/dev/null; then
  echo "userhook.sys ok, DOS goes to the HMA"
else
  echo "no userhook.sys: HMA stays free"
fi
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
timeout 2700 $D -s -X -f $O/rc -o $O/sc.log -D+E -K . -E "PD.BAT" </dev/null > $O/sc.out 2>&1 &
DP=$!
grab () {
  local id
  id=$(xwininfo -root -children 2>/dev/null | grep -oE '0x[0-9a-f]+' | sed -n 2p)
  [ -n "$id" ] && xwd -id "$id" -silent > $O/cur.xwd 2>/dev/null
  md5sum $O/cur.xwd 2>/dev/null | cut -c1-12
}
cnt () { grep -ac 'VCPI: switch to PM' $O/sc.log; }
keys () { /tmp/claude-0/xkey2 :7 "$@" >/dev/null 2>&1; }
n=0; last=0; samec=0
while [ $SECONDS -lt 2650 ]; do
  kill -0 $DP 2>/dev/null || { echo "dosemu gone at ${SECONDS}s"; break; }
  if [ $SECONDS -gt 300 ]; then
    keys "plus:80:10"
    keys "Tab:3000:1"
    keys "Down:1200:1"
    keys "Up:1200:1"
  fi
  c=$(cnt)
  if [ "$c" = "$last" ]; then samec=$((samec+1)); else samec=0; fi
  last=$c
  n=$((n+1))
  cp -f $O/cur.xwd $O/f$(printf %03d $n)-${SECONDS}s.xwd 2>/dev/null
  echo "t=${SECONDS}s de0c=$c same=$samec md5=$(grab)"
  if [ $samec -ge 3 ]; then
    echo "=== WEDGED at ${SECONDS}s, backtrace ==="
    ps -eo pid,args | grep 'dosemu2\.bin' | grep -v grep | awk '{print $1}' > /tmp/claude-0/wpids
    while read p; do
      echo "--- pid $p"
      timeout 60 gdb -p $p -batch -ex "info threads" -ex "thread apply all bt 25" 2>&1 | head -80
    done < /tmp/claude-0/wpids
    echo "=== end backtrace ==="
    samec=0
  fi
  sleep 18
done
wait $DP; echo "dosemu exit=$?"
echo "=== de0c: $(cnt)"
echo "=== XHMA: $(grep -ac XHMA $O/sc.log)"
grep -a XHMA $O/sc.log | head -6
kill $XP 2>/dev/null
for f in $O/*.xwd; do
  [ -e "$f" ] || continue
  xwdtopnm < "$f" 2>/dev/null | pnmtopng > "${f%.xwd}.png" 2>/dev/null
done
rm -f $O/*.xwd
echo "done"

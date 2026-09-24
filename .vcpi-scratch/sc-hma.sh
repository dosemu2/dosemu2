#!/bin/bash
# Click PLAY, let the intro run, and the moment the guest stops making VCPI
# switches take a backtrace of every dosemu thread.
exec > /tmp/schma.log 2>&1
export DOSEMU2_COMCOM_DIR=/usr/local/share/comcom32
D=/home/user/dosemu2/bin/dosemu
O=/tmp/claude-0/schma
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
# put DOS in the HMA: fdppconf.sys chains userhook.sys from the boot drive
mkdir -p /root/.dosemu/drive_c
printf 'DOS=HIGH\r\n' > /root/.dosemu/drive_c/userhook.sys
cat -v /root/.dosemu/drive_c/userhook.sys
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
PIDS=""
for i in $(seq 1 40); do
  sleep 3
  PIDS=$(ps -eo pid,cmd | grep 'dosemu2\.bin' | grep -v grep | awk '{print $1}')
  [ -n "$PIDS" ] && break
done
echo "dosemu pids: $PIDS"
ps -eo pid,cmd | grep '[d]osemu' | sed 's/^/  ps: /'
grab () {
  local id
  id=$(xwininfo -root -children 2>/dev/null | grep -oE '0x[0-9a-f]+' | sed -n 2p)
  [ -n "$id" ] && xwd -id "$id" -silent > $O/cur.xwd 2>/dev/null
  md5sum $O/cur.xwd 2>/dev/null | cut -c1-12
}
cnt () { grep -ac 'VCPI: switch to PM' $O/sc.log 2>/dev/null; }
clicked=0; last=-1; same=0; dumped=0
while [ $SECONDS -lt 1620 ]; do
  kill -0 $DP 2>/dev/null || { echo "dosemu gone at ${SECONDS}s"; break; }
  m=$(grab)
  if [ "$m" = "$MENU" ] && [ $clicked -eq 0 ]; then
    echo "MENU on screen at ${SECONDS}s, clicking"
    /tmp/claude-0/xclick :7 348 88 2>&1 | sed 's/^/  /'
    clicked=1
  fi
  c=$(cnt)
  if [ "$c" = "$last" ]; then same=$((same+1)); else same=0; fi
  last=$c
  echo "t=${SECONDS}s de0c=$c same=$same md5=$m"
  if [ $same -ge 3 ] && [ $dumped -lt 3 ] && [ "$c" -gt 1000 ]; then
    dumped=$((dumped+1))
    cp -f $O/cur.xwd $O/wedge-$dumped.xwd 2>/dev/null
    echo "=== WEDGED, backtrace #$dumped at ${SECONDS}s ==="
    for P in $PIDS; do
      echo "--- pid $P"
      timeout 150 gdb -p "$P" -batch \
        -ex "set pagination off" \
        -ex "info threads" \
        -ex "thread apply all bt 30" 2>&1 | sed 's/^/  /'
    done
    echo "=== end backtrace #$dumped ==="
    same=0
  fi
  sleep 15
done
kill $DP 2>/dev/null; kill $XP 2>/dev/null
echo "=== de0c: $(cnt)"
for f in $O/*.xwd; do
  [ -e "$f" ] || continue
  xwdtopnm < "$f" 2>/dev/null | pnmtopng > "${f%.xwd}.png" 2>/dev/null
done
rm -f $O/*.xwd
echo "done"

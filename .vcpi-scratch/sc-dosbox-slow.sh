#!/bin/bash
exec > /tmp/scdbxslow.log 2>&1
O=/tmp/claude-0/scdbxslow
rm -rf $O; mkdir -p $O
rm -f /tmp/.X8-lock
Xvfb :8 -screen 0 1024x768x24 >/tmp/xvfb8.log 2>&1 &
XP=$!
for i in $(seq 1 30); do [ -e /tmp/.X11-unix/X8 ] && break; sleep 1; done
export DISPLAY=:8
export SDL_AUDIODRIVER=dummy
cd /tmp/claude-0/sc-dbx
timeout 700 dosbox -conf /tmp/claude-0/dosbox-slow.conf -noconsole > $O/dosbox.out 2>&1 &
DP=$!
grab () {
  local id
  id=$(xwininfo -root -children 2>/dev/null | grep -oE '0x[0-9a-f]+' | sed -n 2p)
  [ -n "$id" ] && xwd -id "$id" -silent > $O/cur.xwd 2>/dev/null
  md5sum $O/cur.xwd 2>/dev/null | cut -c1-12
}
clicked=0
while [ $SECONDS -lt 680 ]; do
  kill -0 $DP 2>/dev/null || { echo "dosbox gone at ${SECONDS}s"; break; }
  m=$(grab)
  [ $((SECONDS % 10)) -lt 2 ] && { echo "t=${SECONDS}s md5=$m"; cp -f $O/cur.xwd $O/t-$SECONDS.xwd 2>/dev/null; }
  if [ $SECONDS -ge 300 ] && [ $clicked -lt 1 ]; then
    echo "clicking PLAY at ${SECONDS}s"
    /tmp/claude-0/xclick :8 348 88 2>&1 | sed 's/^/  /'
    clicked=1
    for k in $(seq 1 20); do
      sleep 5
      echo "  after+$((k*5))s md5=$(grab)"
      cp -f $O/cur.xwd $O/after-$k.xwd 2>/dev/null
    done
  fi
  sleep 2
done
kill $DP 2>/dev/null; kill $XP 2>/dev/null
for f in $O/*.xwd; do [ -e "$f" ] || continue; xwdtopnm < "$f" 2>/dev/null | pnmtopng > "${f%.xwd}.png" 2>/dev/null; done
echo "=== done"

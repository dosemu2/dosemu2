#!/bin/sh
# Per-branch stand script: runs with DISPLAY=:7 (Xvfb) after the build.
# Games, if fetched, are in ~/games.  Put logs and screenshots in ~/probe.
set -x
ls ~/games 2>/dev/null
cd ~/probe
printf '$_cpu_vm = "kvm"\n$_cpu_vm_dpmi = "kvm"\n$_video = "X"\n' > x.conf
timeout 60 dosemu -f x.conf -X -n -q -o x.log -E "ver" &
sleep 20
import -window root x-boot.png
wait
grep -iE 'kvm|X: ' x.log | head -20

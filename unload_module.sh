#DOSEMUPATH=/usr/src/dosemu
DOSEMUPATH=.

if [ "`lsmod|grep emumodule`" != "" ]; then
  rmmod emumodule
fi
if [ "`lsmod|grep syscallmgr`" != "" ]; then
  echo "are you shure to unload syscallmgr ? (y)"
  read x
  if [ "$x" = "y" ]; then
    rmmod syscallmgr
  fi
fi

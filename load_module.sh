#DOSEMUPATH=/usr/src/dosemu
DOSEMUPATH=.

if [ ! -d $DOSEMUPATH ]; then
  echo "$DOSEMUPATH not existing"
  exit 1
fi
if [ ! -x ${DOSEMUPATH}/syscallmgr/insmod ]; then
  echo "${DOSEMUPATH}/syscallmgr/insmod not existing"
  exit 1
fi
if [ ! -f ${DOSEMUPATH}/syscallmgr/syscallmgr.o ]; then
  echo "${DOSEMUPATH}/syscallmgr/syscallmgr.o not existing"
  exit 1
fi      
if [ ! -f ${DOSEMUPATH}/emumod/emumodule.o ]; then
  echo "${DOSEMUPATH}/emumod/emumodule.o not existing"
  exit 1
fi

if [ "`lsmod|grep emumodule`" != "" ]; then
  rmmod emumodule
fi
if [ "`lsmod|grep syscallmgr`" = "" ]; then
  ${DOSEMUPATH}/syscallmgr/insmod ${DOSEMUPATH}/syscallmgr/syscallmgr.o
fi
 
${DOSEMUPATH}/syscallmgr/insmod -z ${DOSEMUPATH}/emumod/emumodule.o

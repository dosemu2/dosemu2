#!/bin/bash
#DOSEMUPATH=/usr/src/dosemu
DOSEMUPATH=.

if [ ! -f ${DOSEMUPATH}/syscallmgr/rmmod ]; then
  echo "${DOSEMUPATH}/syscallmgr/rmmod not existing"
  exit 1
fi
if [ "`lsmod|grep emumodule`" != "" ]; then
  ${DOSEMUPATH}/syscallmgr/rmmod emumodule
fi
if [ "`lsmod|grep syscallmgr`" != "" ]; then
  echo "are you shure to unload syscallmgr ? (y)"
  read x
  if [ "$x" = "y" ]; then
    ${DOSEMUPATH}/syscallmgr/rmmod syscallmgr
  fi
fi

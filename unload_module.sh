#!/bin/bash
BINDIR=./bin

if [ ! -f ${BINDIR}/rmmod ]; then
  echo "${BINDIR}/rmmod not existing"
  exit 1
fi
if [ "`lsmod|grep emumodule`" != "" ]; then
  ${BINDIR}/rmmod emumodule
fi
if [ "`lsmod|grep syscallmgr`" != "" ]; then
  echo "are you shure to unload syscallmgr ? (y)"
  read x
  if [ "$x" = "y" ]; then
    ${BINDIR}/rmmod syscallmgr
  fi
fi

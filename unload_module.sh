#!/bin/bash
BINDIR=./bin

if [ ! -f ${BINDIR}/rmmod ]; then
  echo "${BINDIR}/rmmod not existing"
  exit 1
fi
if [ "`lsmod|grep emumodule`" != "" ]; then
  ${BINDIR}/rmmod emumodule
fi

#!/bin/bash
MODULESDIR=./modules
BINDIR=./bin

if [ ! -d $BINDIR ]; then
  echo "$BINDIR not existing"
  exit 1
fi
if [ ! -d $MODULESDIR ]; then
  echo "$MODULESDIR not existing"
  exit 1
fi
if [ ! -x ${BINDIR}/insmod ]; then
  echo "${BINDIR}/insmod not existing"
  exit 1
fi
if [ ! -f ${BINDIR}/rmmod ]; then
  echo "${BINDIR}/rmmod not existing"
  exit 1
fi
if [ ! -f ${MODULESDIR}/syscallmgr.o ]; then
  echo "${MODULESDIR}/syscallmgr.o not existing"
  exit 1
fi      
if [ ! -f ${MODULESDIR}/emumodule.o ]; then
  echo "${MODULESDIR}/emumodule.o not existing"
  exit 1
fi

if [ "`lsmod|grep emumodule`" != "" ]; then
  ${BINDIR}/rmmod emumodule
fi
if [ "`lsmod|grep syscallmgr`" = "" ]; then
  ${BINDIR}/insmod -z ${MODULESDIR}/syscallmgr.o
fi
 
${BINDIR}/insmod -lz ${MODULESDIR}/emumodule.o

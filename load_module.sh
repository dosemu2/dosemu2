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
if [ ! -f ${MODULESDIR}/emumodule.o ]; then
  echo "${MODULESDIR}/emumodule.o not existing"
  exit 1
fi

# we have check if the modules fit in the type of kernel compilation
# (we would come into _great_ trouble when ignoring this )
#
if grep cpu_data /proc/ksyms >/dev/null 2>&1; then
  # we run on SMP kernel
  if ! nm ${MODULESDIR}/emumodule.o |grep apic_reg >/dev/null 2>&1; then
    echo "emumodule is compile for uniprocessor, but your kernel is SMP"
    exit 1
  fi
else
  # we run on uniprocessor kernel
  if nm ${MODULESDIR}/emumodule.o |grep apic_reg >/dev/null 2>&1; then
    echo "emumodule is compile for SMP, but your kernel is uniprocessor"
    exit 1
  fi
fi

if [ "`lsmod|grep emumodule`" != "" ]; then
  ${BINDIR}/rmmod emumodule
fi
 
${BINDIR}/insmod -lz ${MODULESDIR}/emumodule.o

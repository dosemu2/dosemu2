#! /bin/bash
#
# This installs the emumodule, so that you can load the modules
# via 'modprobe dosemu'

KVERSION=`uname -r`
MOD_LIB=/lib/modules/${KVERSION}/misc
MOD_BIN=/sbin
CONF=/etc/conf.modules

if [ ! -d ${MOD_LIB} ]; then
  echo "you have no installed modules tree in ${MOD_LIB}"
  echo "... giving up"
fi

echo "================================================================"
echo "NOTE: This will replace your ${MOD_BIN}/insmod with our insmod."
echo "      If you don't like that, type Ctrl-C and use load_module.sh"
echo "      else type ENTER to continue."
echo "================================================================"
echo
read

if [ ! -f /etc/conf.modules ]; then
  CONF=/etc/modules.conf
fi

if [ ! -f ${MOD_BIN}/insmod.old ]; then
  ( cd ${MOD_BIN}; mv insmod insmod.old )
fi

cp -p ./bin/insmod ${MOD_BIN}
cp -p ./modules/* ${MOD_LIB}
depmod -a >/dev/null 2>&1     # will produce an error, we know about it.


if [ "`awk '/emumodule/ && /options/ {print}' ${CONF}`" = "" ]; then
  cat >> /x <<EOF

options emumodule +-lz
alias dosemu emumodule

EOF
fi

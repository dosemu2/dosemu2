#!/bin/sh
#
# This program configures DOSEmu.
#
# This script is (c) Copyright 1996 Alistair MacDonald 
#                                   <alistair@slitesys.demon.co.uk>
#
# It is made available to the DOSEmu project under the GPL Version 2, or at
# your option any later version. Details of this Licence can be found in the
# file 'COPYING' in the distribution's top level directory.
#
# Version 0.1
#  Initial Version.
#

# A Few settings
mkdir -p $HOME/.dosemu/tmp
TEMP="$HOME/.dosemu/tmp/$0.$$"
CONF_FILE="../compiletime-settings"

. select-dialog

clean_up() {
  clear

  rm -f $TEMP
  exit
}

get_defaults() {
  $AWK -f 'parse-config' -f 'parse-misc' -f 'parse-config-sh' $CONF_FILE > $TEMP 2> /dev/null
  . $TEMP 2> /dev/null
}

get_value() {
  local TMP

  eval "TMP=\$config_$2"

  if [ "@$TMP" != "@" ]
  then
    eval "$1=$TMP"
  fi
}

set_value() {
  eval "config_$1=$2"
}

handle_response_value() {
  if [ "$1" = "*\"$2\"*" ]
  then
    set_value "$3" "on"
  else
    set_value "$3" "off"
  fi
}

form_pairs () {
  local str val

  var=$1
  prefix=$2
  shift 2

  while [ $# -gt 1 ]
  do
    val=""
    get_value val $1
    if [ "@$val" != "@" -a "@$val" != "@\*" ]; 
    then
      str="$str $1 $val"
    fi

    shift
  done

  eval "$var=\$str"
}

form_multi_pairs () {
  local str val

  var=$1
  prefix=$2
  suffix=$3
  shift 3

  while [ $# -gt 1 ]
  do
    val=""
    get_value val "${prefix}_$1_${suffix}"
    if [ "@$val" != "@" -a "@$val" != "@\*" ]; 
    then
      str="$str $1 $val"
    fi

    shift
  done

  eval "$var=\$str"
}

write_pairs() {
  local text

  form_pairs text "$@"

  if [ "@$text" != "@" -a "@$text" != "@\*" ]; 
  then
    echo "config[\"$1\"] = \"$1 { $text }\"" >> $TEMP
  fi
}

write_single() {
  local value

  get_value value $1

  if [ "@$value" != "@" -a "@$value" != "@\*" ]; 
  then
    echo "config[\"$1\"] = \"$1 $value\"" >> $TEMP
  fi
}

write_out() {
  local value

  handle_Configure_Action

  value=""
  get_value value configure
 if [ "@$value" = "@save" ]; then

  echo "function setup_config () {" > $TEMP
  write_pairs  config  experimental sbemu mitshm x net debug \
    newport linkstatic newx cpuemu dummydummy
  echo "}" >> $TEMP

  (cd ..; clear; ./default-configure $TEMP)
  echo ""
  echo "   ... type ENTER to return to menu"
  read

  $DIALOG --backtitle "DOSEMU Compile-Time Configuration" \
    --infobox "Writing Configuration ..." 3 50 2> /dev/null

  cp -f $CONF_FILE ${CONF_FILE}~ 2> /dev/null
  touch $CONF_FILE
  $AWK -f 'parse-misc' -f 'write-config' -f $TEMP $CONF_FILE > $CONF_FILE.tmp
  mv $CONF_FILE.tmp $CONF_FILE  

 fi
}

load_menus() {
  $DIALOG --backtitle "DOSEMU Compile-Time Configuration" \
    --infobox "Building Menus ..." 3 50 2> /dev/null

  # Create the menus
  $AWK -f 'parse-menu-sh' -f 'parse-menu' -f 'parse-misc' compiletime_setup.menu > $TEMP
  # Load them up ...
  . $TEMP

}

main() {
  get_defaults

  set_value configure save

  load_menus

  start

  write_out

  clean_up
}


# Start the whole process ...
main

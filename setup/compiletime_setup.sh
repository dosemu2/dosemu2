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
TEMP="/tmp/$0.$$"
CONF_FILE="../.compiletime-settings"

. select-dialog

clean_up() {
  clear

  rm -f $TEMP
  exit
}

get_defaults() {
  awk -f 'parse-config' -f 'parse-misc' -f 'parse-config-sh' $CONF_FILE > $TEMP
  . $TEMP
}

get_value() {
  local TMP
  eval "TMP=\${LOCAL_$2:-\$SYSTEM_$2}"
  if [ "@$TMP" != "@" ]
  then
    eval "$1=$TMP"
  fi
}

set_value() {
  eval "$1=$2"
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
    get_value val "${prefix}_$1"
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

MainMenu_End() {
  local value
  local STRING=""

  value=""
  get_value value novm86plus
  if [ "@$value" != "@" ]
  then
    STRING="$STRING --enable-novm86plus"
  fi

  value=""
  get_value value dodebug
  if [ "@$value" != "@" ]
  then
    STRING="$STRING --enable-dodebug"
  fi

  value=""
  get_value value new-kbd
  if [ "@$value" != "@" ]
  then
    STRING="$STRING --enable-new-kbd"
  fi

  value=""
  get_value value x
  if [ "@$value" != "@" ]
  then
    STRING="$STRING --with-x"
  else
    STRING="$STRING --without-x"
  fi

  value=""
  get_value value mitshm
  if [ "@$value" != "@" ]
  then
    STRING="$STRING --enable-mitshm"
  else
    STRING="$STRING --disable-mitshm"
  fi

  value=""
  get_value value sbemu
  if [ "@$value" != "@" ]
  then
    STRING="$STRING --enable-sbemu"
  else
    STRING="$STRING --disable-sbemu"
  fi

  (cd ..; clear; ./configure $STRING)
}

write_out() {
  echo "function setup_config () {" > $TEMP

  write_single  sbemu
  write_single  novm86plus
  write_single  mitshm
  write_single  x
  write_single  dodebug
  write_single  new-kbd

  echo "}" >> $TEMP

  $DIALOG --backtitle "DOSEmu Compile-Time Configuration" \
    --infobox "Writing Configuration ..." 3 50 2> /dev/null

  cp $CONF_FILE $CONF_FILE.bak
  awk -f 'parse-misc' -f 'write-config' -f $TEMP $CONF_FILE > $CONF_FILE.tmp
  mv $CONF_FILE.tmp $CONF_FILE  
}

load_menus() {
  $DIALOG --backtitle "DOSEmu Compile-Time Configuration" \
    --infobox "Building Menus ..." 3 50 2> /dev/null

  # Create the menus
  awk -f 'parse-menu-sh' -f 'parse-menu' -f 'parse-misc' compiletime_setup.menu > $TEMP
  # Load them up ...
  . $TEMP

}

main() {
  get_defaults

  load_menus

  start

  write_out

  clean_up
}


# Start the whole process ...
main

#!/bin/sh
#
# This program configures DOSEmu. It will either modify a local file, or a
# system file.
#
# Setup is this:
# User selects global or local.
# Global:
#  If there is no global file and global is selected, copy in 
#  './etc/config.dist'.
# Both:
#  Scan global file for "defaults".
# Local:
#  Scan local file for "overrides".
# Both:
#  Handle responses until done.
# Global:
#  Write all changes
# Local:
#  Write out any non-global-default changes.
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
TEMP="$HOME/.dosemu/tmp/runtime_setup.$$"

. select-dialog

clean_up() {
  clear

  rm -f $TEMP
  exit
}


select_configuration_file() {
  local RETVAL RESULT CURRENT

  if [ "@$DIALOG_SUPPORTS_CURRENT" = "@true" ]; then
    CURRENT=1
  fi


  $DIALOG --backtitle "DOSEMU Run-Time Configuration" \
    --title "Select Configuration File" \
    --menu "Choose the Configuration File to read" 8 60 2 $CURRENT \
      1 "System Wide (/etc/dosemu.conf)" \
      2 "Personal (~/.dosrc)" 2> $TEMP
  
  RETVAL=$?
  if [ $RETVAL -eq 0 ]
  then
    RESULT=`cat $TEMP`
    if [ $RESULT -eq 1 ]
    then
      CONF_FILE=/etc/dosemu.conf
      TYPE=system
    else
      CONF_FILE=~/.dosrc
      TYPE=local
    fi
  else
    clean_up
  fi
}

get_defaults() {
  if [ ! -e /etc/dosemu.conf ]
  then
    SYSTEM_FILE=../etc/config.dist
  else
    SYSTEM_FILE=/etc/dosemu.conf
  fi

  $DIALOG --backtitle "DOSEMU Run-Time Configuration" \
    --infobox "Reading System Wide Configuration ..." 3 50 2> /dev/null

  $AWK -f 'parse-config' -f 'parse-misc' -f 'parse-config-sh' -v PREFIX=SYSTEM_ $SYSTEM_FILE > $TEMP
  . $TEMP
  
  if [ $TYPE = local -a -e $CONF_FILE ]
  then
    $DIALOG --backtitle "DOSEMU Run-Time Configuration" \
      --infobox "Reading Local Configuration ..." 3 50 2> /dev/null

    $AWK -f 'parse-config' -f 'parse-misc' -f 'parse-config-sh' -v PREFIX=LOCAL_ $CONF_FILE > $TEMP
    . $TEMP
  fi
}

get_value() {
  local TMP
  eval "TMP=\"\${LOCAL_$2:-\$SYSTEM_$2}\""
  if [ "@$TMP" != "@" ]
  then
    eval "$1=\"$TMP\""
  fi
}

# Here we need a modified call. We only want the local version if it is
# different to the system version.
get_final_value() {
  local TMP_LOCAL TMP_SYSTEM
  eval "TMP_LOCAL=\"\${LOCAL_$2}\";TMP_SYSTEM=\"\${SYSTEM_$2}\""
  if [ "@$TMP_SYSTEM" != "@TMP_LOCAL" ]
  then
    eval "$1=\"\${LOCAL_$2:-\$SYSTEM_$2}\""
  else
    eval "$1=\"\""
  fi
}

set_value() {
  if [ "$TYPE" = "local" ]
  then
    eval "LOCAL_$1=\"$2\""
  else
    eval "SYSTEM_$1=\"$2\""
  fi
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
    get_final_value val "${prefix}_$1"
    if [ "@$val" != "@" -a "@$val" != "@\*" ]; 
    then
      case "$val" in
        "*[ ]*")
          str="$str $1 \\\"$val\\\"" ;;
        *)
          str="$str $1 $val" ;;
      esac
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
    get_final_value val "${prefix}_$1_${suffix}"
    if [ "@$val" != "@" -a "@$val" != "@\*" ]; 
    then
      case "$val" in
        "*[ ]*")
          str="$str $1 \\\"$val\\\"" ;;
        *)
          str="$str $1 $val" ;;
      esac
    fi

    shift
  done

  eval "$var=\$str"
}

form_raw_multi_pairs () {
  local str val

  var=$1
  prefix=$2
  suffix=$3
  shift 3

  while [ $# -gt 1 ]
  do
    val=""
    get_final_value val "${prefix}_$1_${suffix}"
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

  get_final_value value $1

  if [ "@$value" != "@" -a "@$value" != "@\*" ]; 
  then
    echo "config[\"$1\"] = \"$1 $value\"" >> $TEMP
  fi
}

write_boot() {
  local value

  if [ $TYPE = system ];
  then
    get_final_value value boot

    if [ "@$value" != "@" -a "@$value" != "@\*" ]; 
    then
      echo "config[\"boot\"] = \"boot$value\"" >> $TEMP
    fi
  fi
}

write_ems() {
  local value string

  get_final_value value ems_size
  if [ "@$value" != "@" ];
  then
    string="ems_size $value "
  fi

  value=""
  get_final_value value ems_frame
  if [ "@$value" != "@" ];
  then
    string="$string ems_frame $value"
  fi

  if [ "@$string" != "@" -a "@$string" != "@\*" ]; 
  then
    echo "config[\"ems\"] = \"ems { $string }\"" >> $TEMP
  fi
  
}

write_X() {
  local value string

  form_pairs value x update_freq display title icon_name font blinkrate

  get_final_value value x_keycode
  if [ "@$value" = "@on" ];
  then
    string="$string keycode"
  fi

  get_final_value value x_sharecmp
  if [ "@$value" = "@on" ];
  then
    string="$string sharecmp"
  fi

  if [ "@$string" != "@" -a "@$string" != "@\*" ]; 
  then
    echo "config[\"X\"] = \"X { $string }\"" >> $TEMP
  fi

}

write_video() {
  local value string

  form_pairs value video type chipset vbios_seg vbios_size vbios_file

  get_final_value value video_vbios_copy
  if [ "@$value" = "@on" ];
  then
    string="$string vbios_copy"
  fi

  get_final_value value video_vbios_mmap
  if [ "@$value" = "@on" ];
  then
    string="$string vbios_mmap"
  fi

  get_final_value value video_console
  if [ "@$value" = "@on" ];
  then
    string="$string console"
  fi

  get_final_value value video_graphics
  if [ "@$value" = "@on" ];
  then
    string="$string graphics"
  fi

  get_final_value value video_dualmon
  if [ "@$value" = "@on" ];
  then
    string="$string dualmon"
  fi

  get_final_value value video_restore
  if [ "@$value" = "@full" ];
  then
    string="$string fullrestore"
  elif [ "@$value" = "@partial" ];
  then
    string="$string partialrestore"
  fi

  if [ "@$string" != "@" -a "@$string" != "@\*" ]; 
  then
    echo "config[\"video\"] = \"video { $string }\"" >> $TEMP
  fi

}

write_serial() {
  local value string
  local count=1 serial_num=0
  
  get_final_value serial_num serial_num
  if [ "@$serial_num" != "@" ]; then
  while [ $count -le $serial_num ]; do
    value=""
    string=""
    get_final_value value serial_deleted_${count}

    if [ "@$value" != "@on" ]; then
      form_multi_pairs string  serial  $count  com device base irq

      value=""
      get_final_value value serial_mouse_${count}
      if [ "@$value" = "@on" ];
      then
        string="$string mouse"
      fi

      if [ "@$string" != "@" -a "@$string" != "@\*" ]; 
      then
        echo "config[\"serial_${count}\"] = \"serial { $string }\"" >> $TEMP
      fi
    fi

    count=$[$count + 1]
  done
  fi
}

write_mouse() {
  local value string
 
  form_pairs string  mouse  device type

  get_final_value value mouse_internaldriver
  if [ "@$value" = "@1" ];
  then
    string="$string internaldriver"
  fi

  get_final_value value mouse_emulate3buttons
  if [ "@$value" = "@1" ];
  then
    string="$string emulate3buttons"
  fi

  get_final_value value mouse_cleardtr
  if [ "@$value" = "@1" ];
  then
    string="$string cleardtr"
  fi

  if [ "@$string" != "@" -a "@$string" != "@\*" ]; 
  then
    echo "config[\"mouse\"] = \"mouse { $string }\"" >> $TEMP
  fi
}

write_ttylocks() {
  local value string
 
  form_pairs string  ttylocks  directory namestub

  get_final_value value ttylocks_binary
  if [ "@$value" = "@1" ];
  then
    string="$string binary"
  fi

  if [ "@$string" != "@" -a "@$string" != "@\*" ]; 
  then
    echo "config[\"ttylocks\"] = \"ttylocks { $string }\"" >> $TEMP
  fi
}

write_ports() {
  local value string
  local count=1 ports_num=0
  
  get_final_value ports_num ports_num

  if [ "@$ports_num" != "@" ]; then
  while [ $count -le $ports_num ]; do
    value=""
    string=""
    get_final_value value ports_deleted_${count}

    if [ "@$value" != "@on" ]; then
      form_raw_multi_pairs string  ports  $count  ""

      if [ "@$string" != "@" -a "@$string" != "@\*" ]; 
      then
        echo "config[\"port_${count}\"] = \"port { $string }\"" >> $TEMP
      fi
    fi

    count=$[$count + 1]
  done
  fi
}

write_hardware_ram() {
  local value string
  local count=1 hardware_ram_num=0
  
  get_final_value hardware_ram_num hardware_ram_num

  if [ "@$hardware_ram_num" != "@" ]; then
  while [ $count -le $hardware_ram_num ]; do
    value=""
    string=""
    get_final_value value hardware_ram_deleted_${count}

    if [ "@$value" != "@on" ]; then
      form_multi_pairs string  hardware_ram  $count  ""

      if [ "@$string" != "@" -a "@$string" != "@\*" ]; 
      then
        echo "config[\"hardware_ram_${count}\"] = \"hardware_ram { $string }\"" >> $TEMP
      fi
    fi

    count=$[$count + 1]
  done
  fi
}

write_sillyint() {
  local value string
  local count=1 sillyint_num=0
  
  get_final_value sillyint_num sillyint_num

  if [ "@$sillyint_num" != "@" ]; then
  while [ $count -le $sillyint_num ]; do
    value=""
    string=""
    get_final_value value sillyint_deleted_${count}

    if [ "@$value" != "@on" ]; then
      form_pairs string  sillyint  $count  ""

      if [ "@$string" != "@" -a "@$string" != "@\*" ]; 
      then
        echo "config[\"sillyint_${count}\"] = \"sillyint { $string }\"" >> $TEMP
      fi
    fi

    count=$[$count + 1]
  done
  fi
}

write_disk() {
  local value string
  local count=1 disk_num=0
  
  get_final_value disk_num disk_num

  if [ "@$disk_num" != "@" ]; then
  while [ $count -le $disk_num ]; do
    value=""
    string=""
    get_final_value value disk_deleted_${count}

    if [ "@$value" != "@on" ]; then
      form_multi_pairs string  disk  $count  partition wholedisk image bootfile

      if [ "@$string" != "@" -a "@$string" != "@\*" ]; 
      then
        get_final_value value disk_readonly_${count}
        if [ "@$value" = "@on" ];
        then
          string="$string readonly"
        fi

        echo "config[\"disk_${count}\"] = \"disk { $string }\"" >> $TEMP
      fi
    fi

    count=$[$count + 1]
  done
  fi
}

write_floppy() {
  local value string
  local count=1 floppy_num=0
  
  get_final_value floppy_num floppy_num

  if [ "@$floppy_num" != "@" ]; then
  while [ $count -le $floppy_num ]; do
    value=""
    string=""
    get_final_value value floppy_deleted_${count}

    if [ "@$value" != "@on" ]; then
      form_multi_pairs string  floppy  $count  heads sectors tracks file

      if [ "@$string" != "@" -a "@$string" != "@\*" ]; 
      then
        get_final_value value floppy_size_${count}
        string="$string $value"

        echo "config[\"floppy_${count}\"] = \"floppy { $string }\"" >> $TEMP
      fi
    fi

    count=$[$count + 1]
  done
  fi
}

write_bootdisk() {
  local value string
  
  form_pairs string  bootdisk  heads sectors tracks file

  if [ "@$string" != "@ " -a "@$string" != "@\*" ]; 
  then
    get_final_value value bootdisk_size
    string="$string $value"

    echo "config[\"bootdisk\"] = \"bootdisk { $string }\"" >> $TEMP
  fi
}

write_printer() {
  local value string
  local count=1 printer_num=0
  
  get_final_value printer_num printer_num

  if [ "@$printer_num" != "@" ]; then
  while [ $count -le $printer_num ]; do
    value=""
    string=""
    get_final_value value printer_deleted_${count}

    if [ "@$value" != "@on" ]; then
      form_multi_pairs string  printer  $count  command options file base \
	timeout

      if [ "@$string" != "@" -a "@$string" != "@\*" ]; 
      then
        echo "config[\"printer_${count}\"] = \"printer { $string }\"" >> $TEMP
      fi
    fi

    count=$[$count + 1]
  done
  fi
}

write_out() {
  local value

  set_value output "replace"
  handle_Output_File
  get_value value output
  if [ "@$value" = "@abort" ]; then
    return
  elif [ "@$value" = "@new" ]; then
    handle_New_File
    value=""
    get_value value output_file

    if [ "@$value" = "@" ]; then
      clean_up
    else
      CONF_FILE=$value
    fi
  fi

  if [ ! -e $CONF_FILE ]; then
    if [ "$CONF_FILE" = "/etc/dosemu.conf" ]; then
      cp ../etc/config.dist /etc/dosemu.conf
    else
      echo > $CONF_FILE
    fi
  fi

  echo "function setup_config () {" > $TEMP

  write_pairs  debug  config disk warning hardware port read general IPC \
	video write xms ems serial keyb dpmi printer mouse sound
  write_single  dosbanner
  write_single  timint
  write_pairs  keyboard  layout rawkeyboard
  write_serial
  write_mouse
  write_ttylocks
  write_single  hogthreshold
  write_single  ipxsupport
  write_single  pktdriver
  write_pairs  terminal  escchar color charset
  write_X
  write_video
  write_single  allowvideoportaccess
  write_ports
  write_single  mathco
  write_single  cpu
  write_boot
  write_single  umb_max
  write_single  dpmi
  write_single  xms
  write_ems
  write_hardware_ram
  write_single  dosmem
  write_sillyint
  write_single  speaker
  write_disk
  write_floppy
  write_bootdisk
  write_single  emusys
  write_single  fastfloppy
  write_printer
  write_pairs  sound_emu  sb_base sb_irq sb_dma sb_dsp sb_mixer mpu_base
 
  echo "}" >> $TEMP

  $DIALOG --backtitle "DOSEMU Run-Time Configuration" \
    --infobox "Writing Configuration ..." 3 50 2> /dev/null

  cp $CONF_FILE ${CONF_FILE}.old
  $AWK -f 'parse-misc' -f 'write-config' -f $TEMP $CONF_FILE > $CONF_FILE.tmp
  mv $CONF_FILE.tmp $CONF_FILE  
}

MainMenu_End() {
  echo
}

load_menus() {
  $DIALOG --backtitle "DOSEMU Run-Time Configuration" \
    --infobox "Building Menus ..." 3 50 2> /dev/null

  # Create the menus
  $AWK -f 'parse-menu-sh' -f 'parse-menu' -f 'parse-misc' runtime_setup.menu > $TEMP
  # Load them up ...
  . $TEMP

}

main() {
  # Initialise
  set_geometry
  select_dialog

  select_configuration_file

  get_defaults

  load_menus

  start

  write_out

  clean_up
}


# Start the whole process ...
main

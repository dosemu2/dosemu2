#!/bin/sh
#
# This program sends a bug report
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
EXEC_PREFIX="../setup/"
REPORT_FILE="/tmp/dosemu-bug-report.$$"

. ${EXEC_PREFIX}/select-dialog

clean_up() {
  rm -f ${TEMP}*
  exit
}

get_value() {
  local TMP

  eval "TMP=\"\$$2\""

  if [ "@$TMP" != "@" ]
  then
    eval "$1=\"$TMP\""
  fi
}

set_value() {
  eval "$1=\"$2\""
}

set_editor() {
  if [ "@$EDITOR" != "@" ]
  then
    set_value editor $EDITOR
  elif [ "@$VISUAL" != "@" ]
  then
    set_value editor $VISUAL
  else
    set_value editor vi
  fi
}

set_email() {
    set_value from `sh get-email.sh`
}


set_defaults() {
  set_editor
  set_email
  set_value sendreport on
  set_value severity non-critical
  set_value category misc
   touch "$TEMP.description"
  touch "$TEMP.repeat"
  touch "$TEMP.fix"
}

handle_response_value() {
  if [ "$1" = "*\"$2\"*" ]
  then
    set_value "$3" "on"
  else
    set_value "$3" "off"
  fi
}

MainMenu_End() {
  local value
  local reportaction
  local STRING=""

  handle_Report_Action
}

handle_Save_Report() {
  set_value action "save"
  process_report

  # Force an exit!
  clean_up
}

handle_Email_Report() {
  set_value action "email"
  process_report

  # Force an exit!
  clean_up
}

handle_Cancel_Report() {
  set_value action "cancel"
  process_report

  # Force an exit!
  clean_up
}

process_report() {
  reportaction=""
  get_value reportaction action
  if [ "@$reportaction" = "@email" -o "@$reportaction" = "@save" ]
  then

if [ -s $TEMP.description ]; then # avoid-bogus-reports

    $DIALOG --backtitle "DOSEmu Bug Reporting." \
      --infobox "Building Bug Report ..." 3 50 2> /dev/null

    value=""
    get_value value debug_filename
    if [ "@$value" != "@" ]
    then
      DEBUG_STRING="-d $value"
    fi

    # Check the FROM address. If this is blank then we let send-pr decide.
    value=""
    get_value value from
    if [ "@$value" != "@" ]
    then
      LOGNAME=$value
      export LOGNAME
    fi

    (cd ..; gnats/send-pr -P $DEBUG_STRING > "$TEMP.pr")

    echo > $TEMP

    value=""
    get_value value synopsis
    if [ "@$value" != "@" ]
    then
      insert_value synopsis "$value"
    fi

    # Use the synopsis for a subject - since it defaults to ""
    echo "tolower(\$1) == \"subject:\" {" >> $TEMP
    echo "print \$1,\"[DOSEMU BUG] $value\"" >> $TEMP
    echo "next" >> $TEMP
    echo "}" >> $TEMP

    value=""
    get_value value severity
    if [ "@$value" != "@" ]
    then
      insert_value severity "$value"
    fi

    value=""
    get_value value category
    if [ "@$value" != "@" ]
    then
      insert_value category "$value"
    fi

    value=""
    get_value value class
    if [ "@$value" != "@" ]
    then
      insert_value class "$value"
    fi

    insert_value confidential "no"

    insert_file description description
    insert_file "how-to-repeat" repeat
    insert_file "fix" fix

    echo "{ print \$0 }" >> $TEMP

    awk -f $TEMP $TEMP.pr > $TEMP.pr.ready

    clear
    echo "Please Wait: Final Processing ...."

    if [ "@$reportaction" = "@email" ]
    then
      (cd ..; echo "s" | gnats/send-pr --file "$TEMP.pr.ready" | grep -v "a)bort" )
      return 0
    else
      awk  '/SEND-PR:/ { next } { gsub(/<.*>/, ""); print }' "$TEMP.pr.ready" > $REPORT_FILE
      echo "Bug report left in $REPORT_FILE"
      return 0
    fi      

else  # avoid-bogus-reports
  echo "
*** You didn't give any description of the bug, didn't you?
*** Restart submit-bug-report and enter the menu 'Bug Description'
*** ... and give some useful information before posting to us.
***
***                            Thanks in advance, the DOSEMU-Team.
"
  exit 1
fi    # avoid-bogus-reports

  else
    clear
  fi
}

insert_value() {
      echo "tolower(\$1) == \">$1:\" {" >> $TEMP
      echo "print \$1,\"  $2\"" >> $TEMP
      echo "next" >> $TEMP
      echo "}" >> $TEMP
}

insert_file() {
      echo "tolower(\$1) == \">$1:\" {" >> $TEMP
      echo "print \$1" >> $TEMP
      echo "system(\"cat $TEMP.$2\")" >> $TEMP
      echo "print \"\"" >> $TEMP
      echo "next" >> $TEMP
      echo "}" >> $TEMP
}

load_menus() {
  $DIALOG --backtitle "DOSEmu Bug Reporting." \
    --infobox "Building Menus ..." 3 50 2> /dev/null

  # Create the menus
  awk -f "${EXEC_PREFIX}/parse-menu-sh" -f "${EXEC_PREFIX}/parse-menu" -f "${EXEC_PREFIX}/parse-misc" send-pr.menu > $TEMP
  # Load them up ...
  . $TEMP

}

handle_Show_Description() {
    show_file "$TEMP.description"
}

handle_Append_file_to_Description() {
    append_file "description" "description"
}

handle_Edit_Description() {
    edit_file "$TEMP.description"
}

handle_Show_Duplication_information() {
    show_file "$TEMP.repeat"
}

handle_Append_file_to_Duplication_information() {
    append_file "duplication information" "repeat"
}

handle_Edit_Duplication_information() {
    edit_file "$TEMP.repeat"
}

handle_Show_Fix() {
    show_file "$TEMP.fix"
}

handle_Append_file_to_Fix() {
    append_file "fix" "fix"
}

handle_Edit_Fix() {
    edit_file "$TEMP.fix"
}

show_file() {
  $DIALOG --backtitle "DOSEmu Bug Reporting" \
     --textbox $1 20 80 2> /dev/null

  return 0
}

append_file() {
  local DESCRIPTION
  local FILE_EXTN
  local RESULT
  local FILENAME

  DESCRIPTION=$1
  FILE_EXTN=$2

  while [ $FINISHED -ne 1 ]
  do
    $DIALOG --backtitle "DOSEmu Bug Reporting." \
      --title "Input File for $DESCRIPTION" \
      --inputbox "Please enter the name of the file containing the $DESCRIPTION information:" \
      9 60 2> $TEMP
  
    RESULT=$?

    if [ $RESULT -eq 0 -o $RESULT -eq 3 -o $RESULT -eq 6 ]
    then
      FILENAME=`cat $TEMP`
      FINISHED=1
    elif [ $RESULT -eq 1 ]
    then
      $DIALOG --backtitle "DOSEmu Bug Reporting." \
      --msgbox "Please enter the name of the file containing the $DESCRIPTION information. The file will be looked for relative to the top level DOSEmu directory by your default shell. (Warning - the ~ may not work.)" \
      9 60 2> /dev/null
    else
      FINISHED=1
    fi
  done

  if [ "@$FILENAME" != "@" ]
  then
    (cd ..; cat $FILENAME >> "$TEMP.$FILE_EXTN" ) 2> $TEMP

    RESULT=$?

    if [ $RESULT -ne 0 ]
    then
      $DIALOG --backtitle "DOSEmu Bug Reporting." \
      --msgbox "The following error message was returned by the system: `cat $TEMP`" 9 60
    fi
  fi
}

edit_file() {
  local EDIT

  get_value EDIT editor

  eval $EDIT $1

  return 0
}

main() {

  set_defaults

  load_menus

  start

  clean_up
}


# Start the whole process ...
main

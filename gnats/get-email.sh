#!/bin/sh
# Writes a (hopefully) valid email address on STDOUT.
#
# Method:
# Look for & use DOSEMUFROM, else
# Look for & use SKIMFROM, else
# Get the LoginID & hostname
# Look for & use pine's 'reply-to', else
# Look for & set hostname from pine's 'user-domain'
# Look for & set hostname from sendmails 'DM'
# Use LoginID@hostname
#
mkdir -p $HOME/.dosemu/tmp
TEMP=$HOME/.dosemu/tmp/$0.$$

clean_up() {
    rm -f $TEMP
    exit 0
}

if [ "@$DOSEMUFROM" != "@" ]
then
    echo $DOSEMUFROM
    clean_up
elif [ "@$SKIMFROM" != "@" ]
then
    echo $SKIMFROM
    clean_up
else
    # Do it the hard way!

    # Try to find out who we are. 'whoami' should work, but I don't trust
    # it to be set up right ... 8-). $USER certainly works for BASH.
    MYSELF=`whoami || echo $USER`

    # Assume that 'hostname -f' is well enough supported to be the default 
    # choice. I'm assuming that it is at least standard under Linux ...
    # Otherwise hope that 'uname -n' gives the FQDN instead.
    HOSTNAME=`hostname -f || uname -n`

    # Try and get a reply-to from the ~/.pinerc
    if ( grep "^reply-to" $HOME/.pinerc > $TEMP 2> /dev/null )
    then
	echo `cut -c10- $TEMP`
	clean_up
    fi

    if ( grep "^user-domain" $HOME/.pinerc > $TEMP 2> /dev/null )
    then
	HOSTNAME=`cut -c13- $TEMP`
    fi

    if ( grep "^DM" /etc/sendmail.cf > $TEMP 2> /dev/null )
    then
	HOSTNAME=`cut -c3- $TEMP`
    fi

    echo "$MYSELF@$HOSTNAME"
    clean_up
fi

#!/bin/sh

# Notes:
#   1/ this file needs to be installed in the well known location by root
#      cp test/dosemu_tap_interface.sh /bin/.
#      chmod 755 /bin/dosemu_tap_interface.sh
#      chown root:root /bin/dosemu_tap_interface.sh
#
#   2/ sudo needs to be configured by root for the test user to run it
#      without passwd
#      echo 'ajb ALL=(ALL) NOPASSWD: /bin/dosemu_tap_interface.sh' > /etc/sudoers.d/dosemu
#      chmod 0440 /etc/sudoers.d/dosemu

if [ "$1" = "setup" ] ; then
	ip tuntap add mode tap user ${SUDO_UID} tap0
	brctl addif virbr0 tap0
	ifconfig tap0 promisc up

elif [ "$1" = "teardown" ] ; then
	ifconfig tap0 down
	brctl delif virbr0 tap0
# FIXME: this sometimes fails with -EBUSY
#	ip tuntap del mode tap tap0

else
	echo "${0}: incorrect args"
	exit 1
fi

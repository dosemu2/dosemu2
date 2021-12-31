#!/bin/sh

# Notes:
#   1/ this file needs to be installed in the well known location by root
#      cp test/dosemu_fat_mount.sh /bin/.
#      chmod 755 /bin/dosemu_fat_mount.sh
#      chown root.root /bin/dosemu_fat_mount.sh
#
#   2/ sudo needs to be configured by root for the test user to run it
#      without passwd
#      echo 'ajb ALL=(ALL) NOPASSWD: /bin/dosemu_fat_mount.sh' > /etc/sudoers.d/dosemu
#      chmod 0440 /etc/sudoers.d/dosemu

PNT=/mnt/dosemu
IMG=/img/dosemu.img

if [ "$1" = "setup" ] ; then
	[ -d /img ] || mkdir /img
	chown ${SUDO_UID}:${SUDO_GID} /img

elif [ "$1" = "mount" ] ; then
	if [ ! -f ${IMG} ] ; then
		echo "${0}: image missing"
		exit 1
	fi
	if [ ! -d ${PNT} ] ; then
		mkdir -p ${PNT} || exit 1
	fi
	mount -t vfat -o loop,noexec,codepage=437,shortname=win95,uid=${SUDO_UID},gid=${SUDO_GID} ${IMG} ${PNT}

elif [ "$1" = "umount" ] ; then
	umount -f ${PNT}

else
	echo "${0}: incorrect args"
	exit 1
fi

#
# RPM spec file for dosemu
# 
# Copyright (c) 1998 DOSEMU-Development-Team
#
# Note: set 'defaultdocdir:' in your .rpmrc to fit your needs,
#       mkbindist uses this to update the docs and scripts.
#

Vendor:       DOSEMU-Development-Team
Distribution: any i386
Name:         dosemu
Release:      1
Copyright:    GPL version 2, (C) 1994-1998 The DOSEMU-Development-Team
Group:        Applications/Emulators
Provides:     dosemu
#Requires:     mtools tcltk
Packager:     Hans Lermen <lermen@dosemu.org>

Version:      0.99.5
Summary:      The Linux DOS emulator
Source:       dosemu-0.99.5.tgz
Url:          http://www.dosemu.org
Buildroot:    /var/tmp/dosemu

%description
This package allows MS-DOS programs to be started in Linux. A virtual
machine (the DOS box) provides the necessary BIOS functions and emulates
most of the chip devices (e.g. timer, interrupt- and keyboard controler)
Documentation can be found in $RPM_DOC_DIR/dosemu and in the man
page, as well as in the sources.

If you wish to allow non-root users to run the emulator, you must
enter their names in the file '/etc/dosemu.users'. Even so, some DOS
programs will run only if DOSEMU was started by root. Please note as
well that you should only allow 'trusted users' to use DOSEMU, as
it inherently permits hardware access that would otherwise be forbidden
to 'normal' users. However, running a non-suid-root DOSEMU binary
(low feature) won't impact your system more than any other Linux application.


#----------------------------------------------------------------------
%prep
%setup -q

#----------------------------------------------------------------------
%build
./src/tools/mkbindist -c -s -- --host=i386-any-linux --prefix=/usr

#----------------------------------------------------------------------
%install
./src/tools/mkbindist -t $RPM_BUILD_ROOT -d ${RPM_DOC_DIR#/}/dosemu
mkdir -p $RPM_BUILD_ROOT/etc
cp -p ./etc/dosemu.conf $RPM_BUILD_ROOT/etc
cp -p ./etc/dosemu.users.easy $RPM_BUILD_ROOT/etc
cp -p ./etc/dosemu.users.secure $RPM_BUILD_ROOT/etc
(cd $RPM_BUILD_ROOT/etc; ln -sf dosemu.users.easy dosemu.users)
( cd $RPM_BUILD_ROOT; \
  find ./var/lib/dosemu -type d | sed -e 's|\./|%dir /|' | grep -v 'etc/keymap'; \
  find ./etc -type f | sed -e 's|\./|%config /|'; \
  find ./etc -type l | sed -e 's|\./|%config /|'; \
  find ./ -name global.conf | sed -e 's|\./|%config /|'; \
  find ./usr/bin -type f | sed -e 's|\./|/|'; \
  find ./usr/bin -type l | sed -e 's|\./|/|'; \
  find ./usr -type f | sed -e 's|\./|/|' | grep -v '/usr/bin'; \
  find ./var/lib/dosemu -type f | sed -e 's|\./|/|' \
       | awk '!(/etc\/keymap/ || /global.conf/){print}'; \
) > dosemu.files

 
#----------------------------------------------------------------------
%post
( cd /var/lib/dosemu; /usr/bin/mkfatimage16 -b dosC/boot.bin -p \
  -f hdimage.test -l DOSEMU dosC/ipl.sys dosC/kernel.exe \
  dosC/command.com commands/* )
if [ -x /usr/X11R6/bin/mkfontdir ]; then
        (cd /usr/X11R6/lib/X11/fonts/misc; /usr/X11R6/bin/mkfontdir)
fi

#----------------------------------------------------------------------
%postun
if [ -x /usr/X11R6/bin/mkfontdir ]; then
        (cd /usr/X11R6/lib/X11/fonts/misc; /usr/X11R6/bin/mkfontdir)
fi

#----------------------------------------------------------------------
%clean
# gee, hope you didn't override 'Buildroot:' with something stupid,
# ... atleast we check for '/' ;-)
if [ "$RPM_BUILD_ROOT" != "/" ]; then
  rm -rf $RPM_BUILD_ROOT
fi
rm -f dosemu.files

#----------------------------------------------------------------------
%files
#NOTE: %docdir $RPM_DOC_DIR
%files -f dosemu.files

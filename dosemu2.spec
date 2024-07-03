#
# spec file template for dosemu2
#
# Written by Mateusz Viste, stsp
#

Name: dosemu2
Version: 2.0pre9
Release: 2
Summary: fast and secure DOS emulator

Group: System/Emulator

License: GPLv2+
URL: https://github.com/dosemu2/dosemu2
VCS: https://github.com/dosemu2/dosemu2.git
Source0: dosemu2.tar.gz
Source1: etc/sysusers.conf

BuildRequires: SDL2-devel
BuildRequires: SDL2_ttf-devel
BuildRequires: fontconfig-devel
BuildRequires: libXext-devel
BuildRequires: alsa-lib-devel
BuildRequires: fluidsynth-devel
BuildRequires: gpm-devel
BuildRequires: libao-devel
BuildRequires: ladspa-devel
BuildRequires: slang-devel
BuildRequires: libslirp-devel
BuildRequires: libieee1284-devel
BuildRequires: mt32emu-devel
BuildRequires: libbsd-devel
BuildRequires: gcc
BuildRequires: bison
BuildRequires: flex
BuildRequires: gawk
BuildRequires: autoconf
BuildRequires: automake
BuildRequires: make
BuildRequires: sed
BuildRequires: bash
BuildRequires: findutils
BuildRequires: git >= 2.0
BuildRequires: bdftopcf
BuildRequires: mkfontscale
BuildRequires: readline-devel
BuildRequires: json-c-devel
BuildRequires: libb64-devel
BuildRequires: libacl-devel
BuildRequires: libsearpc-devel
BuildRequires: glib2-devel
BuildRequires: binutils
BuildRequires: binutils-x86_64-linux-gnu
BuildRequires: pkgconf-pkg-config
BuildRequires: fdpp-devel
BuildRequires: dj64dev-djdev64-devel
BuildRequires: systemd-rpm-macros
%{?sysusers_requires_compat}

# our startup script is bash-specific
Requires:   bash
Requires:   comcom64
Recommends: fluid-soundfont-gm
Suggests:   timidity++ >= 2.14.0
Recommends: ladspa
# ncurses-base is for terminfo
Recommends: ncurses-base
Recommends: gdb
Recommends: kbd
Recommends: sudo
Suggests:   valgrind
Recommends: install-freedos
Suggests:   install-otherdos

# cannot coexist with dosemu1
Conflicts:  dosemu

%description
dosemu2 is an emulator for running DOS programs under linux.
It can also serve as a VM to boot various DOSes.

%prep
%setup -T -b 0 -q -n dosemu2

%build
./autogen.sh
%configure
make %{?_smp_mflags}

%check

%install
mkdir -p %{buildroot}%{_sysconfdir}/X11/fontpath.d
make DESTDIR=%{buildroot} install

%pre
%sysusers_create_compat %{SOURCE1}

%files
%defattr(-,root,root)
%{_bindir}/*
%{_sysusersdir}/dosemu2.conf
%dir %{_libexecdir}/dosemu2
%attr(06755, dosemu2, dosemu2) %{_libexecdir}/dosemu2/dosemu2.bin
%{_mandir}/man1/*
%lang(ru) %dir %{_mandir}/ru
%lang(ru) %dir %{_mandir}/ru/man1
%lang(ru) %{_mandir}/ru/man1/*
# Not using libdir here as we only install plugins, and their path hard-coded
%{_prefix}/lib/dosemu
%{_datadir}/dosemu
%{_datadir}/applications/dosemu.desktop
%{_datadir}/fonts/oldschool
%{_sysconfdir}/X11/fontpath.d/dosemu2*
%doc %{_docdir}/dosemu2
%dir %{_sysconfdir}/dosemu
%config(noreplace) %{_sysconfdir}/dosemu/dosemu.conf

%changelog
* Tue Apr 09 2024 Stas Sergeev <stsp@users.sourceforge.net> 2.0pre9-2
- 

* Tue Apr 09 2024 Stas Sergeev <stsp@users.sourceforge.net>
- 

* Tue Apr 09 2024 Stas Sergeev <stsp@users.sourceforge.net> 2.0pre9-1
- new package built with tito

* Sat Aug 20 2016 Stas Sergeev <stsp@users.sourceforge.net> 2.0pre6-dev
(none)

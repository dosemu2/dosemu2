dnl Process this file with autoconf to produce a configure script.
AC_INIT([DOSEMU], [],
[http://sourceforge.net/tracker/?atid=457447&group_id=49784&func=browse],dosemu)
AC_CONFIG_SRCDIR(src/dos.c)
AC_PREREQ(2.53)

# first check if we are called from default-configure and if not,
# execute that to parse compiletime settings
if test -z "$DOSEMU_DEFAULT_CONFIGURE" ; then
  eval "set -- $ac_configure_args"
  exec $srcdir/default-configure "$@"
fi

AC_CONFIG_HEADER(src/include/config.h)

dnl Checks for programs.
: ${CFLAGS=""}
AC_PROG_CC
AC_ARG_WITH(target-bits, [  --with-target-bits=bits compile for 32 or 64 bits (default=auto)],
  CPPFLAGS="$CPPFLAGS -m$with_target_bits"
  export CPPFLAGS
  LDFLAGS="$LDFLAGS -m$with_target_bits"
  export LDFLAGS
)
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LEX
if test -z "`echo $LEX | grep flex`" ; then
  AC_CHECK_PROG(lex_ok, $LEX, "yes", "no")
fi
if test "$lex_ok" = "no"; then
  AC_MSG_WARN( )
  AC_MSG_WARN(Your system doesn't seem to have lex or flex available.)
  AC_MSG_ERROR(Install lex or flex and retry.)
fi
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_YACC
dnl The bison problem
if test -z "`echo $YACC | grep bison`" ; then
  AC_MSG_WARN( )
  AC_MSG_WARN(Your system doesn't seem to have bison available.)
  AC_MSG_ERROR(Install bison and retry.)
fi

dnl Checks for libraries.

dnl Checks for header files.
AC_SYS_LARGEFILE
AC_HEADER_MAJOR

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UID_T
AC_CHECK_MEMBERS([struct stat.st_rdev])

AC_CACHE_CHECK([if C compiler has "__FILE__" macro],
ac_cv_c_has_file_macro,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stdio.h>]], [[printf("%s", __FILE__);]])],[ac_cv_c_has_file_macro=yes],[ac_cv_c_has_file_macro=no]) ] )
if test "$ac_cv_c_has_file_macro" = "no" ; then
  AC_DEFINE(__FILE__, "")
fi

AC_CACHE_CHECK([if C compiler has "__LINE__" macro],
ac_cv_c_has_line_macro,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stdio.h>]], [[printf("%s", __LINE__);]])],[ac_cv_c_has_line_macro=yes],[ac_cv_c_has_line_macro=no]) ] )
if test "$ac_cv_c_has_line_macro" = "no" ; then
  AC_DEFINE(__LINE__, "")
fi

AC_CACHE_CHECK([if C compiler has "__FUNCTION__" macro],
ac_cv_c_has_function_macro,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stdio.h>]], [[printf("%s", __FUNCTION__);]])],[ac_cv_c_has_function_macro=yes],[ac_cv_c_has_function_macro=no]) ] )
if test "$ac_cv_c_has_function_macro" = "no" ; then
  AC_DEFINE(__FUNCTION__, "")
fi


dnl Checks for library functions.
AC_CHECK_FUNCS(gettimeofday)
AC_CHECK_FUNCS(sigaltstack)
AC_CHECK_LIB(rt, shm_open)
AC_CHECK_FUNCS(shm_open)

dnl Here is where we do our stuff

AC_ARG_WITH(docdir,
  [  --with-docdir=dir       give the directory used to install documentation]
  [                        (default: ${prefix}/share/doc/dosemu)],
  docdir="$withval", docdir="${prefix}/share/doc/dosemu")
AC_ARG_WITH(x11fontdir,
  [  --with-x11fontdir=dir   give the directory used to install the VGA X11 font]
  [                        (default: ${datadir}/dosemu/Xfonts)],
  x11fontdir="$withval", x11fontdir="${datadir}/dosemu/Xfonts")
AC_ARG_WITH(syshdimagedir,
  [  --with-syshdimagedir=dir give the directory used for system wide boot directories and hdimages]
  [                        (default: /var/lib/dosemu)],
  syshdimagedir="$withval", syshdimagedir="/var/lib/dosemu")
AC_ARG_WITH(fdtarball,
  [  --with-fdtarball=file   give the name of the FreeDOS tarball or 'none']
  [                        (default: dosemu-freedos-bin.tgz)],
  fdtarball="$withval", fdtarball="dosemu-freedos-bin.tgz")

AC_SUBST(docdir)
AC_SUBST(x11fontdir)
AC_SUBST(syshdimagedir)
AC_SUBST(fdtarball)

CONFIG_HOST=`uname -s`
if test "$CONFIG_HOST" = "Linux"; then
  CONFIG_HOST='linux'
fi
AC_SUBST(CONFIG_HOST)

dnl Do ELF if possible

if ! printf '%b\n' '#ifdef __ELF__\n#define foo bar\n#endif\nfoo' \
   | ${CC-cc} -E - | grep bar > /dev/null; then
  if test "$CONFIG_HOST" = "linux"; then
    echo "------------------------------------------------------------"
    echo "Sorry, a.out system detected, but we do no longer support it"
    echo "------------------------------------------------------------"
    exit 1
  fi
fi

dnl Check for static and then also use it for tests

DOSEMU_LDFLAGS="-Wl,-warn-common"
LIBS="$LIBS -ldl -lm"
X_LIBS=""

AC_ARG_ENABLE(linkstatic,
              [  --enable-linkstatic     make statically linked binaries])
if test "$enable_linkstatic" = "yes"; then
              AC_MSG_NOTICE(Linking statically...)
              LDFLAGS="${LDFLAGS} -static"
else
	      AC_MSG_NOTICE(Linking for shared libraries...)
fi

AC_ARG_ENABLE(dlplugins,
              [  --disable-dlplugins     do NOT use dynamically loaded plugins])
if test "$enable_dlplugins" != "no"; then
	      AC_MSG_NOTICE(Using dynamically loaded plugins...)
              AC_DEFINE(USE_DL_PLUGINS)
              USE_DL_PLUGINS="USE_DL_PLUGINS=1"
              AC_SUBST(USE_DL_PLUGINS)
              DOSBIN_LDFLAGS="-rdynamic"
              AC_SUBST(DOSBIN_LDFLAGS)
else
              AC_MSG_NOTICE(Not using dynamically loaded plugins...)
fi

# set DOSEMU_APBUILD to use apgcc
if test -n "$DOSEMU_APBUILD"; then
  USE_APBUILD="USE_APBUILD=1"
  AC_SUBST(USE_APBUILD)
  CC=apgcc
  CPP="apgcc -E"
fi

dnl Check whether we have svgalib and whether to use it

AC_ARG_WITH(svgalib,
	[  --with-svgalib          use svgalib if available])
if test "$with_svgalib" = "yes"; then
  AC_MSG_NOTICE(Checking for svgalib... )
  AC_CHECK_LIB(vga, vga_simple_init,
  [ AC_CHECK_HEADER(vga.h,
    [ AC_MSG_CHECKING(for svgalib version >= 1.9.21)
      svgalib_ver=`printf '%b\n' '#include <vga.h>\nXXAaZZ SVGALIB_VER'|${CC-cc} -E -|awk '/XXAaZZ/ {print $2}' | cut -dx -f2`
      svgalib_ver_a=`expr $svgalib_ver / 10000`
      svgalib_ver_b=`expr $svgalib_ver / 100 % 100`
      svgalib_ver_c=`expr $svgalib_ver % 100`
      if test $svgalib_ver -ge 010921; then
        use_svgalib="yes"
      else
        use_svgalib="no"
      fi
      AC_MSG_RESULT([$use_svgalib, found ${svgalib_ver_a}.${svgalib_ver_b}.${svgalib_ver_c}])
      if test "$use_svgalib" = "no"; then
        AC_MSG_WARN([Your svgalib is too old.])
      fi
    ], AC_MSG_WARN(svgalib headers not found.))
  ], AC_MSG_WARN(svgalib not found.))

  if test "$use_svgalib" = "yes"; then
    AC_MSG_NOTICE(Compiling with svgalib support)
    AC_DEFINE(USE_SVGALIB)
    USE_SVGALIB="USE_SVGALIB=1"
    AC_SUBST(USE_SVGALIB)
    LIBS="-lvga $LIBS"
  else
    AC_MSG_NOTICE(Compiling without svgalib support)
  fi
fi

dnl Check whether we have gpm and whether to use it

AC_ARG_WITH(gpm,
	[  --without-gpm           don't use gpm])
if test "$with_gpm" != "no"; then
  AC_CHECK_LIB(gpm, Gpm_Open, with_gpm="yes", with_gpm="no")
fi
if test "$with_gpm" != "no"; then
  AC_MSG_NOTICE(Compiling with GPM support...)
  AC_DEFINE(USE_GPM)
  USE_GPM="USE_GPM=1"
  AC_SUBST(USE_GPM)
  GPMLIB="-lgpm"
  AC_SUBST(GPMLIB)
  $srcdir/mkpluginhooks enable gpm yes
else
  AC_MSG_NOTICE(Compiling without GPM support...)
  $srcdir/mkpluginhooks enable gpm no
fi

AC_ARG_WITH(alsa,
	[  --without-alsa          don't use alsa even if available])
if test "$with_alsa" != "no"; then
  AC_CHECK_LIB(asound, snd_rawmidi_open, with_alsa="yes", with_alsa="no")
fi
if test "$with_alsa" != "no"; then
  AC_MSG_NOTICE(Compiling with alsa support...)
  AC_DEFINE(USE_ALSA)
  USE_ALSA="USE_ALSA=1"
  AC_SUBST(USE_ALSA)
  ALSALIB="-lasound"
  AC_SUBST(ALSALIB)
  $srcdir/mkpluginhooks enable alsa yes
else
  AC_MSG_NOTICE(Compiling without alsa support...)
  $srcdir/mkpluginhooks enable alsa no
fi

AC_ARG_WITH(sndfile,
	[  --without-sndfile       don't use sndfile even if available])
if test "$with_sndfile" != "no"; then
  AC_CHECK_LIB(sndfile, sf_open, with_sndfile="yes", with_sndfile="no")
fi
if test "$with_sndfile" != "no"; then
  AC_MSG_NOTICE(Compiling with sndfile support...)
  AC_DEFINE(USE_SNDFILE)
  USE_SNDFILE="USE_SNDFILE=1"
  AC_SUBST(USE_SNDFILE)
  SNDFLIB="-lsndfile"
  AC_SUBST(SNDFLIB)
  $srcdir/mkpluginhooks enable sndfile yes
else
  AC_MSG_NOTICE(Compiling without sndfile support...)
  $srcdir/mkpluginhooks enable sndfile no
fi

dnl Check whether we have pthreads and whether to use it

AC_ARG_WITH(pthreads)
if test "$with_pthreads" = "yes"; then
  AC_CHECK_LIB(pthread, pthread_create, , with_pthreads="no")
fi
if test "$with_pthreads" = "yes"; then
  AC_MSG_NOTICE(Compiling with pthreads support...)
  AC_DEFINE(USE_PTHREADS)
  USE_PTHREADS="USE_PTHREADS=1"
  AC_SUBST(USE_PTHREADS)
else
  AC_MSG_NOTICE(Compiling without pthreads support...)
fi

#S-Lang directory checking -- thanks to mutt's configure script.
AC_ARG_WITH(slangdir, [  --with-slangdir=dir     specify S-Lang directory])
AC_MSG_CHECKING(for S-Lang)
SLANGINC=""
SLANGLIB=""
if test "$with_slangdir" = ""; then
  if test -d $srcdir/../slang; then
    SLANGINC="-I ${srcdir}/../slang/src"
    SLANGLIB="-L ${srcdir}/../slang/src/objs"
  else
    if test -d $prefix/include/slang; then
      SLANGINC="-I${prefix}/include/slang"
    elif test -d /usr/include/slang; then
      SLANGINC="-I/usr/include/slang"
    fi
  fi
else
  dnl ---Check to see if $withval is a source directory
  if test -f $with_slangdir/src/slang.h; then
    SLANGINC="-I${with_slangdir}/src"
    SLANGLIB="-L${with_slangdir}/src/objs"
  else
    dnl ---Must be installed somewhere
    if test -d $with_slangdir/include/slang; then
      SLANGINC="-I${with_slangdir}/include/slang"
    elif test -d $with_slangdir/include; then
      SLANGINC="-I${with_slangdir}/include"
    fi
    SLANGLIB="-L${with_slangdir}/lib"
  fi
fi
dnl --- now that we've found it, check the link
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $SLANGINC"
AC_MSG_RESULT($SLANGINC $SLANGLIB)
have_slang="no"
AC_CHECK_HEADER(slang.h,
  AC_CHECK_LIB(slang, SLtt_get_terminfo,
    AC_MSG_NOTICE(Compiling with S-Lang support...)
    have_slang="yes"
    AC_DEFINE(USE_SLANG)
    SLANGLIB="$SLANGLIB -lslang",
    [AC_MSG_WARN([No S-Lang library found.])]
    $srcdir/mkpluginhooks enable term no
  ),
  [AC_MSG_WARN([No S-Lang header file found.])]
  $srcdir/mkpluginhooks enable term no
)
CPPFLAGS="$save_CPPFLAGS"
AC_SUBST(SLANGINC)
AC_SUBST(SLANGLIB)

AC_PATH_XTRA
if test "$have_x" = "yes" ; then
  SLIBS=$LIBS
  LIBS="$LIBS $X_LIBS"
  AC_CHECK_LIB(X11, XOpenDisplay, X_LIBS="$X_LIBS -lX11",
   AC_CHECK_LIB(X11, XCloseDisplay, X_LIBS="$X_LIBS -lX11",
     AC_CHECK_LIB(Xwindow, main, , have_x="no" )))
  LIBS=$SLIBS
  unset SLIBS
fi

if test "$have_x" = "yes" ; then

    SLIBS=$LIBS
    LIBS="$LIBS $X_LIBS"

    AC_MSG_NOTICE(Compiling with X support...)
    AC_DEFINE(X_SUPPORT) 
    X_SUPPORT="X_SUPPORT=1"
    AC_SUBST(X_SUPPORT)

  dnl Check for the MITSHM extention to speed up VGAemu

    AC_ARG_ENABLE(mitshm,
              [  --disable-mitshm        do NOT use the MITSHM X extension])

    if test "$enable_mitshm" != "no" ; then
      AC_CHECK_LIB(Xext, XShmPutImage, enable_mitshm="yes", enable_mitshm="no")
    fi
    if test "$enable_mitshm" != "no" ; then
      AC_CHECK_HEADER(X11/extensions/XShm.h,
         [AC_DEFINE(HAVE_MITSHM,1) X_LIBS="-lXext $X_LIBS"],
         AC_MSG_NOTICE(Compiling without the MITSHM X extension),
         [#include <X11/Xlib.h>])
    fi
    LIBS="-lXext $LIBS"

    AC_ARG_ENABLE(vidmode,
              [  --disable-vidmode       do NOT use the XF86 vidmode extension])
    if test "$enable_vidmode" != "no" ; then
      AC_CHECK_LIB(Xxf86vm, XF86VidModeSwitchToMode, enable_vidmode="yes", enable_vidmode="no")
    fi
    if test "$enable_vidmode" = "no"; then
               AC_MSG_NOTICE(Compiling without the XF86 video mode extension)
    else
	       AC_CHECK_HEADER(X11/extensions/xf86vmode.h,
		 [AC_DEFINE(HAVE_XVIDMODE,1) X_LIBS="-lXxf86vm $X_LIBS"],
                 AC_MSG_NOTICE(Compiling without the XF86 video mode extension),
                 [#include <X11/Xlib.h>])
    fi

    LIBS=$SLIBS
    unset SLIBS

    AC_CHECK_PROG([BDFTOPCF], [bdftopcf], ["yes"], ["no"])
    AC_CHECK_PROG([MKFONTDIR], [mkfontdir], ["yes"], ["no"])
    if test "$MKFONTDIR" = "no" -o "$BDFTOPCF" = "no"; then
        AC_MSG_ERROR([
Your system doesn't seem to have bdftopcf or mkfontdir available.
Please install these packages or a package named like 'xorg-x11-font-utils'
or 'xfonts-utils' and retry.])
     fi

else
  AC_MSG_NOTICE(Compiling without X support...)
fi

PACKAGE_VERSION=`cat ${srcdir}/VERSION`
VV=$PACKAGE_VERSION
VERSION=${VV%%.*}
VV=${VV#*.}
SUBLEVEL=${VV%%.*}
VV=${VV#*.}
PATCHLEVEL1=${VV%%.*}
if test $PATCHLEVEL1 = $VV; then
  PATCHLEVEL2=0
else
  VV=${VV#*.}
  PATCHLEVEL2=${VV%%.*}
fi
DOSEMU_VERSION_CODE=`echo "$(( ( $VERSION * 16777216 ) + ( $SUBLEVEL * 65536 ) + ( $PATCHLEVEL1 * 256 ) + $PATCHLEVEL2 ))"`
PATCHLEVEL="${PATCHLEVEL1}.${PATCHLEVEL2}"
THISVERSION="$VERSION.$SUBLEVEL.$PATCHLEVEL"
EMUVER="$VERSION.$SUBLEVEL"
RELEASE_DATE=`head -n1 $srcdir/ChangeLog | cut '-d ' -f1`

AC_SUBST(VERSION)
AC_SUBST(SUBLEVEL)
AC_SUBST(PATCHLEVEL1)
AC_SUBST(PATCHLEVEL2)
AC_SUBST(RELEASE_DATE)

AC_DEFINE_UNQUOTED(EMUVER, "$EMUVER")
CONFIG_TIME=`date`
AC_DEFINE_UNQUOTED(CONFIG_HOST, "$CONFIG_HOST")
AC_DEFINE_UNQUOTED(CONFIG_TIME, "$CONFIG_TIME")
AC_DEFINE_UNQUOTED(VERSION, $VERSION)
AC_DEFINE_UNQUOTED(DOSEMU_VERSION_CODE, $DOSEMU_VERSION_CODE)
AC_DEFINE_UNQUOTED(SUBLEVEL, $SUBLEVEL)
AC_DEFINE_UNQUOTED(PATCHLEVEL, $PATCHLEVEL)
AC_DEFINE_UNQUOTED(VERSTR, "$THISVERSION")
AC_DEFINE_UNQUOTED(VERDATE, "$RELEASE_DATE")

OPTIONALSUBDIRS="dosext/net/v-net"
REQUIRED="tools commands tools/periph"

dnl enable EXPERIMENTAL stuff
 AC_ARG_ENABLE(experimental,
               [  --enable-experimental   enable configuration of EXPERIMENTAL stuff])
  if test "$enable_experimental" = "yes"; then
   AC_MSG_NOTICE(Allowing EXPERIMENTAL stuff to be configured...);
  else
   AC_MSG_NOTICE(EXPERIMENTAL stuff disabled...);
  fi

if test "$CONFIG_HOST" = "linux"; then
dnl Do vm86plus 
dnl Starting wth dosemu-1.0 we nolonger support the old vm86 interface
dnl but only vm86plus
   AC_DEFINE_UNQUOTED(USE_MHPDBG)
   USE_MHPDBG="USE_MHPDBG=1"
   AC_SUBST(USE_MHPDBG)
   DEBUGGER="arch/$CONFIG_HOST/debugger"
   OPTIONALSUBDIRS="dosext/net/v-net"
   REQUIRED="tools commands tools/periph"
fi
AC_SUBST(DEBUGGER)

dnl try to hook in available plug-ins
if test -x $srcdir/mkpluginhooks; then
  $srcdir/mkpluginhooks
  if test -f plugin_libdirs; then
    PLUGINSUBDIRS=`cat plugin_libdirs`
    AC_MSG_NOTICE(Including plugins: $PLUGINSUBDIRS...);
    if test -f plugin_incdirs; then
      cd src
      for i in `cat ../plugin_incdirs`; do
        if test -f ../configure.ac; then
          for j in $i/*.h; do
            ln -sf ../../$j plugin/include/`basename $j`
          done
        else
	  abssrcdir=`cd .. && cd $srcdir && pwd`
          for j in $abssrcdir/src/$i/*.h; do
            ln -sf $j plugin/include/`basename $j`
          done 
        fi
      done
      cd ..
    fi
  else
    AC_MSG_NOTICE(Compiling without plug-ins...)
  fi
else
  AC_MSG_NOTICE(Compiling without plug-ins...)
fi

dnl Do compilation for GDB
 AC_ARG_ENABLE(debug,
               [  --enable-debug          compile with debug info])
  if test "$enable_debug" = "yes"; then
   AC_MSG_NOTICE(Compiling with debug info...);
  else
   AC_MSG_NOTICE(Compiling without debug info...);
  fi

dnl Compile with ASPI support
 AC_ARG_ENABLE(aspi,
               [  --disable-aspi          do NOT compile with ASPI support])
  if test "$enable_aspi" != "no"; then
   AC_MSG_NOTICE(Compiling with ASPI support...);
   AC_DEFINE(ASPI_SUPPORT)
   AC_SUBST(ASPI_SUPPORT,"ASPI_SUPPORT=1")
  else
   AC_MSG_NOTICE(Compiling without ASPI support...);
  fi
 
dnl Do SB emulation
 AC_ARG_ENABLE(sbemu,
               [  --disable-sbemu         do NOT use SoundBlaster emulator])
 if test "$enable_sbemu" = "no"; then
                AC_MSG_NOTICE(Compiling without SB Emulation...)
 else
                AC_MSG_NOTICE(Compiling with SB Emulation...)
                AC_DEFINE(USE_SBEMU)
                USE_SBEMU="USE_SBEMU=1"
                AC_SUBST(USE_SBEMU)
 fi


dnl DPMI requirements

AC_SUBST(OPTIONALSUBDIRS)
AC_SUBST(REQUIRED)
AC_SUBST(PLUGINSUBDIRS)

dnl Now CFLAGS and INCLUDES

INCDIR="-I\${top_builddir}/src/include -I\${top_builddir}/src/plugin/include"
AC_SUBST(INCDIR)

DOSEMU_CFLAGS="-Wall -Wstrict-prototypes -Wmissing-declarations -Wnested-externs"
if test -z "$CFLAGS"; then
  if test "$enable_debug" = "yes"; then
    CFLAGS="$CFLAGS -g -O"
  else
              # BIG NOTE: do _not_ use -O3,
              # because in forces _inline_ at places where we don't want it !
              # If we want _inline_, well then we 'code' it.
    CFLAGS="-O2 -fomit-frame-pointer"  # never omit the frame pointer when debugging!
  fi
fi
if test "$enable_debug" = "yes"; then
  AC_DEFINE(_DEBUG)
fi

dnl CPU emulator
 AC_ARG_ENABLE(cpuemu,
               [  --disable-cpuemu        do NOT compile with optional x86 emulation code])
  if test "$enable_cpuemu" != "no"; then
   AC_MSG_NOTICE(Compiling with optional 386 emulation code...);
   AC_DEFINE_UNQUOTED(X86_EMULATOR)
   X86_EMULATOR="X86_EMULATOR=1"
   AC_SUBST(X86_EMULATOR)
  else
   AC_MSG_NOTICE(Compiling for native x86...);
  fi


dnl Check for proper gas code16 functionality

if echo '.code16;.text;addr32 pushw 4(%esp)' | as -o dummy.o >/dev/null 2>&1; then
  GASCODE16=1
else
  if echo '.code16;.text;addr32; pushw 4(%esp)' | as -o dummy.o >/dev/null 2>&1; then
    GASCODE16=0
  else
    echo ''
    echo '*** error:'
    echo '*** Your system has no complete code16 functional gas,'
    echo '*** you must upgrade to atleast binutils-2.9.1.0.25'
    echo '*** in order to compile DOSEMU ... giving up'
    echo ''
    exit 1
  fi
fi
rm -f dummy.o
AC_DEFINE_UNQUOTED(GASCODE16, $GASCODE16)

# OPT=-fno-inline
if test "$CONFIG_HOST" = "linux"; then
 AC_MSG_NOTICE(Linux Specific build options...)
 dnl Linux net code
  AC_ARG_ENABLE(net,
               [  --disable-net           do NOT use Linux net code])
  if test "$enable_net" = "no"; then
    AC_MSG_NOTICE(Compiling without network support...);
  else
    AC_MSG_NOTICE(Compiling with network support...);
    AC_DEFINE(USING_NET)
  fi
 PIPE=-pipe
fi


AC_ARG_WITH(target_cpu, [  --with-target-cpu=CPU	  use the specified target CPU (default=auto)])
if test "$with_target_cpu" = ""; then
  AC_MSG_NOTICE(Compiling with default target CPU...)
  target_cpu=""
else
  AC_MSG_NOTICE(Compiling with specified target CPU...)
  target_cpu=$with_target_cpu
fi


OPT=""
if test "$GCC" = "yes" ; then
  AC_MSG_CHECKING(for the version of gcc...)
  minor=`echo "" |${CC-cc} -E -dM -|awk '/__GNUC_MINOR__/{print $3}'`
  major=`echo "" |${CC-cc} -E -dM -|awk '/__GNUC__/{print $3}'`
  GCC_VERSION_CODE=$((($major * 1000) + $minor))
  AC_MSG_RESULT($GCC_VERSION_CODE)
  if test "$(( $GCC_VERSION_CODE >= 2008 ))" = "1"; then
     AC_MSG_CHECKING(for egcs...)
     if "$CC" -v 2>&1|grep 'egcs-' >/dev/null; then
       AC_MSG_RESULT([yes, using -fno-gcse to work around bugs])
       OPT="$OPT -fno-gcse"
     else
       AC_MSG_RESULT(no)
     fi
     AC_MSG_CHECKING(for gcc-2.95 or newer ...)
     if test "$(( $GCC_VERSION_CODE >= 2095 ))" = "1"; then
       AC_MSG_RESULT([yes, using -fno-strict-aliasing to work around bugs])
       OPT="$OPT -fno-strict-aliasing"
       if test "$(( $GCC_VERSION_CODE >= 3000 ))" = "1"; then
         DOSEMU_CPPFLAGS="$DOSEMU_CPPFLAGS -MP"
       fi
     else
       AC_MSG_RESULT(no)
     fi
  fi


  # -m486 is usually in the specs for the compiler
  # but gcc-3.0 wants -mcpu=i486 instead
  # and gcc-3.4 wants -mtune=i486 instead
  # let the compiler options be dependent of the _target_
  # where dosemu is compiled for, not the host
  if test "$target_cpu" = "athlon" -a $GCC_VERSION_CODE -le 2095 ;  then
    target_cpu="i686"
  fi
  if test -z "$target_cpu" ; then
     true
  elif test "$GCC_VERSION_CODE" -ge 3004 ; then
     OPT="$OPT -mtune=$target_cpu"
  elif test "$GCC_VERSION_CODE" -ge 2095 ; then
     OPT="$OPT -mcpu=$target_cpu"
  else
     case "$target_cpu" in
       i486)
         OPT="$OPT -m486"
         ;;
       i386)
         OPT="$OPT -m386"
         ;;
       *)
         ;;
     esac
  fi
  if test "$target_cpu" = "athlon"; then
     OPT="$OPT -malign-functions=4 -fexpensive-optimizations"
  elif test "$target_cpu" = "i486"; then
     OPT="$OPT -malign-loops=2 -malign-jumps=2 -malign-functions=2"
  fi

  AC_MSG_CHECKING(for glibc...)
  set `printf '%b\n' '#include <features.h>\nXXAaZZ __GLIBC__ XXBbZZ __GLIBC_MINOR__'|${CC-cc} -E -|awk '/XXAaZZ/ {print $2 " " $4}'`
  major=$1; minor=$2
  if test "$major" = "__GLIBC__"; then
    AC_MSG_ERROR([Sorry, you need glibc-2.1.3 or newer.])
  else
    if test "$minor" = "__GLIBC_MINOR__"; then
      minor=0;
    fi
    GLIBC_VERSION_CODE=$((($major * 1000) + $minor))
    AC_MSG_RESULT([yes, version code $GLIBC_VERSION_CODE])
    if test $GLIBC_VERSION_CODE -lt 2001; then
      AC_MSG_ERROR([Sorry, you need glibc-2.1.3 or newer.])
    fi
  fi
fi

AC_MSG_CHECKING(for clang...)
if test -z "$ASFLAGS"; then
  ASFLAGS=-Wa,-a
fi
if "$CC" -v 2>&1|grep 'clang' >/dev/null; then
  if "$CC" -no-integrated-as -E - < /dev/null 2>&1|grep 'clang' >/dev/null; then
    AC_MSG_RESULT([yes])
  else
    ASFLAGS="$ASFLAGS -no-integrated-as"
    AC_MSG_RESULT([yes, using -no-integrated-as for assembly language files.])
  fi
else
  CCAS="$CC"
  AC_MSG_RESULT([no])
fi

AC_DEFINE(_GNU_SOURCE)

DOSEMU_CFLAGS="${DOSEMU_CFLAGS} ${OPT} ${PIPE}"
DOSEMU_CPPFLAGS="${DOSEMU_CPPFLAGS} -MMD"
AC_SUBST(ASFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(DOSEMU_CFLAGS)
AC_SUBST(DOSEMU_CPPFLAGS)
AC_SUBST(DOSEMU_LDFLAGS)
AC_SUBST(LIBS)
AC_SUBST(X_LIBS)
AC_SUBST(X_CFLAGS)

dnl Create output files. If you add new ones, please do it in order.

AC_CONFIG_FILES([Makefile.conf dosemu.spec])

AC_CONFIG_COMMANDS([Makefile], 
[ if ! test -f configure.ac; then 
    abssrcdir=`cd $srcdir && pwd`
    mkdir -p `(cd $abssrcdir; find . -name .svn -prune -o -name CVS -prune -o -type d -print)`
    for i in `(cd $abssrcdir; find . -name Makefile)`; do
      echo  ln -sf $abssrcdir/$i $i
      ln -sf $abssrcdir/$i $i
    done
  fi
])

dnl autoheader stuff
AH_TOP([
/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * BIG FAT WARNING:
 *
 * These #defines come from configure.ac.
 * If you edit config.h.in, your efforts will become useless the next 
 * time you run autoheader or autoreconf.
 *
 * All lines within AH_TOP and AH_BOTTOM are copied unchanged into 
 * config.h.in. Read the GNU autoconf documentation to learn more 
 * xabout configure & friends.
 *
 */

#ifndef CONFIG_H
#define CONFIG_H 1

#define VERSION_OF(a,b,c,d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))
#define IS_DEVEL_RELEASE (DOSEMU_VERSION_CODE && 65536)
#define GCC_VERSION_CODE (__GNUC__ * 1000 + __GNUC_MINOR__)

#ifdef __ASM__
/* trick to comment out prototypes */
#define extern #
#else
#include "extern.h"

#include "confpath.h"
/* not overridable file constants */
#define  DOSEMU_RC          ".dosemurc"       /* per user config file */
#define  OLD_DOS_RC         ".dosrc"          /* old, obsolete user config file */
#define  LOCALDIR_BASE_NAME ".dosemu"         /* base directory in $HOME */
#define  DOSEMU_CONF        "dosemu.conf"     /* standard configuration file */
#define  DOSEMU_USERS       "dosemu.users"    /* access right configuration file */
#define  DEFAULT_CONFIG_SCRIPT "builtin"      /* main configuration script */
#define  DOSEMU_LOGLEVEL    "dosemu.loglevel" /* logging configuration file */
#define  DOSEMU_MIDI        "dosemu-midi"     /* fifo for midi daemon */
#define  DOSEMU_MIDI_IN     "dosemu-midi_in"  /* fifo for midi input */

EXTERN char *config_script_name INIT(DEFAULT_CONFIG_SCRIPT);
EXTERN char *config_script_path INIT(0);
EXTERN char *dosemu_users_file_path INIT("/etc/" DOSEMU_USERS);
EXTERN char *dosemu_loglevel_file_path INIT("/etc/" DOSEMU_LOGLEVEL);
EXTERN char *dosemu_rundir_path INIT("~/" LOCALDIR_BASE_NAME "/run");
EXTERN char *dosemu_localdir_path INIT("~/" LOCALDIR_BASE_NAME);

EXTERN char dosemulib_default[] INIT(DOSEMULIB_DEFAULT);
EXTERN char *dosemu_lib_dir_path INIT(dosemulib_default);
EXTERN char dosemuhdimage_default[] INIT(DOSEMUHDIMAGE_DEFAULT);
EXTERN char *dosemu_hdimage_dir_path INIT(dosemuhdimage_default);
EXTERN char keymaploadbase_default[] INIT(DOSEMULIB_DEFAULT "/");
EXTERN char *keymap_load_base_path INIT(keymaploadbase_default);
EXTERN char *keymap_dir_path INIT("keymap/");
EXTERN char *owner_tty_locks INIT("uucp");
EXTERN char *tty_locks_dir_path INIT("/var/lock");
EXTERN char *tty_locks_name_path INIT("LCK..");
EXTERN char *dexe_load_path INIT(dosemuhdimage_default);
EXTERN char *dosemu_midi_path INIT("~/" LOCALDIR_BASE_NAME "/run/" DOSEMU_MIDI);
EXTERN char *dosemu_midi_in_path INIT("~/" LOCALDIR_BASE_NAME "/run/" DOSEMU_MIDI_IN);

#define    DOSEMU_USERS_FILE     dosemu_users_file_path
#define    DOSEMU_LOGLEVEL_FILE  dosemu_loglevel_file_path
#define    RUNDIR                dosemu_rundir_path
#define    LOCALDIR              dosemu_localdir_path
#define    KEYMAP_LOAD_BASE_PATH keymap_load_base_path
#define    KEYMAP_DIR            keymap_dir_path
#define    OWNER_LOCKS           owner_tty_locks
#define    PATH_LOCKD            tty_locks_dir_path
#define    NAME_LOCKF            tty_locks_name_path
#define    DOSEMU_MAP_PATH       dosemu_map_file_name
#define    DOSEMU_MIDI_PATH      dosemu_midi_path
#define    DOSEMU_MIDI_IN_PATH   dosemu_midi_in_path

#endif /* not __ASM__ */
])

AH_TEMPLATE([CONFIG_HOST],
[ Define the host for which Dosemu is configured ])

AH_TEMPLATE([CONFIG_TIME],
[ Define the configure time ])

AH_TEMPLATE([VERSION],
[ Define the Dosemu version ])

AH_TEMPLATE([DOSEMU_VERSION_CODE],
[ Define the Dosemu version code ])

AH_TEMPLATE([VERDATE],
[ Define the Dosemu version release date ])

AH_TEMPLATE([SUBLEVEL],
[ Define the Dosemu sublevel ])

AH_TEMPLATE([PATCHLEVEL],
[ Define the Dosemu patchlevel ])

AH_TEMPLATE([VERSTR],
[ Define the version string ])

AH_TEMPLATE([EMUVER],
[ Define the EMU version ])

AH_TEMPLATE([GASCODE16],
[ Define the stepping of gas code16 functionality ])

AH_TEMPLATE([__FUNCTION__],
[Define if the compiler doesn't have __FUNCTION__ macro
 This is very gcc specific (and very useful!). It is defined
 as "" if the compiler doesn't have it. ])

AH_TEMPLATE([__FILE__],
[ Define if the compiler doesn't have __FILE__ macro.
Most compilers have this one, but just to be sure...])

AH_TEMPLATE([__LINE__],
[ Define if the compiler doesn't have __LINE__ macro.
Most compilers have this one, but just to be sure...])

AH_TEMPLATE([USE_DL_PLUGINS],
[ DEFINE this, if you want dynamically loaded plugins ])

AH_TEMPLATE([USE_SLANG],
[ DEFINE this, if you want S-Lang support ])

AH_TEMPLATE([USE_SVGALIB],
[ DEFINE this, if you want svgalib support ])

AH_TEMPLATE([USE_GPM],
[ DEFINE this, if you want GPM support ])

AH_TEMPLATE([USE_ALSA],
[ DEFINE this, if you want alsa support ])

AH_TEMPLATE([USE_SNDFILE],
[ DEFINE this, if you want wav file output support ])

AH_TEMPLATE([USE_PTHREADS],
[ DEFINE this, if you want pthreads support ])

AH_TEMPLATE([USE_MHPDBG],
[ DEFINE this, if you want the dosdebugger (mhpdbg) ])

AH_TEMPLATE([X_SUPPORT],
[ Define this, if you want X support ])

AH_TEMPLATE([USE_SBEMU],
[ This is DEFINED when SB emulation is required. Just changing this is not
  enough - you must reconfigure. ])

AH_TEMPLATE([HAVE_MITSHM],
[ Define this if you want to use MITSHM ])

AH_TEMPLATE([HAVE_XVIDMODE],
[ Define this if you want to use the XF86 video mode extension ])

AH_TEMPLATE([ASPI_SUPPORT],
[ Define this for ASPI (generic SCSI) support ])

AH_TEMPLATE([X86_EMULATOR],
[ Define this to use the X86 CPU emulator ])

AH_TEMPLATE([_DEBUG],
[ Define this for DEBUG info ])

AH_TEMPLATE([_GNU_SOURCE],
[ Define this if the GNU_SOURCE macro is necessary ])

AH_TEMPLATE([USING_NET],
[ Define if compiling with network support ])

AH_BOTTOM([
#ifndef __ASM__
EXTERN char *dosemu_map_file_name INIT(0);
#endif

#include "plugin_config.h"

#endif /* CONFIG_H */
])

AC_OUTPUT

dnl Run plugin configure scripts
dnl AC_CONFIG_SUBDIRS requires literal which doesn't work too well
dnl with our approach
#AC_CONFIG_SUBDIRS(`touch ./plugin_configure; cat ./plugin_configure`)
for i in `touch ./plugin_configure; cat ./plugin_configure`; do
  popdir=`pwd`
  if test -f ../configure.ac; then
    abs_i=$i
  else
    abs_i=`cd $srcdir && pwd`/$i
  fi
  cd $i
  echo "=== configuring in $i"
  echo "Running $SHELL $abs_i/configure $ac_configure_args ..."
  eval ${SHELL} $abs_i/configure --disable-option-checking $ac_configure_args
  cd $popdir
done

dnl Print some warnings (if neccessary)

if test "$GCC" != "yes" ; then
  AC_MSG_WARN( )
  AC_MSG_WARN(Your system doesn't seem to have the GNU C compiler (gcc).)
  AC_MSG_WARN(Trying to compile with $CC.)
  AC_MSG_WARN(If you are sure you have gcc, set the environment variable)
  AC_MSG_WARN(CC to the complete name of gcc. Examples:)
  AC_MSG_WARN(csh/tcsh: setenv CC /usr/local/bin/gcc)
  AC_MSG_WARN(sh/ksh/bash: CC=/usr/local/bin/gcc ; export CC)
  AC_MSG_WARN(Of course you could also include the gcc bin directory in)
  AC_MSG_WARN(your path.)
fi
  
if test "$have_x" = "no" ; then
  AC_MSG_WARN( )
  AC_MSG_WARN(Compiling without X support.)
  AC_MSG_WARN(Install the X development libraries if you want support for X.)
fi
if test "$have_slang" = "no" ; then
  AC_MSG_WARN( )
  AC_MSG_WARN(Compiling without terminal support.)
  AC_MSG_WARN(Install the slang development libraries if you want to run in a terminal.)
fi
echo ""

dnl Process this file with autoconf to produce a configure script.
AC_INIT([DOSEMU], [],
[http://sourceforge.net/tracker/?atid=457447&group_id=49784&func=browse],dosemu)
AC_CONFIG_SRCDIR(src/base/misc/dosio.c)
AC_PREREQ(2.53)

# first check if we are called from default-configure and if not,
# execute that to parse compiletime settings
if test -z "$DOSEMU_DEFAULT_CONFIGURE" ; then
  exec $srcdir/default-configure "$@"
fi

AC_CONFIG_HEADER(src/include/config.h)

AC_CANONICAL_TARGET([])

dnl Checks for programs.
AC_PROG_CC
AC_PROG_GCC_TRADITIONAL
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
AC_HEADER_DIRENT
AC_HEADER_MAJOR
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_TIME
AC_CHECK_HEADERS(fcntl.h limits.h syslog.h sys/file.h sys/ioctl.h sys/time.h \
    unistd.h termios.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIGNAL
AC_TYPE_SIZE_T
AC_TYPE_UID_T
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_STRUCT_TM

AC_CACHE_CHECK(if C compiler has \"__FILE__\" macro,
ac_cv_c_has_file_macro,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stdio.h>]], [[printf("%s", __FILE__);]])],[ac_cv_c_has_file_macro=yes],[ac_cv_c_has_file_macro=no]) ] )
if test "$ac_cv_c_has_file_macro" = "no" ; then
  AC_DEFINE(__FILE__, "")
fi

AC_CACHE_CHECK(if C compiler has \"__LINE__\" macro,
ac_cv_c_has_line_macro,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stdio.h>]], [[printf("%s", __LINE__);]])],[ac_cv_c_has_line_macro=yes],[ac_cv_c_has_line_macro=no]) ] )
if test "$ac_cv_c_has_line_macro" = "no" ; then
  AC_DEFINE(__LINE__, "")
fi

AC_CACHE_CHECK(if C compiler has \"__FUNCTION__\" macro,
ac_cv_c_has_function_macro,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stdio.h>]], [[printf("%s", __FUNCTION__);]])],[ac_cv_c_has_function_macro=yes],[ac_cv_c_has_function_macro=no]) ] )
if test "$ac_cv_c_has_function_macro" = "no" ; then
  AC_DEFINE(__FUNCTION__, "")
fi


dnl Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_MEMCMP
AC_FUNC_MMAP
AC_FUNC_UTIME_NULL
AC_FUNC_VPRINTF
dnl we currently don't use the following ...
dnl ___AC_CHECK_FUNCS(getcwd gethostname gettimeofday mkdir mkfifo mktime rmdir select socket strdup strerror strstr strtol strtoul uname)
dnl ... except
AC_CHECK_FUNCS(gettimeofday)
AC_CHECK_FUNCS(sigaltstack)

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
  if test ! -f /usr/include/linux/version.h ; then
    echo ''
    echo '*** error:'
    echo '*** Your system has no complete /usr/include/linux,'
    echo '*** (version.h missing) ... giving up'
    echo ''
    exit 1
  fi
fi
AC_SUBST(CONFIG_HOST)

dnl Do ELF if possible

if ! printf '%b\n' '#ifdef __ELF__\n#define foo bar\n#endif\nfoo' \
   | gcc -E - | grep bar > /dev/null; then
  if test "$CONFIG_HOST" = "linux"; then
    echo "------------------------------------------------------------"
    echo "Sorry, a.out system detected, but we do no longer support it"
    echo "------------------------------------------------------------"
    exit 1
  fi
fi

dnl Check for static and then also use it for tests

LDFLAGS="-Wl,-warn-common"

AC_ARG_ENABLE(linkstatic,
              [  --enable-linkstatic     make statically linked binaries])
if test "$enable_linkstatic" = "yes"; then
              AC_MSG_NOTICE(Linking statically...)
              LDFLAGS="${LDFLAGS} -static"
else
	      AC_MSG_NOTICE(Linking for shared libraries...)
fi

dnl Check whether we have svgalib and whether to use it

AC_ARG_WITH(svgalib,
	[  --with-svgalib          use svgalib if available])
if test "$with_svgalib" = "yes"; then
  AC_CHECK_LIB(vga, vga_simple_init, , with_svgalib="no")
fi
if test "$with_svgalib" = "yes"; then
  AC_MSG_NOTICE(Compiling with svgalib support...)
  AC_DEFINE(USE_SVGALIB)
  USE_SVGALIB="USE_SVGALIB=1"
  AC_SUBST(USE_SVGALIB)
else
  AC_MSG_NOTICE(Compiling without svgalib support...)
fi

dnl Check whether we have gpm and whether to use it

AC_ARG_WITH(gpm,
	[  --without-gpm           don't use gpm if available])
if test "$with_gpm" != "no"; then
  AC_CHECK_LIB(gpm, Gpm_Open, , with_gpm="no")
fi
if test "$with_gpm" != "no"; then
  AC_MSG_NOTICE(Compiling with GPM support...)
  AC_DEFINE(USE_GPM)
  USE_GPM="USE_GPM=1"
  AC_SUBST(USE_GPM)
else
  AC_MSG_NOTICE(Compiling without GPM support...)
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
# pthreads causes stability problems so ordinary users shouldn't know about it
#else
#  AC_MSG_NOTICE(Compiling without pthreads support...)
fi

#S-Lang directory checking -- thanks to mutt's configure script.
AC_ARG_WITH(slangdir, [  --with-slangdir=dir     Specify S-Lang directory],
  AC_MSG_CHECKING(for S-Lang)
  SLANGINC=""
  if test "$withval" = "src/plugin/slang"; then
    $srcdir/mkpluginhooks enable slang yes
  elif test "$withval" = "default"; then
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
    if test -f $withval/src/slang.h; then
      SLANGINC="-I${withval}/src"
      SLANGLIB="-L${withval}/src/objs"
    else
      dnl ---Must be installed somewhere
      if test -d $withval/include/slang; then
        SLANGINC="-I${withval}/include/slang"
      elif test -d $withval/include; then
        SLANGINC="-I${withval}/include"
      fi
      SLANGLIB="-L${withval}/lib"
    fi
  fi
  dnl --- now that we've found it, check the link
  if test "$withval" = "src/plugin/slang"; then
   LIBS="$LIBS -lm -ldl"
   AC_MSG_RESULT() 
  else
   AC_MSG_RESULT($SLANGINC $SLANGLIB)
   AC_CHECK_LIB(slang, SLsmg_write_nwchars,
    AC_MSG_WARN([Rejecting system S-Lang with UTF-8 support. It is incompatible with DOSEMU. Using supplied S-Lang.])
    buggy_slang=yes
    $srcdir/mkpluginhooks enable slang yes,
   AC_CHECK_LIB(slang, SLtt_get_terminfo, 
                LIBS="$LIBS -lslang -lm -ldl";LDFLAGS="$LDFLAGS $SLANGLIB",
                [AC_MSG_WARN([No S-Lang library found. Using supplied S-Lang.])]
		$srcdir/mkpluginhooks enable slang yes,
   -lm -ldl), -lm -ldl)
  fi
)

AC_PATH_XTRA
if test "$have_x" = "yes" ; then
  LDFLAGS="$LDFLAGS $X_LIBS"
  CPPFLAGS="$X_CFLAGS"

  AC_CHECK_LIB(X11, XOpenDisplay, , 
   # sometimes statically linked binaries need libdl too ...
   AC_CHECK_LIB(X11, XCloseDisplay, LIBS="$LIBS -lX11 -ldl", 
     AC_CHECK_LIB(Xwindow, main, , have_x="no" ),
  -ldl))
fi

if test "$have_x" = "yes" ; then
    AC_MSG_NOTICE(Compiling with X support...)
    AC_DEFINE(X_SUPPORT) 
    X_SUPPORT="X_SUPPORT=1"
    AC_SUBST(X_SUPPORT)
    AC_DEFINE(X_GRAPHICS)
    X_GRAPHICS="X_GRAPHICS=1"
    AC_SUBST(X_GRAPHICS)


  dnl Check for the MITSHM extention to speed up VGAemu

    AC_ARG_ENABLE(mitshm,
              [  --disable-mitshm        do NOT use the MITSHM X extension])

    if test "$enable_mitshm" != "no" ; then
      AC_CHECK_HEADER(X11/extensions/XShm.h,
         [AC_DEFINE(HAVE_MITSHM,1) LIBS="-lXext $LIBS"],
         AC_MSG_NOTICE(Compiling without the MITSHM X extension),
         [#include <X11/Xlib.h>])
    fi

    AC_ARG_ENABLE(vidmode,
              [  --disable-vidmode       do NOT use the XF86 vidmode extension])
    if test "$enable_vidmode" = "no"; then
               AC_MSG_NOTICE(Compiling without the XF86 video mode extension)
    else
	       AC_CHECK_HEADER(X11/extensions/xf86vmode.h,
		 [AC_DEFINE(HAVE_XVIDMODE,1) LIBS="-lXxf86vm $LIBS"],
                 AC_MSG_NOTICE(Compiling without the XF86 video mode extension),
                 [#include <X11/Xlib.h>])
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
          for j in $srcdir/src/$i/*.h; do
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

dnl Setup to run plugin configure scripts
AC_CONFIG_SUBDIRS(`touch ./plugin_configure; cat ./plugin_configure`)

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
		AC_CHECK_LIB(gus, gus_close,
			[AC_MSG_NOTICE(Compiling Midid with Ultra support...)
			USE_ULTRA="USE_ULTRA=1"
			AC_DEFINE(USE_ULTRA)
			AC_SUBST(USE_ULTRA)],)
                AC_SUBST(USE_SBEMU)
 fi


dnl DPMI requirements

AC_DEFINE(WANT_WINDOWS)

AC_SUBST(OPTIONALSUBDIRS)
AC_SUBST(REQUIRED)
AC_SUBST(PLUGINSUBDIRS)

dnl Now CFLAGS and INCLUDES

INCDIR="-I\${top_builddir}/src/include -I\${top_builddir}/src/plugin/include ${SLANGINC}"
AC_SUBST(INCDIR)

CFLAGS="-Wall -Wstrict-prototypes -Wmissing-declarations -Wnested-externs"
if test "$enable_debug" = "yes"; then
  CFLAGS="$CFLAGS -gstabs+"
  AC_DEFINE(_DEBUG)
  OPT="-O"
else
  OPT="-O2"   # BIG NOTE: do _not_ use -O3,
              # because in forces _inline_ at places where we don't want it !
              # If we want _inline_, well then we 'code' it.
  OPT="$OPT -fomit-frame-pointer"  # never omit the frame pointer when debugging!
fi

dnl CPU emulator
 AC_ARG_ENABLE(cpuemu,
               [  --enable-cpuemu         EXPERIMENTAL, compile with optional x86 emulation code])
  if test "$enable_cpuemu" = "yes" -a "$enable_experimental" = "yes"; then
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


if test "$GCC" = "yes" ; then
  AC_MSG_CHECKING(for the version of gcc...)
  minor=`echo "" |${CC-cc} -E -dM -|awk '/__GNUC_MINOR__/{print $3}'`
  major=`echo "" |${CC-cc} -E -dM -|awk '/__GNUC__/{print $3}'`
  GCC_VERSION_CODE=$((($major * 1000) + $minor))
  AC_MSG_RESULT($GCC_VERSION_CODE)
  AC_MSG_CHECKING(need for pedantic asm constraints...)
  if test "$(( $GCC_VERSION_CODE >= 2008 ))" = "1"; then
     AC_MSG_RESULT(yes)
     AC_DEFINE(ASM_PEDANTIC)
     AC_MSG_CHECKING(for egcs...)
     if gcc -v 2>&1|grep 'egcs-' >/dev/null; then
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
         CPPFLAGS="$CPPFLAGS -MP"
       fi
     else
       AC_MSG_RESULT(no)
     fi
  else
     AC_MSG_RESULT(no)
  fi


  # -m486 is usually in the specs for the compiler
  # but gcc-3.0 wants -mcpu=i486 instead
  # let the compiler options be dependent of the _target_
  # where dosemu is compiled for, not the host
  if test "$target_cpu" = "athlon" -a $GCC_VERSION_CODE -le 2095 ;  then
    target_cpu="i686"
  fi
  if test "$GCC_VERSION_CODE" -ge 2095 ; then
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
  set `printf '%b\n' '#include <features.h>\nXXAaZZ __GLIBC__ XXBbZZ __GLIBC_MINOR__'|gcc -E -|awk '/XXAaZZ/ {print $2 " " $4}'`
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
AC_DEFINE_UNQUOTED(GCC_VERSION_CODE,$GCC_VERSION_CODE)
AC_SUBST(ASM_PEDANTIC)
AC_DEFINE(_GNU_SOURCE)

CFLAGS="${CFLAGS} ${OPT} ${PIPE}"
CPPFLAGS="${CPPFLAGS} -MMD \${INCDIR}"
AC_SUBST(LDFLAGS)
AC_SUBST(LIBS)

dnl Create output files. If you add new ones, please do it in order.

AC_CONFIG_FILES([Makefile.conf dosemu.spec])

AC_CONFIG_COMMANDS([Makefile], 
[ if ! test -f configure.ac; then 
    mkdir -p `(cd $srcdir; find . -type d)`
    for i in `(cd $srcdir; find . -name Makefile)`; do
      echo  ln -sf $srcdir/$i $i
      ln -sf $srcdir/$i $i
    done
  fi
])

dnl autoheader stuff
AH_TOP([
/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
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
EXTERN char *ipx_dos_ini_path INIT(ALTERNATE_ETC "/dos.ini");
EXTERN char *dosemu_midi_path INIT("~/" LOCALDIR_BASE_NAME "/run/" DOSEMU_MIDI);

#define    DOSEMU_USERS_FILE     dosemu_users_file_path
#define    DOSEMU_LOGLEVEL_FILE  dosemu_loglevel_file_path
#define    RUNDIR                dosemu_rundir_path
#define    LOCALDIR              dosemu_localdir_path
#define    KEYMAP_LOAD_BASE_PATH keymap_load_base_path
#define    KEYMAP_DIR            keymap_dir_path
#define    OWNER_LOCKS           owner_tty_locks
#define    PATH_LOCKD            tty_locks_dir_path
#define    NAME_LOCKF            tty_locks_name_path
#define    IPX_DOS_INI_PATH      ipx_dos_ini_path
#define    DOSEMU_MAP_PATH       dosemu_map_file_name
#define    DOSEMU_MIDI_PATH      dosemu_midi_path

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

AH_TEMPLATE([WANT_WINDOWS],
[ DPMI test windows ])

AH_TEMPLATE([USE_SVGALIB],
[ DEFINE this, if you want svgalib support ])

AH_TEMPLATE([USE_GPM],
[ DEFINE this, if you want GPM support ])

AH_TEMPLATE([USE_PTHREADS],
[ DEFINE this, if you want pthreads support ])

AH_TEMPLATE([USE_MHPDBG],
[ DEFINE this, if you want the dosdebugger (mhpdbg) ])

AH_TEMPLATE([X_SUPPORT])
AH_TEMPLATE([X_GRAPHICS],
[ Define these for X support ])

AH_TEMPLATE([USE_SBEMU],
[ This is DEFINED when SB emulation is required. Just changing this is not
  enough - you must reconfigure. ])

AH_TEMPLATE([USE_ULTRA],
[This is DEFINED when the Ultra libraries are available ])

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

AH_TEMPLATE([GCC_VERSION_CODE],
[ Set to GCC version code ])

AH_TEMPLATE([ASM_PEDANTIC],
[ Set if pedantic ASM is needed ])

AH_BOTTOM([
#ifndef __ASM__
EXTERN char *dosemu_map_file_name INIT(0);
#endif

#include "plugin_config.h"

#endif /* CONFIG_H */
])

AC_OUTPUT

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
  
if test "$buggy_slang" = "yes" ; then
  AC_MSG_WARN( )
  AC_MSG_WARN([Rejecting system S-Lang with UTF-8 support. It is incompatible with DOSEMU. Forcing use of the supplied S-Lang plugin.])
fi
if test "$have_x" = "no" ; then
  AC_MSG_WARN( )
  AC_MSG_WARN(Compiling without X support.)
  AC_MSG_WARN(Install the X development libraries if you want support for X.)
fi
echo ""

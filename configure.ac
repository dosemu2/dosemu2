dnl Process this file with autoconf to produce a configure script.
AC_INIT(DOSEMU, `cat VERSION`, http://sourceforge.net/tracker/?atid=457447&group_id=49784&func=browse)
AC_CONFIG_SRCDIR(src/base/misc/dosio.c)
AC_PREREQ(2.52)

# first check if we are called from default-configure and if not,
# execute that to parse compiletime settings
if test -z "$DOSEMU_DEFAULT_CONFIGURE" ; then
  exec ./default-configure $@
fi

AC_CONFIG_HEADER(src/include/config.h)

AC_CANONICAL_SYSTEM

dnl Checks for programs.
AC_PROG_CC
AC_PROG_GCC_TRADITIONAL
dnl AC_PROG_AWK
dnl AC_PROG_INSTALL
AC_PROG_LEX
AC_CHECK_PROG(lex_ok, $LEX, "yes", "no")
if test "$lex_ok" != "yes"; then
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
AC_CHECK_HEADERS(fcntl.h limits.h syslog.h sys/file.h sys/ioctl.h sys/time.h unistd.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIGNAL
AC_TYPE_SIZE_T
AC_TYPE_UID_T
AC_STRUCT_ST_RDEV
AC_STRUCT_TM

AC_CACHE_CHECK(if C compiler has \"__FILE__\" macro,
ac_cv_c_has_file_macro,
[AC_TRY_COMPILE([#include <stdio.h>],[printf("%s", __FILE__);],
  ac_cv_c_has_file_macro=yes,
  ac_cv_c_has_file_macro=no) ] )
if test "$ac_cv_c_has_file_macro" = "no" ; then
  AC_DEFINE(__FILE__, "")
fi

AC_CACHE_CHECK(if C compiler has \"__LINE__\" macro,
ac_cv_c_has_line_macro,
[AC_TRY_COMPILE([#include <stdio.h>],[printf("%s", __LINE__);],
  ac_cv_c_has_line_macro=yes,
  ac_cv_c_has_line_macro=no) ] )
if test "$ac_cv_c_has_line_macro" = "no" ; then
  AC_DEFINE(__LINE__, "")
fi

AC_CACHE_CHECK(if C compiler has \"__FUNCTION__\" macro,
ac_cv_c_has_function_macro,
[AC_TRY_COMPILE([#include <stdio.h>],[printf("%s", __FUNCTION__);],
  ac_cv_c_has_function_macro=yes,
  ac_cv_c_has_function_macro=no) ] )
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

 if ! $CC -v 2>&1|grep specs >/dev/null 2>&1; then
   AC_MSG_WARN(Your GCC installation is incomplete: specs file is missing.)
   AC_MSG_WARN(We assume your GCC compiles for ELF.)
   AC_MSG_WARN( )
   ELF="ELF=1"
 else
   if grep __ELF__ `$CC -v 2>&1|head -n1|cut -d' ' -f4` /dev/null 1>/dev/null 2>&1; then 
    ELF="ELF=1"
   else
     if test "$CONFIG_HOST" = "linux"; then
      echo "------------------------------------------------------------"
      echo "Sorry, a.out system detected, but we do no longer support it"
      echo "------------------------------------------------------------"
      exit 1
     else
      # DON'T CHANGE THIS: this makes libdosemu start high enough to be safe.
      # should be okay at...0x20000000 for .5 GB mark.
      LIBSTART="LIBSTART = 0x20000000"
     fi
   fi
 fi
 AC_SUBST(LIBSTART)
 AC_SUBST(ELF)

dnl Check for static and then also use it for tests

LNKOPTS="-Wl,-warn-common"

AC_ARG_ENABLE(linkstatic,
              [  --enable-linkstatic     make statically linked binaries],
              [AC_CHECKING("Linking statically")
               LNKOPTS="${LNKOPTS} -static"
	       CFLAGS="-static $CFLAGS"],
	      [AC_CHECKING("Linking for shared libraries")])

dnl Do check wether we have svgalib and whether to use it

AC_ARG_ENABLE(use-svgalib,
	[  --enable-use-svgalib    use svgalib if available],
	[use_svgalib="yes"],[use_svgalib="no"])
if test "$use_svgalib" = "yes"; then
  AC_CHECK_LIB(vga, vga_simple_init, , use_svgalib="no", -lm)
fi
if test "$use_svgalib" = "yes"; then
  AC_CHECKING("compiling with svgalib support")
  AC_DEFINE(USE_SVGALIB)
  USE_SVGALIB="USE_SVGALIB=1"
  AC_SUBST(USE_SVGALIB)
else
  AC_CHECKING("compiling without svgalib support")
fi

dnl Do check wether we have pthreads and whether to use it

AC_ARG_ENABLE(use-pthreads,,
	[use_pthreads="yes"],[use_pthreads="no"])
if test "$use_pthreads" = "yes"; then
  AC_CHECK_LIB(pthread, pthread_create, , use_pthreads="no", -lm)
fi
if test "$use_pthreads" = "yes"; then
  AC_CHECKING("compiling with pthreads support")
  AC_DEFINE(USE_PTHREADS)
  USE_PTHREADS="USE_PTHREADS=1"
  AC_SUBST(USE_PTHREADS)
# pthreads causes stability problems so ordinary users shouldn't know about it
#else
#  AC_CHECKING("compiling without pthreads support")
fi

#S-Lang directory checking -- thanks to mutt's configure script.
AC_ARG_WITH(slangdir, [  --with-slangdir=dir     Specify S-Lang directory],
  AC_MSG_CHECKING(for S-Lang)
  if test "$withval" = "default"; then
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
  CFLAGS="$CFLAGS $SLANGINC $SLANGLIB -lm"
  dnl --- now that we've found it, check the link
  AC_CHECK_LIB(slang, SLtt_get_terminfo,
                [SLANGLIB="$SLANGLIB -lslang -lm"],
                [AC_MSG_ERROR(unable to compile.  install slang-devel or check config.log)], -lm)
)

AC_SUBST(SLANGLIB)

dnl check for X. This was reproduced from TKined

AC_CHECK_PROG(x_ok, xmkmf, 1, 0)
if test $x_ok = 1 -a "${with_x+set}" != set; then
    AC_PATH_X
fi
if test "$XINCLUDE_DIR" != ""; then
    x_includes="$XINCLUDE_DIR"
fi
if test "$x_includes" = /usr/include; then
    XINCLUDES="# no special path needed"
elif test "$x_includes" != "NONE"; then
    XINCLUDES="-I$x_includes"
fi
if test "$XINCLUDES" = nope; then
  AC_WARN(Couldn't find any X11 include files)
  XINCLUDES="# no include files found"
fi
if test "$XINCLUDES" = "-I"; then
  XINCLUDES=""
fi
AC_SUBST(XINCLUDES)

if test "$XLIBRARY_DIR" != ""; then
    x_libraries="$XLIBRARY_DIR"
fi
if test "$x_libraries" = /usr/lib; then
    XLIBSWD="#no special path needed"
elif test "$x_libraries" != "NONE"; then
    XLIBSWD="-L$x_libraries"
else
    AC_CHECKING(for X11 library archive)
    AC_CHECK_LIB(X11, main, XLIBSWL="-lX11", XLIBSWL=nope)
    if test "$XLIBSWL" = nope; then
	dirs="/usr/unsupported/lib /usr/local/lib /usr/X386/lib /usr/lib/X11R4 /usr/X11R5/lib /usr/lib/X11R5 /usr/openwin/lib /usr/X11/lib"
	for i in $dirs ; do
	    if test -r $i/libX11.so; then
		XLIBSWL="-lX11"
		XLIBSWD="-L$i"
	    fi
	done
    fi
fi
CFLAGS="$CFLAGS $XLIBSWD"
AC_CHECK_LIB(X11, XOpenDisplay, XLIBSWL="-lX11", XLIBSWL=nope)
dnl sometimes statically linked binaries need libdl too ...
if test "$XLIBSWL" = nope ; then
    AC_CHECK_LIB(X11, XCloseDisplay, XLIBSWL="-lX11 -ldl", XLIBSWL=nope, -ldl)
fi
if test "$XLIBSWL" = nope ; then
    AC_CHECK_LIB(Xwindow, main, XLIBSWL=-lXwindow)
fi
if test "$with_x" = "no"; then
  XLIBSWL=nope
fi
if test "$XLIBSWL" = nope ; then
    AC_WARN(Couldn't find the X11 library archive.)
    AC_CHECKING(Compiling without X support)
    XLIBSWL=""
    XLIBSWD=""
    X_SUPPORT=""
    AC_SUBST(X_SUPPORT)
    X_GRAPHICS=""
    AC_SUBST(X_GRAPHICS)
    X2CFILES=""
    AC_SUBST(X2CFILES)
    X2CEXE=""
    AC_SUBST(X2CEXE)
else
    AC_CHECKING(Compiling with X support)
    X_SUPPORT="X_SUPPORT=1"
    AC_DEFINE_UNQUOTED(X_SUPPORT) 
    AC_SUBST(X_SUPPORT)
    X_GRAPHICS="X_GRAPHICS=1"
    AC_DEFINE_UNQUOTED(X_GRAPHICS)
    AC_SUBST(X_GRAPHICS)
    

dnl All the X2 specs
    AC_DEFINE_UNQUOTED(X2_SUPPORT) 
    X2CFILES="x2dos.c"
    AC_SUBST(X2CFILES)
    X2CEXE="x2dos ../etc/xtermdos ../etc/xinstallvgafont"
    AC_SUBST(X2CEXE)

fi


dnl Check for the MITSHM extention to speed up VGAemu

AC_ARG_ENABLE(mitshm,
              [  --enable-mitshm         force use of MITSHM X extension],
              [forced_mitshm="yes"],[forced_mitshm="no"])

AC_ARG_ENABLE(nomitshm,
              [  --enable-nomitshm       do NOT use the MITSHM X extension],
              [forced_nomitshm="yes"],[forced_nomitshm="no"])

if test "$forced_mitshm" = "no" -a "$forced_nomitshm" = "no" ; then
  AC_CHECK_HEADER(X11/extensions/XShm.h,have_mitshm="yes",have_mitshm="no")
else
  if test "$forced_mitshm" = "yes" -a "$forced_nomitshm" = "no" ; then
    have_mitshm="yes"
  fi

  if test "$forced_mitshm" = "no" -a "$forced_nomitshm" = "yes" ; then
    have_mitshm="no"
  fi

  if test "$forced_mitshm" = "yes" -a "$forced_nomitshm" = "yes" ; then
    AC_MSG_ERROR(Can't use --enable-mitshm and --enable-nomitshm at the same time)
  fi
fi

if test "$have_mitshm" = "yes" -a "$X_SUPPORT" = "X_SUPPORT=1"; then
  XLIBSWL="-lXext $XLIBSWL"
  AC_DEFINE(HAVE_MITSHM,1)
fi

XLIBSW="$XLIBSWD $XLIBSWL"

AC_SUBST(XLIBSW)
AC_SUBST(x_libraries)

REALTOPDIR=`nolinks=1; pwd`
VV=`cat VERSION`
VERSION=${VV%%.*}
VV=${VV#*.}
SUBLEVEL=${VV%%.*}
VV=${VV#*.}
PATCHLEVEL=${VV%%.*}
if test $PATCHLEVEL = $VV; then
  PATCHLEVEL_=0
else
  VV=${VV#*.}
  PATCHLEVEL_=${VV%%.*}
fi
DOSEMU_VERSION_CODE=`echo "$(( ( $VERSION * 16777216 ) + ( $SUBLEVEL * 65536 ) + ( $PATCHLEVEL * 256 ) + $PATCHLEVEL_ ))"`
PATCHLEVEL="${PATCHLEVEL}.${PATCHLEVEL_}"
THISVERSION="$VERSION.$SUBLEVEL.$PATCHLEVEL"
EMUVER="$VERSION.$SUBLEVEL"
RELEASE_DATE=`head -n1 $REALTOPDIR/ChangeLog | cut '-d ' -f1`

AC_DEFINE_UNQUOTED(EMUVER, "$EMUVER")
AC_SUBST(EMUVER)
CONFIG_TIME=`date`
AC_SUBST(REALTOPDIR)
AC_SUBST(SUBLEVEL)
AC_SUBST(PATCHLEVEL)
AC_SUBST(THISVERSION)
AC_SUBST(VERSION)
AC_SUBST(DOSEMU_VERSION_CODE)
BINPATH="$REALTOPDIR/$THISVERSION"
AC_SUBST(BINPATH)
SRCPATH="$REALTOPDIR/src"
AC_SUBST(SRCPATH)
AC_DEFINE_UNQUOTED(CONFIG_HOST, "$CONFIG_HOST")
AC_DEFINE_UNQUOTED(CONFIG_TIME, "$CONFIG_TIME")
AC_DEFINE_UNQUOTED(VERSION, $VERSION)
AC_DEFINE_UNQUOTED(DOSEMU_VERSION_CODE, $DOSEMU_VERSION_CODE)
AC_DEFINE_UNQUOTED(SUBLEVEL, $SUBLEVEL)
AC_DEFINE_UNQUOTED(PATCHLEVEL, $PATCHLEVEL)
AC_DEFINE_UNQUOTED(VERSTR, "$THISVERSION")
AC_DEFINE_UNQUOTED(VERDATE, "$RELEASE_DATE")

OPTIONALSUBDIRS="dosext/net/v-net"
#OPTIONALSUBDIRS="dosext/net/v-net commands"
REQUIRED="tools commands tools/periph"
#REQUIRED="tools tools/periph"

dnl enable EXPERIMENTAL stuff
 AC_ARG_ENABLE(experimental,
               [  --enable-experimental   enable configuration of EXPERIMENTAL stuff],
               [experimental="yes"],[experimental="no"])
  if test "$experimental" = "yes"; then
   AC_CHECKING("allowing EXPERIMENTAL stuff to be configured");
  else
   AC_CHECKING("EXPERIMENTAL stuff disabled");
  fi

if test "$CONFIG_HOST" = "linux"; then
dnl Do vm86plus 
dnl Starting wth dosemu-1.0 we nolonger support the old vm86 interface
dnl but only vm86plus
   AC_DEFINE_UNQUOTED(USE_MHPDBG)
   USE_MHPDBG="-DUSE_MHPDBG"
   AC_SUBST(USE_MHPDBG)
   DEBUGGER="arch/$CONFIG_HOST/debugger"
   OPTIONALSUBDIRS="dosext/net/v-net"
#   OPTIONALSUBDIRS="dosext/net/v-net commands"
   REQUIRED="tools commands tools/periph"
#   REQUIRED="tools tools/periph"
fi
AC_SUBST(DEBUGGER)

dnl try to hook in available plug-ins
if test -x ./mkpluginhooks; then
  ./mkpluginhooks
  if test -f ./plugin_libdirs; then
    PLUGINSUBDIRS=`cat ./plugin_libdirs`
    AC_CHECKING("Including plugins: $PLUGINSUBDIRS");
    if test -f ./plugin_incdirs; then
      for i in `cat ./plugin_incdirs`; do
        PLUGININC="$PLUGININC -I\${SRCPATH}/${i}"
      done
    fi
  else
    AC_CHECKING("Compiling without plug-ins")
  fi
else
  AC_CHECKING("Compiling without plug-ins")
fi

dnl Setup to run plugin configure scripts
AC_CONFIG_SUBDIRS(`touch ./plugin_configure; cat ./plugin_configure`)

dnl Do compilation for GDB
 AC_ARG_ENABLE(dodebug,
               [  --enable-dodebug        do compile for with debug info],
               [dodebug="yes"],[dodebug="no"])
  if test "$dodebug" = "yes"; then
   AC_CHECKING("Compiling with debug info");
  else
   AC_CHECKING("Compiling without debug info");
  fi

dnl Compile with ASPI support
 AC_ARG_ENABLE(aspi,
               [  --enable-aspi           do compile with ASPI support],
	       [aspi="yes"],[aspi="no"])
  if test "$aspi" = "yes"; then
   AC_CHECKING("Compiling with ASPI support");
   ASPI_SUPPORT="ASPI_SUPPORT=1"
   AC_DEFINE_UNQUOTED(ASPI_SUPPORT)
   AC_SUBST(ASPI_SUPPORT)
  else
   AC_CHECKING("Compiling without ASPI support");
  fi
 
dnl Do SB emulation
 AC_ARG_ENABLE(nosbemu,
               [  --enable-nosbemu        do NOT use SoundBlaster emulator],
               [AC_CHECKING("Compiling without SB Emulation")],
               [AC_CHECKING("Compiling with SB Emulation")
                AC_DEFINE(USE_SBEMU)
                USE_SBEMU="USE_SBEMU=1"
		AC_CHECK_LIB(gus, gus_close,
			[AC_CHECKING("Compiling Midid with Ultra support")
			USE_ULTRA="USE_ULTRA=1"
			AC_DEFINE(USE_ULTRA)
			AC_SUBST(USE_ULTRA)],)
                AC_SUBST(USE_SBEMU)])


dnl DPMI requirements

AC_DEFINE_UNQUOTED(WANT_WINDOWS)
WANT_WINDOWS="WANT_WINDOWS=-DWANT_WINDOWS"
AC_SUBST(WANT_WINDOWS)

AC_SUBST(OPTIONALSUBDIRS)
AC_SUBST(REQUIRED)
AC_SUBST(PLUGINSUBDIRS)

dnl Now CFLAGS and INCLUDES

# does this work if you do make -C <some dir>
TOPDIR=`nolinks=1;pwd`
AC_SUBST(TOPDIR)

# BIG NOTE: XINCLUDES _must_ come last, because some joker did preset
# it with a comment line (such as: XINCLUDES="# no special path needed")
# We need to fix this, because this is a time bomb. --HL

INCDIR="${PLUGININC} -I\${SRCPATH}/include -I\${SRCPATH}/dosext/dpmi ${SLANGINC} ${XINCLUDES}"
AC_SUBST(INCDIR)

if test "$dodebug" = "yes"; then
  COPTFLAGS=" -g -Wall -D_DEBUG"
  OPT="-O"
else
  COPTFLAGS=" -Wall"
  OPT="-O2"   # BIG NOTE: do _not_ use -O3,
              # because in forces _inline_ at places where we don't want it !
              # If we want _inline_, well then we 'code' it.
  OPT="$OPT -fomit-frame-pointer"  # never omit the frame pointer when debugging!
fi

dnl CPU emulator
 AC_ARG_ENABLE(cpuemu,
               [  --enable-cpuemu         EXPERIMENTAL, compile with optional x86 emulation code],
	       [cpuemu="yes"],[cpuemu="no"])
  if test "$cpuemu" = "yes" -a "$experimental" = "yes"; then
   AC_CHECKING("Compiling with optional 386 emulation code");
   X86_EMULATOR="X86_EMULATOR=1"
   AC_DEFINE_UNQUOTED(X86_EMULATOR)
   AC_SUBST(X86_EMULATOR)
  else
   AC_CHECKING("Compiling for native x86");
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

OPT="$OPT -MMD" # this should give painless dependencies

# OPT=-fno-inline
if test "$CONFIG_HOST" = "linux"; then
 AC_CHECKING("Linux Specific build options")
 dnl Linux net code
  AC_ARG_ENABLE(nonet,
               [  --enable-nonet          do NOT use Linux net code],
               [nonet="yes"],[nonet="no"])
  if test "$nonet" = "yes"; then
    AC_CHECKING("Compiling without network support");
    USING_NET=""
  else
    AC_CHECKING("Compiling with network support");
    USING_NET="-DUSING_NET"
  fi
 PIPE=-pipe
fi


if test "$GCC" = "yes" ; then
  echo -n "generating gcc version code..."
  minor=`echo "" |${CC-cc} -E -dM -|awk '/__GNUC_MINOR__/{print $3}'`
  major=`echo "" |${CC-cc} -E -dM -|awk '/__GNUC__/{print $3}'`
  GCC_VERSION_CODE=$((($major * 1000) + $minor))
  echo "$GCC_VERSION_CODE"
  echo -n "checking need for pedantic asm constraints..."
  if test "$(( $GCC_VERSION_CODE >= 2008 ))" = "1"; then
     echo "yes"
     ASM_PEDANTIC="-DASM_PEDANTIC"
     echo -n "checking for egcs..."
     if gcc -v 2>&1|grep 'egcs-' >/dev/null; then
       echo "yes, using -fno-gcse to work around bugs"
       EGCSFLAGS="-fno-gcse"
     else
       echo "no"
     fi
     echo -n "checking for gcc-2.95..."
     if test "$(( $GCC_VERSION_CODE >= 2095 ))" = "1"; then
       echo "yes, using -fno-strict-aliasing to work around bugs"
       GCC295FLAGS="-fno-strict-aliasing"
     else
       echo "no"
     fi
  else
     echo "no"
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
  GCC_VERSION_CODE="-DGCC_VERSION_CODE=$GCC_VERSION_CODE"

  echo -n "checking for glibc..."
  set `echo -e '#include <features.h>\nXXAaZZ __GLIBC__ XXBbZZ __GLIBC_MINOR__'|gcc -E -|awk '/XXAaZZ/ {print $2 " " $4}'`
  major=$1; minor=$2
  if test "$major" = "__GLIBC__"; then
    echo "no"
  else
    if test "$(( $major > 1 ))" = "1"; then
      GLIBCFLAGS="-D_XOPEN_SOURCE"
    fi
    if test "$minor" = "__GLIBC_MINOR__"; then
      minor=0;
    fi
    GLIBC_VERSION_CODE=$((($major * 1000) + $minor))
    echo "yes, version code $GLIBC_VERSION_CODE"
    if test $GLIBC_VERSION_CODE -le 2001; then
      echo Sorry, you need glibc-2.1.3 or newer.
      exit 1
    fi
  fi
fi
AC_SUBST(GCC_VERSION_CODE)
AC_SUBST(ASM_PEDANTIC)
AC_SUBST(GLIBCFLAGS)

CFLAGS="-D_GNU_SOURCE ${OPT} ${EGCSFLAGS} ${GCC295FLAGS} ${GCC_VERSION_CODE} ${ASM_PEDANTIC} ${GLIBCFLAGS} ${PIPE} ${USING_NET} ${CDEBUGOPTS} ${COPTFLAGS} ${INCDIR}"



# set to use a simpler fork for unix command
# CFLAGS+=-DSIMPLE_FORK
# set to debug fork with environment
# CFLAGS+=-DFORK_DEBUG

# We need to use the C_RUN_IRQS with -fno-inline (TBD why)
# this is in pic/pic.c
#CFLAGS+=-DC_RUN_IRQS

# use fd3 for soft errors, stderr for hard error, don't ope
# stderr to /dev/null
# CFLAGS+=-DUSE_FD3_FOR_ERRORS

LDFLAGS="${LNKOPTS}" # exclude symbol information
AC_SUBST(LDFLAGS)

dnl Create output files. If you add new ones, please do it in order.

AC_OUTPUT([Makefile.conf])

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

echo ""
echo "Configure finished. Do 'make' to compile Dosemu."
echo ""

dnl autoheader stuff
AH_TOP([
/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
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

#ifndef __ASM__
#include "extern.h"

/* not overridable file/path constants */
#define  DOSEMU_RC          ".dosemurc"       /* per user config file */
#define  OLD_DOS_RC         ".dosrc"          /* old, obsolete user config file */
#define  LOCALDIR_BASE_NAME ".dosemu"         /* base directory in $HOME */
#define  DOSEMU_CONF        "dosemu.conf"     /* standard configuration file */
#define  DOSEMU_USERS       "dosemu.users"    /* access right configuration file */
#define  DEFAULT_CONFIG_SCRIPT "builtin"      /* main configuration script */
#define  DOSEMU_LOGLEVEL    "dosemu.loglevel" /* logging configuration file */
#define  ALTERNATE_ETC      "/etc/dosemu"     /* alternate config. directory */
#define  DOSEMU_MIDI        "dosemu-midi"     /* fifo for midi daemon */

EXTERN char *config_file_path INIT("/etc/" DOSEMU_CONF);
EXTERN char *config_script_name INIT(DEFAULT_CONFIG_SCRIPT);
EXTERN char *config_script_path INIT(0);
EXTERN char *dosemu_users_file_path INIT("/etc/" DOSEMU_USERS);
EXTERN char *dosemu_loglevel_file_path INIT("/etc/" DOSEMU_LOGLEVEL);
EXTERN char *dosemu_tmpfile_path INIT("~/" LOCALDIR_BASE_NAME "/run/dosemu.");
EXTERN char *dosemu_tmpdir_path INIT("~/" LOCALDIR_BASE_NAME "/tmp");
EXTERN char *dosemu_tmpdir_process_path INIT("~/" LOCALDIR_BASE_NAME "/tmp/<pid>");
EXTERN char *dosemu_rundir_path INIT("~/" LOCALDIR_BASE_NAME "/run");
EXTERN char *dosemu_localdir_path INIT("~/" LOCALDIR_BASE_NAME);

#define config_xstr(s) config_str(s)
#define config_str(s) #s

EXTERN char *dosemu_lib_dir_path INIT(config_xstr(DOSEMULIB_DEFAULT));
EXTERN char *dosemu_hdimage_dir_path INIT(config_xstr(DOSEMUHDIMAGE_DEFAULT));
EXTERN char *keymap_load_base_path INIT(config_xstr(DOSEMULIB_DEFAULT) "/");
EXTERN char *keymap_dir_path INIT("keymap/");
EXTERN char *owner_tty_locks INIT("uucp");
EXTERN char *tty_locks_dir_path INIT("/var/lock");
EXTERN char *tty_locks_name_path INIT("LCK..");
EXTERN char *dexe_load_path INIT(config_xstr(DOSEMUHDIMAGE_DEFAULT));
EXTERN char *ipx_dos_ini_path INIT(ALTERNATE_ETC "/dos.ini");
EXTERN char *dosemu_midi_path INIT("~/" LOCALDIR_BASE_NAME "/run/" DOSEMU_MIDI);

#define    CONFIG_FILE           config_file_path
#define    DOSEMU_USERS_FILE     dosemu_users_file_path
#define    DOSEMU_LOGLEVEL_FILE  dosemu_loglevel_file_path
#define    TMPFILE               dosemu_tmpfile_path
#define    TMPDIR                dosemu_tmpdir_path
#define    TMPDIR_PROCESS        dosemu_tmpdir_process_path
#define    RUNDIR                dosemu_rundir_path
#define    LOCALDIR              dosemu_localdir_path
#define    DOSEMU_LIB_DIR        dosemu_lib_dir_path
#define    DOSEMU_HDIMAGE_DIR    dosemu_hdimage_dir_path
#define    KEYMAP_LOAD_BASE_PATH keymap_load_base_path
#define    KEYMAP_DIR            keymap_dir_path
#define    OWNER_LOCKS           owner_tty_locks
#define    PATH_LOCKD            tty_locks_dir_path
#define    NAME_LOCKF            tty_locks_name_path
#define    DEXE_LOAD_PATH        dexe_load_path
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

AH_TEMPLATE([USE_PTHREADS],
[ DEFINE this, if you want pthreads support ])

AH_TEMPLATE([USE_MHPDBG],
[ DEFINE this, if you want the dosdebugger (mhpdbg) ])

AH_TEMPLATE([X_SUPPORT])
AH_TEMPLATE([X2_SUPPORT])
AH_TEMPLATE([X_GRAPHICS],
[ Define these for X support ])

AH_TEMPLATE([USE_SBEMU],
[ This is DEFINED when SB emulation is required. Just changing this is not
  enough - you must reconfigure. ])

AH_TEMPLATE([USE_ULTRA],
[This is DEFINED when the Ultra libraries are available ])

AH_TEMPLATE([HAVE_MITSHM],
[ Define this if you want to use MITSHM ])

AH_TEMPLATE([ASPI_SUPPORT],
[ Define this for ASPI (generic SCSI) support ])

AH_TEMPLATE([X86_EMULATOR],
[ Define this to use the X86 CPU emulator ])

AH_BOTTOM([
#ifndef __ASM__
EXTERN char *dosemu_map_file_name INIT(config_xstr(DOSEMULIB_DEFAULT) "/dosemu-" VERSTR ".map");
#endif

#include "plugin_config.h"

#endif /* CONFIG_H */
])

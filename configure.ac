dnl Process this file with autoconf to produce a configure script.
AC_INIT([dosemu2], m4_esyscmd([./getversion -s | tr -d '\n']),
    [https://github.com/stsp/dosemu2/issues], dosemu2)
dnl AM_SILENT_RULES and AM_DEP_TRACK should not be used explicitly,
dnl AM_INIT_AUTOMAKE includes them
AM_SILENT_RULES
AM_DEP_TRACK
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR(src/emu.c)
AC_PREREQ(2.64)

AC_CONFIG_HEADER(src/include/config.hh)

dnl Checks for programs.
: ${CFLAGS=""}
AC_PROG_CC
AC_PROG_CPP
AC_GNU_SOURCE

AC_CHECK_PROGS([AS], [i386-elf-as as])
if test -z "$AS"; then
  AC_MSG_ERROR(as not found)
fi
AC_CHECK_PROGS([AS_LD], [i386-elf-ld ld])
if test -z "$AS_LD"; then
  AC_MSG_ERROR(ld not found)
fi
AC_CHECK_PROGS([OBJCOPY], [i386-elf-objcopy objcopy])
if test -z "$OBJCOPY"; then
  AC_MSG_ERROR(objcopy not found)
fi
AC_SUBST(AS)
AC_SUBST(AS_LD)
AC_SUBST(OBJCOPY)

if test "$ac_cv_prog_cc_c11" != "no"; then
  AC_DEFINE(HAVE_STD_C11)
fi

AC_ARG_WITH(target-bits-32, [  --with-target-bits-32 ],
  CPPFLAGS="$CPPFLAGS -m32"
  LDFLAGS="$LDFLAGS -m32"
  with_target_bits="32"
)
AC_PROG_INSTALL
AC_PROG_LEX
if test -z "`echo $LEX | grep flex`" ; then
  AC_CHECK_PROG(lex_ok, $LEX, "yes", "no")
fi
if test "$lex_ok" = "no"; then
  AC_MSG_WARN(Your system doesn't seem to have lex or flex available.)
  AC_MSG_ERROR(Install lex or flex and retry.)
fi
AC_PROG_LN_S
AC_CHECK_PROG([MAKE], [make], ["yes"])
if test "$MAKE" != "yes"; then
  AC_MSG_ERROR(You don't have make installed)
fi
AC_PROG_RANLIB
AC_PROG_YACC
dnl The bison problem
if test -z "`echo $YACC | grep bison`" ; then
  AC_MSG_WARN( )
  AC_MSG_WARN(Your system doesn't seem to have bison available.)
  AC_MSG_ERROR(Install bison and retry.)
fi
AC_PROG_GREP
AC_PROG_EGREP
AC_PROG_SED
AC_PROG_AWK
if test "$ac_cv_prog_AWK" != "gawk" ; then
  AC_MSG_ERROR(Install gawk and retry.)
fi

dnl Checks for libraries.

dnl Checks for header files.
dnl need largefile check here, dont remove, it defines magic macros
AC_SYS_LARGEFILE

dnl Checks for library functions.
AC_MSG_CHECKING([for static_assert support])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <assert.h>]], [[
  static_assert(sizeof(char) == 1, "sizeof doesn't work");
]])], [
  AC_MSG_RESULT([yes])
  AC_DEFINE(HAVE_STATIC_ASSERT)
], [
  AC_MSG_RESULT([no])
])

AC_CHECK_LIB(rt, shm_open)
dnl below defines HAVE_SHM_OPEN
AC_CHECK_FUNCS(shm_open)

dnl Here is where we do our stuff

AC_ARG_WITH(confdir, [AS_HELP_STRING([--with-confdir=dir],
  [directory suffix under sysconfdir. default: dosemu])],
  confdir="$withval", confdir="dosemu")
AC_ARG_WITH(plugindir, [AS_HELP_STRING([--with-plugindir=dir],
  [directory for dosemu plugins. default: ${libdir}/dosemu])],
  plugindir="$withval", plugindir="${libdir}/dosemu")
AC_ARG_WITH(x11fontdir,
  [  --with-x11fontdir=dir   give the directory used to install the VGA X11 font]
  [                        (default: ${datadir}/dosemu/Xfonts)],
  x11fontdir="$withval", x11fontdir="${datadir}/dosemu/Xfonts")
AC_ARG_WITH(syshdimagedir,
  [  --with-syshdimagedir=dir give the directory used for system wide boot directories and hdimages]
  [                        (default: /var/lib/dosemu)],
  syshdimagedir="$withval", syshdimagedir="/var/lib/dosemu")
AC_ARG_WITH(fdtarball,
  [  --with-fdtarball=file   give the name of the FreeDOS tarball],
  fdtarball="$withval")

AC_SUBST(confdir)
AC_SUBST(plugindir)
AC_SUBST(x11fontdir)
AC_SUBST(syshdimagedir)
AC_SUBST(fdtarball)

AC_DEFINE_DIR([SYSCONFDIR], [sysconfdir], [ System config dir ])
AC_DEFINE_DIR([CONFSUBDIR], [confdir], [ dosemu-specific config subdir ])
AC_DEFINE_DIR([DOSEMUPLUGINDIR], [plugindir], [ Directory for dosemu plugins ])
AC_DEFINE_DIR([DATADIR], [datadir], [Where data are placed to.])
AC_DEFINE_DIR([SYSTEM_XFONTS_PATH], [x11fontdir], [ Directory for x11 fonts ])

CONFIG_HOST='linux'
AC_SUBST(CONFIG_HOST)

machine=`uname -m`

DOSEMU_CFLAGS="-Wall -Wstrict-prototypes -Wmissing-declarations \
-Wnested-externs -fms-extensions -pthread -imacros config.hh \
-Wno-unused-result -Wcast-qual -Wwrite-strings"
DOSEMU_LDFLAGS="-pthread"

AC_CHECK_LIB(m, pow)

DOSDEBUG_LIBS=""
AC_CHECK_LIB(readline, main, [
  DOSDEBUG_LIBS="$DOSDEBUG_LIBS -lreadline"
  AC_DEFINE([HAVE_LIBREADLINE], 1, [Define to 1 if you have the readline library (-lreadline).])
], [
  AC_MSG_NOTICE(Please install libreadline-dev for readline support in dosdebug)
])
AC_SUBST(DOSDEBUG_LIBS)

AC_ARG_ENABLE(dlplugins,
              AS_HELP_STRING([--disable-dlplugins], [do NOT use dynamically loaded plugins]))
if test "$enable_dlplugins" != "no"; then
              AC_MSG_NOTICE(Using dynamically loaded plugins...)
              AC_DEFINE(USE_DL_PLUGINS)
              USE_DL_PLUGINS="USE_DL_PLUGINS=1"
              AC_SUBST(USE_DL_PLUGINS)
              DOSBIN_LDFLAGS="-rdynamic"
              AC_SUBST(DOSBIN_LDFLAGS)
              AC_CHECK_LIB(dl, dlopen)
else
              AC_MSG_NOTICE(Not using dynamically loaded plugins...)
fi

SIG_PROTO_PFX="__attribute__((no_instrument_function))"
dnl check for stack protector disabling functionaity
AC_MSG_CHECKING([for __attribute__((optimize("no-stack-protector")))])
AX_SAVE_FLAGS([attr_check])
CFLAGS="-Wall -Werror -fstack-protector-all"
AC_COMPILE_IFELSE([AC_LANG_SOURCE([
	void foo(void);
	__attribute__((optimize("no-stack-protector")))
	void foo(void)
	{
	}
    ])],
    [
	AC_MSG_RESULT([supported])
	SIG_PROTO_PFX="$SIG_PROTO_PFX __attribute__((optimize(\"no-stack-protector\")))"
    ],
    [
	AC_MSG_RESULT([unsupported])
	DOSEMU_CFLAGS="$DOSEMU_CFLAGS -fno-stack-protector"
    ])
AX_RESTORE_FLAGS([attr_check])
AC_DEFINE_UNQUOTED(SIG_PROTO_PFX, $SIG_PROTO_PFX)

dnl Check whether we have pthreads and whether to use it
AC_CHECK_LIB(pthread, pthread_create,,[
    AC_MSG_ERROR([No libpthread found, please install glibc-devel package])
  ]
)
AC_CHECK_FUNCS([pthread_getname_np pthread_setname_np])

dnl Check for the XKB extension to get reliable keyboard handling
AC_CHECK_HEADER(X11/XKBlib.h, AC_DEFINE(HAVE_XKB,1))
AC_CHECK_HEADERS([netipx/ipx.h linux/ipx.h])
CONFIG_TIME=`date +"%F %T %z"`
AC_DEFINE_UNQUOTED(CONFIG_HOST, "$CONFIG_HOST")
AC_DEFINE_UNQUOTED(CONFIG_TIME, "$CONFIG_TIME")

dnl enable EXPERIMENTAL stuff
AC_ARG_ENABLE(experimental,
              AS_HELP_STRING([--enable-experimental], [enable configuration of EXPERIMENTAL stuff]))
if test "$enable_experimental" = "yes"; then
  AC_MSG_NOTICE(Allowing EXPERIMENTAL stuff to be configured...);
  AC_DEFINE([EXPERIMENTAL], 1, [ Define this to enable experimental stuff ])
else
  AC_MSG_NOTICE(EXPERIMENTAL stuff disabled...);
fi

dnl try to hook in available plug-ins
AC_ARG_ENABLE(plugins,
              AS_HELP_STRING([--enable-plugins=list],
              [comma-separated list of plugins, use + to append]))
def_plugins=`cat $srcdir/plugin_list | sed 's/$/,/g' | tr -d '\n' | sed 's/,$//'`
if test -z "$enable_plugins"; then
    enable_plugins="$def_plugins"
fi
if test "${enable_plugins#\+}" != "$enable_plugins"; then
    enable_plugins="$def_plugins,${enable_plugins#\+}"
fi
if test "$enable_plugins" = "no"; then
    enable_plugins=""
fi
AC_MSG_NOTICE([Creating plug-in hooks...])
PLUGINSUBDIRS=
ST_PLUGINSUBDIRS=
$srcdir/scripts/mkpluginhooks clean
for i in $(echo $enable_plugins | tr ',' ' '); do
    if test -z "$i"; then continue; fi
    if $srcdir/scripts/plugctl.sh $i yes ; then
        PLUGINSUBDIRS="$PLUGINSUBDIRS plugin/$i"
        if ! $EGREP 'USE_DL_PLUGINS|NO_LIB' $srcdir/src/plugin/$i/Makefile >/dev/null ; then
            ST_PLUGINSUBDIRS="$ST_PLUGINSUBDIRS plugin/$i"
        fi
    fi
done

AC_ARG_ENABLE(system_wa,
  AS_HELP_STRING([--disable-system-wa], [compile without system-specific
  work-arounds (mostly for linux kernel) that are not needed on this system]))
AS_IF([test "x$enable_system_wa" = "xno"], [
  AC_MSG_NOTICE(Disasbling system-specific work-arounds)
  AC_DEFINE(DISABLE_SYSTEM_WA)
  AS_IF([test -z "$WARN_OUTDATED_WA" -a -z "$NOWARN_UNDISABLED_WA"], [
    AC_DEFINE(WARN_UNDISABLED_WA)])
])
AS_IF([test -n "$WARN_OUTDATED_WA"], [
  AC_MSG_NOTICE(Enabling compile-time warnings for outdated work-arounds)
  AC_DEFINE(WARN_OUTDATED_WA)
])

dnl Do compilation for GDB
HAVE_LIBBFD=0
AC_ARG_ENABLE(debug,
               AS_HELP_STRING([--enable-debug], [compile with debug info]))
if test "$enable_debug" = "yes"; then
  AC_MSG_NOTICE(Compiling with debug info...)
  AC_ARG_ENABLE(xbacktrace, AS_HELP_STRING([--enable-xbacktrace],
    [enable extended backtrace functionality]))
  if test "$enable_xbacktrace" != "no"; then
    AC_CHECK_HEADER(bfd.h, [
      AC_CHECK_LIB(z, inflate)
      dnl check iberty before bfd - order matters
      AC_CHECK_LIB(iberty, lrealpath)
      AC_CHECK_LIB(bfd, bfd_init)
      if test "$ac_cv_lib_bfd_bfd_init" = "yes"; then
        HAVE_LIBBFD=1
        AC_DEFINE(HAVE_LIBBFD)
        AC_DEFINE(HAVE_BACKTRACE)
      fi
    ])
    if test "$HAVE_LIBBFD" = "1"; then
      AC_MSG_NOTICE(Enabling extended backtrace functionality)
    else
      AC_MSG_WARN(libbfd not available, no extended backtrace functionality)
    fi
  fi
  AC_CHECK_HEADER([execinfo.h], [
    AC_DEFINE(HAVE_EXECINFO)
    if test "$HAVE_LIBBFD" != "1"; then
      DOSEMU_LDFLAGS="$DOSEMU_LDFLAGS -rdynamic"
    fi
    AC_DEFINE(HAVE_BACKTRACE)
  ])
else
  AC_MSG_NOTICE(Compiling without debug info...)
fi

AC_ARG_ENABLE(profile,
               AS_HELP_STRING([--enable-profile], [compile with profile info]))

AC_ARG_ENABLE(asan, [AS_HELP_STRING(--enable-asan, [enable address sanitizer])])
AC_ARG_ENABLE(ubsan, [AS_HELP_STRING(--enable-ubsan, [enable UB sanitizer])])

AC_SUBST(OPTIONALSUBDIRS)
AC_SUBST(REQUIRED)
AC_SUBST(PLUGINSUBDIRS)
AC_SUBST(ST_PLUGINSUBDIRS)
RELEASE_DATE=`cd $srcdir && ./getversion -d`
AC_SUBST(RELEASE_DATE)

dnl CPU emulator
AC_ARG_ENABLE(cpuemu,
               [  --disable-cpuemu        do NOT compile with optional x86 emulation code])
if test "$enable_cpuemu" != "no"; then
  AC_MSG_CHECKING([for __attribute__((force_align_arg_pointer))])
  AX_SAVE_FLAGS([attr_check2])
  CFLAGS="-Wall -Werror"
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([
	void foo(void);
	__attribute__((force_align_arg_pointer))
	void foo(void)
	{
	}
    ])],
    [
	AC_MSG_RESULT([supported])
	AC_MSG_NOTICE(Compiling with x86 emulator)
	AC_DEFINE_UNQUOTED(X86_EMULATOR)
	X86_EMULATOR="X86_EMULATOR=1"
	AC_SUBST(X86_EMULATOR)
    ],
    [
	AC_MSG_RESULT([unsupported])
	AC_MSG_WARN(Compiling without x86 emulator)
    ]
  )
  AX_RESTORE_FLAGS([attr_check2])
fi

AC_ARG_WITH(target_cpu, [  --with-target-cpu=CPU	  use the specified target CPU (default=auto)])
if test "$with_target_cpu" = ""; then
  AC_MSG_NOTICE(Compiling with default target CPU...)
  target_cpu=""
else
  AC_MSG_NOTICE(Compiling with specified target CPU...)
  target_cpu=$with_target_cpu
fi

AC_MSG_CHECKING([for $CC actually being clang])
if "$CC" -v 2>&1|grep 'clang' >/dev/null; then
  AC_MSG_RESULT([yes])
  AC_MSG_CHECKING([for clang >= 3.7])
  clang_ver=`$CC -v 2>&1 | grep version | sed 's/.*version //' | cut -d " " -f 1`
  clang_maj=`echo $clang_ver | cut -d "." -f 1`
  clang_mid=`echo $clang_ver | cut -d "." -f 2`
  if test $clang_maj -ge 3 -a $clang_mid -ge 7 -o $clang_maj -ge 4 ; then
    AC_MSG_RESULT([yes, $clang_ver])
  else
    AC_MSG_RESULT([no, $clang_ver])
    AC_MSG_ERROR([Need clang-3.7 or newer, or use gcc instead])
  fi
  DOSEMU_CFLAGS="$DOSEMU_CFLAGS -Wno-microsoft -Wno-incompatible-pointer-types \
    -Wno-address-of-packed-member"
  LDFLAGS="$LDFLAGS -Wno-unused-command-line-argument"
  use_clang="yes"
else
  CCAS="$CC"
  AC_MSG_RESULT([no])
  use_clang="no"

  AC_MSG_CHECKING([for $CC actually being c++])
  if "$CC" -v 2>&1 | grep 'COLLECT_GCC=g++' >/dev/null; then
    AC_MSG_RESULT([yes, using -fpermissive])
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -fpermissive"
  else
    AC_MSG_RESULT([no])
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -fplan9-extensions -Wno-maybe-uninitialized"
  fi
fi

if test "$machine" = "x86_64" -a "$with_target_bits" != "32" -a \
	"$enable_profile" != "yes"; then
    AC_MSG_NOTICE(Compiling as PIE for $machine...)
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -fpie"
    DOSEMU_LDFLAGS="$DOSEMU_LDFLAGS -pie"
else
    AC_MSG_NOTICE(Compiling as non-PIE...)
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -fno-pie"
    DOSEMU_LDFLAGS="$DOSEMU_LDFLAGS -no-pie"
fi

if test "$enable_asan" = "yes"; then
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -fsanitize=address"
    LDFLAGS="$LDFLAGS -fsanitize=address"
    AC_MSG_NOTICE(enabling address sanitizer)
fi
if test "$enable_ubsan" = "yes"; then
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -fsanitize=undefined -fno-sanitize=alignment"
    LDFLAGS="$LDFLAGS -fsanitize=undefined"
    AC_MSG_NOTICE(enabling UB sanitizer)
fi
if test "$enable_profile" = "yes"; then
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -pg -no-pie"
    LDFLAGS="$LDFLAGS -pg -no-pie"
    AC_MSG_NOTICE(enabling profiling)
fi

if test "$enable_debug" = "yes" ; then
  DOSEMU_CFLAGS="$DOSEMU_CFLAGS -ggdb3"
  if test -z "$CFLAGS" ; then
    # simx86 is too slow on gcc without -O
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -O"
  fi
elif test -z "$CFLAGS" ; then
  DOSEMU_CFLAGS="$DOSEMU_CFLAGS -O2"
fi

DOSEMU_CFLAGS="${DOSEMU_CFLAGS} ${OPT} ${PIPE}"
DOSEMU_CPPFLAGS="${DOSEMU_CPPFLAGS} -MMD -DCFLAGS_STR=\"$DOSEMU_CFLAGS $CFLAGS\""
DOSEMU_VERSION=`cd $srcdir && ./getversion -b`
ASFLAGS="--32"
AS_LDFLAGS="-melf_i386"

AC_SUBST(ASFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(AS_LDFLAGS)
AC_SUBST(DOSEMU_CFLAGS)
AC_SUBST(DOSEMU_CPPFLAGS)
AC_SUBST(DOSEMU_LDFLAGS)
AC_SUBST(LIBS)
AC_SUBST(HAVE_LIBBFD)
AC_SUBST(DOSEMU_VERSION)

cmds_ver=0.2
cmdsuff=$PACKAGE_TARNAME-cmds-$cmds_ver
AC_DEFINE_UNQUOTED(CMDS_SUFF, "$cmdsuff")
AC_SUBST(cmdsuff)

dnl Create output files. If you add new ones, please do it in order.
man_files=`cd $srcdir && find man -name '*.in' | sed 's/\.in$//'`
AC_DEFUN([AC_DATAROOTDIR_CHECKED])
AC_CONFIG_FILES([Makefile.conf $PACKAGE_NAME.spec $man_files])

AC_CONFIG_COMMANDS([Makefile],
[ AC_PROG_LN_S
  if ! test -f Makefile; then
    wd=`pwd`
    abssrcdir=`cd $srcdir && pwd`
    if test "${wd#$abssrcdir}" != "$wd" ; then
      builddir=".${wd#$abssrcdir}"
    else
      builddir="$wd"
    fi
    mkdir -p `(cd $abssrcdir; find doc man etc src -path src/lib -prune -o -type d -print)`
    for i in `(cd $abssrcdir; find . -path $builddir -prune -o -name Makefile -print)`; do
      echo  $LN_S -f $abssrcdir/${i#./} $i
      $LN_S -f $abssrcdir/${i#./} $i
    done
    for i in README.bindist NEWS THANKS BUGS; do
	cp $abssrcdir/$i $i
    done
    for i in `(cd $abssrcdir; find man -name '*.1')`; do
	cp $abssrcdir/$i $i
    done
  fi
])

AH_TEMPLATE([CONFIG_HOST],
[ Define the host for which Dosemu is configured ])

AH_TEMPLATE([CONFIG_TIME],
[ Define the configure time ])

AH_TEMPLATE([SIG_PROTO_PFX],
[ Define the signal handling function prototype prefix ])

AH_TEMPLATE([HAVE_STD_C11],
[Define this if your compiler supports c11])

AH_TEMPLATE([HAVE_STATIC_ASSERT],
[Define this if your glibc defines static_assert])

AH_TEMPLATE(HAVE_XKB,
[Define this if you have the XKB extension])

AH_TEMPLATE(HAVE_LIBBFD,
[Define this if you have binutils-devel installed])

AH_TEMPLATE(HAVE_EXECINFO,
[Define this if you have execinfo.h in libc])

AH_TEMPLATE([USE_DL_PLUGINS],
[ DEFINE this, if you want dynamically loaded plugins ])

AH_TEMPLATE([HAVE_BACKTRACE],
[ Define this for backtrace() functionality ])

AH_TEMPLATE([HAVE_SIGCONTEXT_SS],
[ Define this if struct sigcontext has .ss field ])

AH_TEMPLATE([X86_EMULATOR],
[ Define this to use the X86 CPU emulator ])

AH_TEMPLATE([DISABLE_SYSTEM_WA],
[ Define this to disable system-specific work-arounds that are
unneeded on your system ])

AH_TEMPLATE([WARN_UNDISABLED_WA],
[ Define this to enable compile-time warnings for system-specific
work-arounds that were failed to disable ])

AH_TEMPLATE([WARN_OUTDATED_WA],
[ Define this to enable compile-time warnings for outdated
system-specific work-arounds ])

AH_TEMPLATE([CMDS_SUFF],
[ Commands directory suffix ])

AC_OUTPUT

dnl Print some warnings (if neccessary)

PLUGINS=`echo $PLUGINSUBDIRS | $SED 's/plugin\//\n\t/g'`
AC_MSG_RESULT()
AC_MSG_RESULT(Enabling plugins: $PLUGINS)
VID_PLU=`echo $PLUGINS | $EGREP 'sdl|X|term|sdl1'`
if test -z "$VID_PLU"; then
  AC_MSG_RESULT()
  AC_MSG_WARN([No video plugins available!])
fi
AUD_PLU=`echo $PLUGINS | $EGREP 'sdl|ao|sdl1'`
if test -z "$AUD_PLU"; then
  AC_MSG_RESULT()
  AC_MSG_WARN([No audio plugins available!])
fi
FDPP_PLU=`echo $PLUGINS | $EGREP 'fdpp'`
if test -z "$FDPP_PLU"; then
  AC_MSG_RESULT()
  AC_MSG_WARN([fdpp not available!])
fi

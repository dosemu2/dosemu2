dnl Process this file with autoconf to produce a configure script.
AC_INIT([DOSEMU2], m4_esyscmd([./getversion -s | tr -d '\n']),
    [https://github.com/stsp/dosemu2/issues], dosemu2)
dnl AM_SILENT_RULES and AM_DEP_TRACK should not be used explicitly,
dnl AM_INIT_AUTOMAKE includes them
AM_SILENT_RULES
AM_DEP_TRACK
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR(src/emu.c)
AC_PREREQ(2.59)
AC_GNU_SOURCE

AC_ARG_ENABLE(plugin, [AS_HELP_STRING(
  [--enable-plugin=NAME], [enable plugin NAME])])

# first check if we are called from default-configure and if not,
# execute that to parse compiletime settings
if test -z "$DOSEMU_DEFAULT_CONFIGURE" ; then
  exec $srcdir/default-configure "$ac_configure_args"
fi

AC_CONFIG_HEADER(src/include/config.h)

dnl Checks for programs.
: ${CFLAGS=""}
AC_PROG_CC
AC_PROG_CPP

if test "$ac_cv_prog_cc_c11" != "no"; then
  AC_DEFINE(HAVE_STD_C11)
fi

AC_ARG_WITH(target-bits, [  --with-target-bits=bits compile for 32 or 64 bits (default=auto)],
  CPPFLAGS="$CPPFLAGS -m$with_target_bits"
  export CPPFLAGS
  LDFLAGS="$LDFLAGS -m$with_target_bits"
  export LDFLAGS
)
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
AC_PROG_GREP
AC_PROG_EGREP
AC_PROG_SED
AC_PROG_PERL

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

CONFIG_HOST='linux'
AC_SUBST(CONFIG_HOST)

DOSEMU_CFLAGS="-Wall -Wstrict-prototypes -Wmissing-declarations \
-Wnested-externs -fms-extensions -pthread -imacros config.h \
-Wno-unused-result"
DOSEMU_LDFLAGS="-Wl,-warn-common -pthread"
AC_CHECK_LIB(m, pow)

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
])

dnl Check for the XKB extension to get reliable keyboard handling
AC_CHECK_HEADER(X11/XKBlib.h, AC_DEFINE(HAVE_XKB,1))

CONFIG_TIME=`date +"%F %T %z"`
AC_DEFINE_UNQUOTED(CONFIG_HOST, "$CONFIG_HOST")
AC_DEFINE_UNQUOTED(CONFIG_TIME, "$CONFIG_TIME")

REQUIRED="commands tools/periph"

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
$srcdir/mkpluginhooks
if test -f plugin_libdirs; then
    PLUGINSUBDIRS=`cat plugin_libdirs`
    cd src
    for i in $PLUGINSUBDIRS; do
      if ! grep USE_DL_PLUGINS ../$srcdir/src/$i/Makefile >/dev/null ; then
        ST_PLUGINSUBDIRS="$ST_PLUGINSUBDIRS $i"
      fi
    done
    cd ..
    if test -f plugin_incdirs; then
      cd src
      for i in `cat ../plugin_incdirs`; do
        if test -f ../configure.ac; then
          for j in $i/*.h; do
            $LN_S -f ../../$j plugin/include/`basename $j`
          done
        else
	  abssrcdir=`cd .. && cd $srcdir && pwd`
          for j in $abssrcdir/src/$i/*.h; do
            $LN_S -f $j plugin/include/`basename $j`
          done
        fi
      done
      cd ..
    fi
else
    AC_MSG_NOTICE(Compiling without plug-ins...)
fi

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
      AC_CHECK_LIB(bfd, bfd_init)
      if test "$ac_cv_lib_bfd_bfd_init" = "yes"; then
        HAVE_LIBBFD=1
      fi
    ])
    if test "$HAVE_LIBBFD" = "1"; then
      AC_MSG_NOTICE(Enabling extended backtrace functionality)
    else
      AC_MSG_WARN(libbfd not available, no extended backtrace functionality)
    fi
  fi
else
  AC_MSG_NOTICE(Compiling without debug info...)
fi

AC_ARG_ENABLE(profile,
               AS_HELP_STRING([--enable-profile], [compile with profile info]))

AC_ARG_ENABLE(asan, [AS_HELP_STRING(--enable-asan, [enable address sanitizer])])

AC_SUBST(OPTIONALSUBDIRS)
AC_SUBST(REQUIRED)
AC_SUBST(PLUGINSUBDIRS)
AC_SUBST(ST_PLUGINSUBDIRS)

dnl Now CFLAGS and INCLUDES

INCDIR="-I\${top_builddir}/src/include -I\${top_builddir}/src/plugin/include"
AC_SUBST(INCDIR)

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
  if "$CC" -no-integrated-as -E - < /dev/null 2>&1|grep 'clang' >/dev/null; then
    AC_MSG_RESULT([yes])
  else
    ASFLAGS="$ASFLAGS -no-integrated-as"
    AC_MSG_RESULT([yes, using -no-integrated-as])
  fi
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
  DOSEMU_CFLAGS="$DOSEMU_CFLAGS -Wno-microsoft -Wno-incompatible-pointer-types"
  use_clang="yes"
else
  CCAS="$CC"
  DOSEMU_CFLAGS="$DOSEMU_CFLAGS -fplan9-extensions -Wno-maybe-uninitialized"
  AC_MSG_RESULT([no])
  use_clang="no"
fi

if test "$enable_debug" = "yes" ; then
  DOSEMU_CFLAGS="$DOSEMU_CFLAGS -ggdb3"
  if test -z "$CFLAGS" ; then
    # simx86 is too slow on gcc without -O
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -O"
  fi
  if test "$enable_asan" = "yes"; then
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -fsanitize=address"
    LDFLAGS="$LDFLAGS -fsanitize=address"
    AC_MSG_NOTICE(enabling address sanitizer)
  fi
  if test "$enable_profile" = "yes"; then
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -pg"
    LDFLAGS="$LDFLAGS -pg"
    AC_MSG_NOTICE(enabling profiling)
  fi
  AC_DEFINE(_DEBUG)
elif test -z "$CFLAGS" ; then
  DOSEMU_CFLAGS="$DOSEMU_CFLAGS -O2"
fi

DOSEMU_CFLAGS="${DOSEMU_CFLAGS} ${OPT} ${PIPE}"
DOSEMU_CPPFLAGS="${DOSEMU_CPPFLAGS} -MMD -DCFLAGS_STR=\"$DOSEMU_CFLAGS $CFLAGS\""
AC_SUBST(ASFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(DOSEMU_CFLAGS)
AC_SUBST(DOSEMU_CPPFLAGS)
AC_SUBST(DOSEMU_LDFLAGS)
AC_SUBST(LIBS)
AC_SUBST(HAVE_LIBBFD)

cmds_ver=0.1
cmdsuff=$PACKAGE_TARNAME-cmds-$cmds_ver
AC_DEFINE_UNQUOTED(CMDS_SUFF, "$cmdsuff")
AC_SUBST(cmdsuff)

dnl Create output files. If you add new ones, please do it in order.

AC_CONFIG_FILES([Makefile.conf])

AC_CONFIG_COMMANDS([Makefile],
[ if ! test -f configure.ac; then
    abssrcdir=`cd $srcdir && pwd`
    mkdir -p `(cd $abssrcdir; find . -name .git -prune -o -name .svn -prune -o -name CVS -prune -o -type d -print)`
    for i in `(cd $abssrcdir; find . -name Makefile)`; do
      echo  $LN_S -f $abssrcdir/$i $i
      $LN_S -f $abssrcdir/$i $i
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

AH_TEMPLATE([USE_DL_PLUGINS],
[ DEFINE this, if you want dynamically loaded plugins ])

AH_TEMPLATE([_DEBUG],
[ Define this for DEBUG info ])

AH_TEMPLATE([HAVE_SIGCONTEXT_SS],
[ Define this if struct sigcontext has .ss field ])

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
VID_PLU=`echo $PLUGINS | $EGREP 'sdl|X|term'`
if test -z "$VID_PLU"; then
  AC_MSG_RESULT()
  AC_MSG_WARN([No video plugins available!])
fi
AUD_PLU=`echo $PLUGINS | $EGREP 'sdl|ao'`
if test -z "$AUD_PLU"; then
  AC_MSG_RESULT()
  AC_MSG_WARN([No audio plugins available!])
fi

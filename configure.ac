dnl Process this file with autoconf to produce a configure script.
AC_INIT([dosemu2],m4_esyscmd(./getversion -s | tr -d '\n'),[https://github.com/dosemu2/dosemu2/issues],[dosemu2])
dnl AM_SILENT_RULES and AM_DEP_TRACK should not be used explicitly,
dnl AM_INIT_AUTOMAKE includes them
AM_SILENT_RULES
AM_DEP_TRACK
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([src/include/emu.h])
AC_PREREQ([2.64])

AC_CONFIG_HEADERS([src/include/config.hh])

dnl Checks for programs.
: ${CFLAGS=""}
AC_PROG_CC
#AC_PROG_CC_C11([AC_DEFINE(HAVE_STD_C11)])
AC_DEFINE(HAVE_STD_C11)
AC_PROG_CPP
AX_PROG_CC_FOR_BUILD
AC_USE_SYSTEM_EXTENSIONS

AC_MSG_CHECKING([for $CC actually being clang])
if $CC -v 2>&1|grep 'clang' >/dev/null; then
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

  AS="$CC"
  clang_as_as=1
  ASFLAGS="-xassembler -c -"
else
  AC_MSG_RESULT([no])
  use_clang="no"

  AC_MSG_CHECKING([for $CC actually being c++])
  if $CC -v 2>&1 | grep "g++" | grep "COLLECT_GCC" >/dev/null; then
    AC_MSG_RESULT([yes, using -fpermissive])
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -fpermissive -Wno-narrowing"
  else
    AC_MSG_RESULT([no])
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -fplan9-extensions"
  fi
fi

CONFIG_HOST=`uname -s`
AC_SUBST(CONFIG_HOST)
machine=`$CC -dumpmachine | cut -d- -f1 | sed 's/i.86/i386/'`

if test -z "$AS"; then
  AC_PATH_PROG([AS], [as])
  if test -z "$AS"; then
    AC_PATH_PROG([AS], [clang])
    if test -z "$AS"; then
      AC_MSG_ERROR(as not found)
    fi
    AC_MSG_NOTICE(Using clang as as...)
    clang_as_as=1
    ASFLAGS="-xassembler -c -"
  fi
fi
AC_PATH_PROGS([XAS], [i686-linux-gnu-as i386-elf-as])
if test -z "$XAS"; then
  if test "$CONFIG_HOST" = "Darwin"; then
    AC_MSG_ERROR([Please install i386-elf-binutils from https://github.com/nativeos/homebrew-i386-elf-toolchain])
  fi
  AC_PATH_PROG([XAS], [x86_64-linux-gnu-as])
  if test -z "$XAS"; then
    if test "$machine" = "i386" -o "$machine" = "x86_64"; then
      XAS="$AS"
      if test "$clang_as_as" = "1"; then
        XASFLAGS="$ASFLAGS -m32"
      else
        XASFLAGS="$ASFLAGS --32"
      fi
    else
      AC_MSG_ERROR([cross-assembler not found, please install binutils-i686-linux-gnu])
    fi
  else
    XASFLAGS="$ASFLAGS --32"
  fi
fi

AC_PATH_PROGS([LD], [ld ld.lld])
if test -z "$LD"; then
  AC_MSG_ERROR(ld not found)
fi
AC_PATH_PROGS([AS_LD], [i686-linux-gnu-ld i386-elf-ld x86_64-linux-gnu-ld ld.lld ld])
if test -z "$AS_LD"; then
  AS_LD="$LD"
fi

AC_PATH_PROG([OBJCOPY], [objcopy])
O_VER=`$OBJCOPY -V | head -n 1 | grep "GNU objcopy"`
if test -z "$O_VER"; then
  AC_MSG_WARN([rejecting BSD objcopy])
  OBJCOPY=""
fi
if test -z "$OBJCOPY"; then
  AC_PATH_PROG([LOBJCOPY], [llvm-objcopy], [], [$PATH${PATH_SEPARATOR}/usr/local/opt/llvm/bin])
  if test -z "$LOBJCOPY"; then
    AC_MSG_ERROR(objcopy not found)
  fi
  OBJCOPY="$LOBJCOPY"
fi

AC_PATH_PROGS([XOBJCOPY], [i686-linux-gnu-objcopy x86_64-linux-gnu-objcopy objcopy])
if test -z "$XOBJCOPY"; then
  XOBJCOPY="$OBJCOPY"
fi
AC_SUBST(AS)
AC_SUBST(XAS)
AC_SUBST(AS_LD)
AC_SUBST(XOBJCOPY)

AC_ARG_WITH(target-bits-32, [AS_HELP_STRING([--with-target-bits-32],
    [build for 32bit target (multilib in case of x86_64 host)])],
  CPPFLAGS="$CPPFLAGS -m32"
  LDFLAGS="$LDFLAGS -m32"
  ASFLAGS="--32"
  with_target_bits="32"
)
AC_PROG_INSTALL
AC_PROG_LEX([noyywrap])
if test -z "$LEX"; then
  AC_MSG_WARN(Your system doesn't seem to have lex or flex available.)
  AC_MSG_ERROR(Install lex or flex and retry.)
fi

AC_PROG_LN_S
AC_MSG_CHECKING([if $LN_S is a GNU ln])
if $LN_S --help >/dev/null 2>&1; then
  AC_MSG_RESULT([yes, using -T])
  LN_SFT="$LN_S -f -T"
else
  AC_MSG_RESULT([no, using -F])
  LN_SFT="$LN_S -f -F"
fi
AC_SUBST(LN_SFT)

AC_CHECK_PROGS([MAKE], [gmake make])
if test -z "$MAKE"; then
  AC_MSG_ERROR(You don't have make installed)
fi
# export MAKE to run from scripts
export MAKE="$MAKE"
AC_CHECK_PROGS([REALPATH], [realpath])
if test -z "$REALPATH"; then
  AC_MSG_ERROR(You don't have realpath installed)
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
AC_CHECK_PROGS([PKG_CONFIG], [pkg-config])
if test -z "$PKG_CONFIG"; then
  AC_MSG_ERROR(Install pkg-config and retry.)
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

AC_MSG_CHECKING([for assignable stderr])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([
	#include <stdio.h>
	int main(void)
	{
	    stderr = NULL;
	    return 0;
	}
    ])],
    [
	AC_MSG_RESULT([yes])
	AC_DEFINE(HAVE_ASSIGNABLE_STDERR)
    ],
    [
	AC_MSG_RESULT([no])
    ]
)

AC_MSG_CHECKING([for optreset])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([
	#include <unistd.h>
	int main(void)
	{
	    optreset = 1;
	    return 0;
	}
    ])],
    [
	AC_MSG_RESULT([yes])
	AC_DEFINE(HAVE_OPTRESET)
    ],
    [
	AC_MSG_RESULT([no])
    ]
)

AC_CHECK_LIB(rt, shm_open)
dnl below defines HAVE_xxx so needed even if AC_CHECK_LIB() succeeded
AC_CHECK_FUNCS([shm_open memfd_create fopencookie sigtimedwait closefrom])
AC_CHECK_FUNCS([setxattr], [
    USE_XATTRS=1
    AC_SUBST(USE_XATTRS)
])
AC_CHECK_FUNCS([timerfd_create], [
    USE_EVTIMER_FD=1
    AC_SUBST(USE_EVTIMER_FD)
], [AC_CHECK_FUNCS([kqueue], [
    USE_EVTIMER_FD=1
    AC_SUBST(USE_EVTIMER_FD)
])])
AC_CHECK_FUNCS([mremap])
AC_CHECK_FUNCS([fls])

AC_CHECK_DECLS([F_OFD_SETLK], [
    USE_OFD_LOCKS=1
    AC_SUBST(USE_OFD_LOCKS)
  ],, [[#include <fcntl.h>]]
)
AC_CHECK_DECLS([MREMAP_MAYMOVE],,, [[#include <sys/mman.h>]])
AC_CHECK_DECLS([MADV_POPULATE_WRITE, MADV_COLD],,,
  [[#include <sys/mman.h>]])

AC_CHECK_TYPES([__float80],, [
    if test "$machine" != "i386" -a "$machine" != "x86_64"; then
      USE_SOFTFLOAT=1
      AC_SUBST(USE_SOFTFLOAT)
    fi
  ]
)

AC_CHECK_FUNC(strlcpy,, [
  PKG_CHECK_MODULES([LIBBSD], [libbsd], [
    dnl re-check for proper arch (multilib)
    AC_CHECK_LIB(bsd, strlcpy,, [AC_MSG_ERROR(libbsd not found)])
  ]
)])

dnl Here is where we do our stuff

AC_ARG_WITH(confdir, [AS_HELP_STRING([--with-confdir=dir],
  [directory suffix under sysconfdir. default: dosemu])],
  confdir="$withval", confdir="dosemu")
AC_ARG_WITH(x11fontdir, [AS_HELP_STRING([--with-x11fontdir=dir],
  [directory to install the VGA X11 font. default: ${datadir}/dosemu/Xfonts])],
  x11fontdir="$withval", x11fontdir="${datadir}/dosemu/Xfonts")
AC_ARG_WITH(ttffontdir, [AS_HELP_STRING([--with-ttffontdir=dir],
  [directory to install the ttf font. default: ${datadir}/fonts/oldschool])],
  ttffontdir="$withval", ttffontdir="${datadir}/fonts/oldschool")
AC_ARG_WITH(fdtarball, [AS_HELP_STRING([--with-fdtarball=file],
  [path to FreeDOS tarball])],
  fdtarball="$withval")

# Hard-code this. It is evaluated at run-time as <binary_dir>../lib/dosemu,
# so we do not use libdir here (which can be lib64 or something unrelated)
plugindir="\${prefix}/lib/dosemu"

AC_SUBST(confdir)
AC_SUBST(plugindir)
AC_SUBST(x11fontdir)
AC_SUBST(ttffontdir)
AC_SUBST(syshdimagedir)
AC_SUBST(fdtarball)

AC_DEFINE_DIR([SYSCONFDIR], [sysconfdir], [ System config dir ])
AC_DEFINE_DIR([CONFSUBDIR], [confdir], [ dosemu-specific config subdir ])
AC_DEFINE_DIR([PREFIX], [prefix], [ prefix ])
AC_DEFINE_DIR([LIBEXECDIR], [libexecdir], [ Where extra executables are placed to ])
AC_DEFINE_DIR([SYSTEM_XFONTS_PATH], [x11fontdir], [ Directory for x11 fonts ])

DOSEMU_CPPFLAGS="$DOSEMU_CPPFLAGS -imacros config.hh"
DOSEMU_CFLAGS="$DOSEMU_CFLAGS -Wall -Wstrict-prototypes -Wmissing-declarations \
-Wnested-externs -fms-extensions -pthread \
-Wno-unused-result -Wcast-qual -Wwrite-strings -Wstrict-aliasing=2 -Wundef"
AX_CHECK_COMPILE_FLAG([-Waddress-of-packed-member],
  [DOSEMU_CFLAGS="$DOSEMU_CFLAGS -Wno-address-of-packed-member"],, [-Werror])
AX_CHECK_COMPILE_FLAG([-Wstring-plus-int],
  [DOSEMU_CFLAGS="$DOSEMU_CFLAGS -Wno-string-plus-int"],, [-Werror])
DOSEMU_LDFLAGS="-pthread"

AC_CHECK_LIB(m, pow)

AC_ARG_ENABLE(dlplugins,
              AS_HELP_STRING([--disable-dlplugins], [do NOT use dynamically loaded plugins]))
if test "$enable_dlplugins" != "no"; then
              AC_MSG_NOTICE(Using dynamically loaded plugins...)
              AC_DEFINE(USE_DL_PLUGINS)
              USE_DL_PLUGINS=1
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
AC_CHECK_LIB(pthread, pthread_create,, [
  dnl see if the func is provided nevertheless (not needing -lpthread)
  AC_CHECK_FUNC(pthread_create,, [
    AC_MSG_ERROR([No libpthread found, please install glibc-devel package])
  ]
)])
AC_CHECK_FUNC(pthread_cancel,, [
  AC_CHECK_HEADERS([bthread.h],, [
    AC_MSG_ERROR([pthread_cancel() missing, install libbthread])
  ])
])
AC_CHECK_LIB(bthread, pthread_cancel)

AC_CHECK_FUNCS([pthread_getname_np pthread_setname_np])
AC_CHECK_FUNCS([pthread_attr_setsigmask_np pthread_setattr_default_np])

AC_CHECK_HEADER([wordexp.h],, [
  AC_MSG_ERROR([wordexp.h missing, install libwordexp])
])
AC_CHECK_LIB(wordexp, wordexp)

AC_CHECK_HEADERS([scsi/sg.h linux/cdrom.h sys/io.h])
AC_CHECK_HEADERS([netipx/ipx.h linux/ipx.h netpacket/packet.h])
AC_CHECK_HEADERS([asm/ucontext.h],,, [
  #include <ucontext.h>
  #include <signal.h>
])
AC_CHECK_HEADERS([ucontext.h],, [
  AS_UNSET([ac_cv_header_ucontext_h])
  AC_CHECK_HEADER([ucontext.h],
    AC_DEFINE([_XOPEN_SOURCE], [500], [Use X/Open 5 with POSIX 1995]),
    AC_MSG_ERROR([Can't use ucontext.h]), [
  #define _XOPEN_SOURCE 500
])])
AC_CHECK_HEADERS([linux/signal.h],,, [#include <signal.h>])
AC_CHECK_HEADERS([sys/soundcard.h], [
  USE_OSS=1
  AC_SUBST(USE_OSS)
])
DATE_FMT="%F %T %z"
test -n "$SOURCE_DATE_EPOCH" || SOURCE_DATE_EPOCH=`date +%s`
CONFIG_TIME=`date -u -d "@$SOURCE_DATE_EPOCH" "+$DATE_FMT" 2>/dev/null || date -u -r "$SOURCE_DATE_EPOCH" "+$DATE_FMT" 2>/dev/null || date -u "+$DATE_FMT"`
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
AC_ARG_WITH(plugin-options,
    [AS_HELP_STRING([--with-plugin-options=PLUGIN,OPTS],
        [Pass options OPTS to plugin PLUGIN])],
    [
        plu=`echo $withval |cut -d "," -f 1`
        val=`echo $withval |cut -d "," -f 2`
        export ${plu}_OPTS="$val"
    ]
)

if ! test -f Makefile.conf.in; then
  AC_MSG_NOTICE([Populating makefiles...])
  wd=`pwd`
  abssrcdir=`cd $srcdir && pwd`
  if test "${wd#$abssrcdir}" != "$wd" ; then
    builddir=".${wd#$abssrcdir}"
  else
    builddir="$wd"
  fi
  mkdir -p `(cd $abssrcdir; find doc man etc src test -type d -print)`
  for i in `(cd $abssrcdir; find . -path $builddir -prune -o -name Makefile -print -o -name '*.mak' -print -o -name '*.bat' -print -o -name '*.sys' -print)`; do
    echo  $LN_S -f $abssrcdir/${i#./} $i
    $LN_S -f $abssrcdir/${i#./} $i
  done
  for i in `(cd $abssrcdir; find man -name '*.1')`; do
    cp $abssrcdir/$i $i
  done
fi
AC_MSG_NOTICE([Creating plug-in hooks...])
PLUGINSUBDIRS=
ST_PLUGINSUBDIRS=
$srcdir/scripts/mkpluginhooks clean
for i in $(echo $enable_plugins | tr ',' ' '); do
    if test -z "$i"; then continue; fi
    if $srcdir/scripts/plugctl.sh $i yes --disable-option-checking $ac_configure_args; then
        PLUGINSUBDIRS="$PLUGINSUBDIRS plugin/$i"
        if ! $EGREP 'USE_DL_PLUGINS|NO_LIB' $srcdir/src/plugin/$i/Makefile >/dev/null ; then
            ST_PLUGINSUBDIRS="$ST_PLUGINSUBDIRS plugin/$i"
        fi
    fi
done
PLUGINS=`echo $PLUGINSUBDIRS | $SED 's/plugin\//\n\t/g'`

FDPP_PLU=`echo $PLUGINS | $EGREP 'fdpp'`
AC_ARG_ENABLE(fdpp,
  [AS_HELP_STRING([--disable-fdpp], [compile without fdpp support])],,
  enable_fdpp="yes"
)

AC_ARG_ENABLE(system_wa,
  AS_HELP_STRING([--disable-system-wa], [compile without system-specific
  work-arounds (mostly for linux kernel) that are not needed on this system]))
AS_IF([test "x$enable_system_wa" = "xno"], [
  AC_MSG_NOTICE(Disabling system-specific work-arounds)
  AC_DEFINE(DISABLE_SYSTEM_WA)
  AS_IF([test -z "$WARN_OUTDATED_WA" -a -n "$WARN_UNDISABLED_WA"], [
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
if test "$enable_debug" != "no"; then
  AC_MSG_NOTICE(Compiling with debug info...)
  DOSEMU_CFLAGS="$DOSEMU_CFLAGS -ggdb3"
  if test "$use_clang" = "yes" ; then
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -fdebug-macro"
  fi
  AC_ARG_ENABLE(xbacktrace, AS_HELP_STRING([--enable-xbacktrace],
    [enable extended backtrace functionality]))
  if test "$enable_xbacktrace" = "yes"; then
    AC_CHECK_HEADER(bfd.h, [
      AC_CHECK_LIB(z, inflate)
      dnl check iberty before bfd - order matters
      AC_CHECK_LIB(iberty, lrealpath)
      AC_CHECK_LIB(bfd, bfd_init)
      if test "$ac_cv_lib_bfd_bfd_init" = "yes"; then
        HAVE_LIBBFD=1
        AC_DEFINE(HAVE_LIBBFD)
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
    AC_CHECK_FUNCS([backtrace backtrace_symbols])
  ])
else
  AC_MSG_NOTICE(Compiling without debug info...)
fi

AC_ARG_ENABLE(profile,
               AS_HELP_STRING([--enable-profile], [compile with profile info]))

AC_ARG_ENABLE(asan, [AS_HELP_STRING(--enable-asan, [enable address sanitizer])])
AC_ARG_ENABLE(lsan, [AS_HELP_STRING(--enable-lsan, [enable leak sanitizer])])
AC_ARG_ENABLE(tsan, [AS_HELP_STRING(--enable-tsan, [enable thread sanitizer])])
AC_ARG_ENABLE(ubsan, [AS_HELP_STRING(--enable-ubsan, [enable UB sanitizer])])

AC_SUBST(OPTIONALSUBDIRS)
AC_SUBST(REQUIRED)
AC_SUBST(PLUGINSUBDIRS)
AC_SUBST(ST_PLUGINSUBDIRS)
RELEASE_DATE=`cd $srcdir && ./getversion -d`
AC_SUBST(RELEASE_DATE)

dnl CPU emulator
AC_ARG_ENABLE(cpuemu, [AS_HELP_STRING([--disable-cpuemu],
        [do NOT compile with optional x86 emulation code])])
if test "$enable_cpuemu" != "no"; then
  AC_MSG_NOTICE(Compiling with x86 emulator)
  AC_DEFINE(X86_EMULATOR)
  X86_EMULATOR=1
  AC_SUBST(X86_EMULATOR)

  if test "$machine" = "i386" -o "$machine" = "x86_64"; then
    AC_ARG_ENABLE(cpuemu_jit, [AS_HELP_STRING([--disable-cpuemu-jit],
        [do NOT compile with optional x86 JIT])])
    if test "$enable_cpuemu_jit" != "no"; then
      AC_MSG_NOTICE(Compiling with x86 JIT)
      AC_DEFINE(X86_JIT)
      X86_JIT=1
      AC_SUBST(X86_JIT)
    fi
  fi
fi

AC_ARG_WITH(target_cpu, [AS_HELP_STRING([--with-target-cpu=CPU],
  [use the specified target CPU. default=auto])])
if test "$with_target_cpu" = ""; then
  AC_MSG_NOTICE(Compiling with default target CPU...)
  target_cpu=""
else
  AC_MSG_NOTICE(Compiling with specified target CPU...)
  target_cpu=$with_target_cpu
fi

if test "$machine" = "i386" -o "$with_target_bits" = "32" -o \
	"$enable_profile" = "yes"; then
    AC_MSG_NOTICE(Compiling as non-PIE...)
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -fno-pie"
    AX_CHECK_COMPILE_FLAG([-no-pie],
      [DOSEMU_LDFLAGS="$DOSEMU_LDFLAGS -no-pie"])
else
    AC_MSG_NOTICE(Compiling as PIE for $machine...)
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -fpie"
    DOSEMU_LDFLAGS="$DOSEMU_LDFLAGS -pie"
fi

if test "$CONFIG_HOST" = "Linux"; then
  if test "$machine" = "i386" -o "$machine" = "x86_64"; then
    AC_ARG_ENABLE(dnative, [AS_HELP_STRING([--disable-dnative],
        [do NOT compile with native DPMI support])])
    if test "$enable_dnative" != "no"; then
      AC_MSG_NOTICE([Enabling native DPMI support...])
      AC_DEFINE(DNATIVE)
      DNATIVE=1
      AC_SUBST(DNATIVE)
    fi

    AC_DEFINE(USE_KVM)
    KVM=1
    AC_SUBST(KVM)
  fi
fi

if test "$CONFIG_HOST" = "Darwin"; then
   DOSEMU_LDFLAGS="$DOSEMU_LDFLAGS -Wl,-undefined,dynamic_lookup"
fi

AC_ARG_ENABLE(mcontext,
               AS_HELP_STRING([--disable-mcontext], [disable fast context switch]))
if test "$enable_mcontext" != "no"; then
    AC_MSG_NOTICE([Enabling fast context switching...])
    AC_DEFINE(MCONTEXT)
    MCONTEXT=1
    AC_SUBST(MCONTEXT)
fi

if test "$enable_asan" = "yes"; then
  AX_CHECK_LINK_FLAG([-fsanitize=address], [
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -fsanitize=address"
    # working around this bug:
    # https://github.com/google/sanitizers/issues/934
    # Linking to libstdc++ seems to be the cure.
    LDFLAGS="$LDFLAGS -fsanitize=address -lstdc++"
    AC_MSG_NOTICE(enabling address sanitizer)
  ])
fi
if test "$enable_lsan" = "yes"; then
  AX_CHECK_LINK_FLAG([-fsanitize=leak], [
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -fsanitize=leak"
    LDFLAGS="$LDFLAGS -fsanitize=leak"
    AC_MSG_NOTICE(enabling leak sanitizer)
  ])
fi
if test "$enable_tsan" = "yes"; then
  AX_CHECK_LINK_FLAG([-fsanitize=thread], [
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -fsanitize=thread"
    LDFLAGS="$LDFLAGS -fsanitize=thread"
    AC_MSG_NOTICE(enabling thread sanitizer)
  ])
fi
if test "$enable_ubsan" = "yes"; then
  AX_CHECK_LINK_FLAG([-fsanitize=undefined], [
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -fsanitize=undefined -fno-sanitize=alignment"
    LDFLAGS="$LDFLAGS -fsanitize=undefined"
    AC_MSG_NOTICE(enabling UB sanitizer)
  ])
fi
if test "$enable_profile" = "yes"; then
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -pg -no-pie"
    LDFLAGS="$LDFLAGS -pg -no-pie"
    AC_MSG_NOTICE(enabling profiling)
fi

AC_ARG_ENABLE(optimization,
               AS_HELP_STRING([--disable-optimization], [disable optimization]))
if test "$enable_optimization" = "no" ; then
  if test "$enable_debug" = "yes"; then
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -Og"
    AC_MSG_NOTICE(optimization set to -Og)
  else
    # simx86 is too slow on gcc without -O
    DOSEMU_CFLAGS="$DOSEMU_CFLAGS -O0"
    AC_MSG_NOTICE(optimization set to -O0)
  fi
elif test -z "$CFLAGS" ; then
  DOSEMU_CFLAGS="$DOSEMU_CFLAGS -O2"
  AC_MSG_NOTICE(optimization set to -O2)
fi

DOSEMU_CFLAGS="${DOSEMU_CFLAGS} ${OPT} ${PIPE}"
DOSEMU_CPPFLAGS="${DOSEMU_CPPFLAGS} -MD -DCFLAGS_STR=\"$DOSEMU_CFLAGS $CFLAGS\""
DOSEMU_VERSION=`cd $srcdir && ./getversion -b`
AS_LDFLAGS="-melf_i386"

AC_SUBST(XASFLAGS)
AC_SUBST(ASFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(AS_LDFLAGS)
AC_SUBST(DOSEMU_CFLAGS)
AC_SUBST(DOSEMU_CPPFLAGS)
AC_SUBST(DOSEMU_LDFLAGS)
AC_SUBST(LIBS)
AC_SUBST(HAVE_LIBBFD)
AC_SUBST(HAVE_LIBBSD)
AC_SUBST(DOSEMU_VERSION)

cmds_rev=3
cmds_ver=0.$cmds_rev
cmdsuff=$PACKAGE_TARNAME-cmds-$cmds_ver
AC_DEFINE_UNQUOTED(CMDS_SUFF, "$cmdsuff")
AC_DEFINE_UNQUOTED(CMDS_REV, $cmds_rev)
AC_SUBST(cmdsuff)

dnl Print some warnings (if neccessary)
if test "$enable_fdpp" != "no" -a -z "$FDPP_PLU"; then
  AC_MSG_ERROR([fdpp not available!])
fi

dnl Create output files. If you add new ones, please do it in order.
man_files=`cd $srcdir && find man -name '*.in' | sed 's/\.in$//'`
AC_DEFUN([AC_DATAROOTDIR_CHECKED])
adl_RECURSIVE_EVAL([$bindir], [e_bindir])
adl_RECURSIVE_EVAL([$datadir], [e_datadir])
AC_SUBST([e_bindir])
AC_SUBST([e_datadir])
AC_CONFIG_FILES([Makefile.conf $man_files etc/dosemu.desktop])

AC_OUTPUT

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
if test -z "$FDPP_PLU"; then
  AC_MSG_RESULT()
  AC_MSG_WARN([fdpp not available!])
fi

AH_TEMPLATE([HAVE_ASSIGNABLE_STDERR],
[ Define if stderr is not const ])

AH_TEMPLATE([HAVE_OPTRESET],
[ Define if optreset is available ])

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

AH_TEMPLATE(HAVE_LIBBFD,
[Define this if you have binutils-devel installed])

AH_TEMPLATE(HAVE_LIBBSD,
[Define this if you have bsd-devel installed])

AH_TEMPLATE(HAVE_EXECINFO,
[Define this if you have execinfo.h in libc])

AH_TEMPLATE([USE_DL_PLUGINS],
[ DEFINE this, if you want dynamically loaded plugins ])

AH_TEMPLATE([X86_EMULATOR],
[ Define this to use the X86 CPU emulator ])

AH_TEMPLATE([X86_JIT],
[ Define this to use the X86 JIT ])

AH_TEMPLATE([DNATIVE],
[ Define this to enable native DPMI back-end ])

AH_TEMPLATE([USE_KVM],
[ Define this to enable kvm support ])

AH_TEMPLATE([MCONTEXT],
[ Define this to enable libmcontext use ])

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

AH_TEMPLATE([CMDS_REV],
[ Commands revision num ])

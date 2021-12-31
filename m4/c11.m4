# From autoconf latest git
# http://git.savannah.gnu.org/gitweb/?p=autoconf.git;a=blob_plain;f=lib/autoconf/c.m4;hb=refs/heads/master

# _AC_C_STD_TRY(STANDARD, TEST-PROLOGUE, TEST-BODY, OPTION-LIST,
#		ACTION-IF-AVAILABLE, ACTION-IF-UNAVAILABLE)
# --------------------------------------------------------------
# Check whether the C compiler accepts features of STANDARD (e.g `c89', `c99')
# by trying to compile a program of TEST-PROLOGUE and TEST-BODY.  If this fails,
# try again with each compiler option in the space-separated OPTION-LIST; if one
# helps, append it to CC.  If eventually successful, run ACTION-IF-AVAILABLE,
# else ACTION-IF-UNAVAILABLE.
AC_DEFUN([_AC_C_STD_TRY],
[AC_MSG_CHECKING([for $CC option to enable ]m4_translit($1, [c], [C])[ features])
AC_CACHE_VAL(ac_cv_prog_cc_$1,
[ac_cv_prog_cc_$1=no
ac_save_CC=$CC
AC_LANG_CONFTEST([AC_LANG_PROGRAM([$2], [$3])])
for ac_arg in '' $4
do
  CC="$ac_save_CC $ac_arg"
  _AC_COMPILE_IFELSE([], [ac_cv_prog_cc_$1=$ac_arg])
  test "x$ac_cv_prog_cc_$1" != "xno" && break
done
rm -f conftest.$ac_ext
CC=$ac_save_CC
])# AC_CACHE_VAL
ac_prog_cc_stdc_options=
case "x$ac_cv_prog_cc_$1" in
  x)
    AC_MSG_RESULT([none needed]) ;;
  xno)
    AC_MSG_RESULT([unsupported]) ;;
  *)
    ac_prog_cc_stdc_options=" $ac_cv_prog_cc_$1"
    CC=$CC$ac_prog_cc_stdc_options
    AC_MSG_RESULT([$ac_cv_prog_cc_$1]) ;;
esac
AS_IF([test "x$ac_cv_prog_cc_$1" != xno], [$5], [$6])
])# _AC_C_STD_TRY

# _AC_C_C99_TEST_HEADER
# ---------------------
# A C header suitable for testing for C99.
AC_DEFUN([_AC_C_C99_TEST_HEADER],
[[#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <wchar.h>
#include <stdio.h>

// Check varargs macros.  These examples are taken from C99 6.10.3.5.
#define debug(...) fprintf (stderr, __VA_ARGS__)
#define showlist(...) puts (#__VA_ARGS__)
#define report(test,...) ((test) ? puts (#test) : printf (__VA_ARGS__))
static void
test_varargs_macros (void)
{
  int x = 1234;
  int y = 5678;
  debug ("Flag");
  debug ("X = %d\n", x);
  showlist (The first, second, and third items.);
  report (x>y, "x is %d but y is %d", x, y);
}

// Check long long types.
#define BIG64 18446744073709551615ull
#define BIG32 4294967295ul
#define BIG_OK (BIG64 / BIG32 == 4294967297ull && BIG64 % BIG32 == 0)
#if !BIG_OK
  your preprocessor is broken;
#endif
#if BIG_OK
#else
  your preprocessor is broken;
#endif
static long long int bignum = -9223372036854775807LL;
static unsigned long long int ubignum = BIG64;

struct incomplete_array
{
  int datasize;
  double data[];
};

struct named_init {
  int number;
  const wchar_t *name;
  double average;
};

typedef const char *ccp;

static inline int
test_restrict (ccp restrict text)
{
  // See if C++-style comments work.
  // Iterate through items via the restricted pointer.
  // Also check for declarations in for loops.
  for (unsigned int i = 0; *(text+i) != '\0'; ++i)
    continue;
  return 0;
}

// Check varargs and va_copy.
static bool
test_varargs (const char *format, ...)
{
  va_list args;
  va_start (args, format);
  va_list args_copy;
  va_copy (args_copy, args);

  const char *str = "";
  int number = 0;
  float fnumber = 0;

  while (*format)
    {
      switch (*format++)
	{
	case 's': // string
	  str = va_arg (args_copy, const char *);
	  break;
	case 'd': // int
	  number = va_arg (args_copy, int);
	  break;
	case 'f': // float
	  fnumber = va_arg (args_copy, double);
	  break;
	default:
	  break;
	}
    }
  va_end (args_copy);
  va_end (args);

  return *str && number && fnumber;
}]])# _AC_C_C99_TEST_HEADER

# _AC_C_C99_TEST_BODY
# -------------------
# A C body suitable for testing for C99, assuming the corresponding header.
AC_DEFUN([_AC_C_C99_TEST_BODY],
[[
  // Check bool.
  _Bool success = false;

  // Check restrict.
  if (test_restrict ("String literal") == 0)
    success = true;
  char *restrict newvar = "Another string";

  // Check varargs.
  success &= test_varargs ("s, d' f .", "string", 65, 34.234);
  test_varargs_macros ();

  // Check flexible array members.
  struct incomplete_array *ia =
    malloc (sizeof (struct incomplete_array) + (sizeof (double) * 10));
  ia->datasize = 10;
  for (int i = 0; i < ia->datasize; ++i)
    ia->data[i] = i * 1.234;

  // Check named initializers.
  struct named_init ni = {
    .number = 34,
    .name = L"Test wide string",
    .average = 543.34343,
  };

  ni.number = 58;

  int dynamic_array[ni.number];
  dynamic_array[ni.number - 1] = 543;

  // work around unused variable warnings
  return (!success || bignum == 0LL || ubignum == 0uLL || newvar[0] == 'x'
	  || dynamic_array[ni.number - 1] != 543);
]])

# AC_PROG_CC_C11 ([ACTION-IF-AVAILABLE], [ACTION-IF-UNAVAILABLE])
# ----------------------------------------------------------------
# If the C compiler is not in ISO C11 mode by default, try to add an
# option to output variable CC to make it so.  This macro tries
# various options that select ISO C11 on some system or another.  It
# considers the compiler to be in ISO C11 mode if it handles _Alignas,
# _Alignof, _Noreturn, _Static_assert, UTF-8 string literals,
# duplicate typedefs, and anonymous structures and unions.
AC_DEFUN([AC_PROG_CC_C11],
[_AC_C_STD_TRY([c11],
[_AC_C_C99_TEST_HEADER[
// Check _Alignas.
char _Alignas (double) aligned_as_double;
char _Alignas (0) no_special_alignment;
extern char aligned_as_int;
char _Alignas (0) _Alignas (int) aligned_as_int;

// Check _Alignof.
enum
{
  int_alignment = _Alignof (int),
  int_array_alignment = _Alignof (int[100]),
  char_alignment = _Alignof (char)
};
_Static_assert (0 < -_Alignof (int), "_Alignof is signed");

// Check _Noreturn.
int _Noreturn does_not_return (void) { for (;;) continue; }

// Check _Static_assert.
struct test_static_assert
{
  int x;
  _Static_assert (sizeof (int) <= sizeof (long int),
                  "_Static_assert does not work in struct");
  long int y;
};

// Check UTF-8 literals.
#define u8 syntax error!
char const utf8_literal[] = u8"happens to be ASCII" "another string";

// Check duplicate typedefs.
typedef long *long_ptr;
typedef long int *long_ptr;
typedef long_ptr long_ptr;

// Anonymous structures and unions -- taken from C11 6.7.2.1 Example 1.
struct anonymous
{
  union {
    struct { int i; int j; };
    struct { int k; long int l; } w;
  };
  int m;
} v1;
]],
[_AC_C_C99_TEST_BODY[
  v1.i = 2;
  v1.w.k = 5;
  _Static_assert ((offsetof (struct anonymous, i)
		   == offsetof (struct anonymous, w.k)),
		  "Anonymous union alignment botch");
]],
dnl Try
dnl GCC		-std=gnu11 (unused restrictive mode: -std=c11)
dnl with extended modes being tried first.
dnl
dnl Do not try -qlanglvl=extc1x, because IBM XL C V12.1 (the latest version as
dnl of September 2012) does not pass the C11 test.  For now, try extc1x when
dnl compiling the C99 test instead, since it enables _Static_assert and
dnl _Noreturn, which is a win.  If -qlanglvl=extc11 or -qlanglvl=extc1x passes
dnl the C11 test in some future version of IBM XL C, we'll add it here,
dnl preferably extc11.
[[-std=gnu11 -std=c11]], [$1], [$2])[]dnl
])# AC_PROG_CC_C11

AC_PREREQ([2.69])
AC_INIT([libasdf], [0.0.0], [https://github.com/asdf-format/libasdf/issues])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([foreign subdir-objects])
LT_INIT([disable-static])

# silent make https://autotools.io/automake/silent.html
# silent rules enabled by default with 'yes'
# disable silent runles with ./configure --disable-silent-rules
AM_SILENT_RULES([yes]) # less verbose make output 

AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_MACRO_DIR([m4])

AC_LANG([C])
AC_PROG_CC
AC_PROG_CC_STDC

AS_IF([test "x$ac_cv_prog_cc_c11" = "xno"],
  [AC_MSG_ERROR([C11 support is required but not available])])


AC_PROG_INSTALL

AC_HEADER_ASSERT # ./configure --disable-assert to define NDEBUG 

# Check headers
AC_CHECK_HEADER([stdlib.h])
AC_CHECK_HEADERS([endian.h machine/endian.h sys/endian.h])
AC_CHECK_DECLS([be64toh], [], [], [[
#ifdef HAVE_ENDIAN_H
#  include <endian.h>
#endif
#ifdef HAVE_MACHINE_ENDIAN_H
#  include <machine/endian.h>
#endif
#ifdef HAVE_SYS_ENDIAN_H
#  include <sys/endian.h>
#endif
]])


# http://www.gnu.org/software/autoconf-archive/ax_valgrind_check.html
# - make check-valgrind
AX_VALGRIND_CHECK
# http://www.gnu.org/software/autoconf-archive/ax_code_coverage.html#ax_code_coverage
# - make check-code-coverage generates coverage report
AX_CODE_COVERAGE

# Additional enable flags
AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug], [Enable debug build (-g -O0)])],
  [enable_debug=yes],
  [enable_debug=no]
)

if test "x$enable_debug" = "xyes"; then
  CFLAGS="$CFLAGS -g -O0"
  AC_DEFINE([DEBUG], [1], [Define if debugging is enabled])
fi

AC_ARG_ENABLE([tool],
  [AS_HELP_STRING([--disable-tool], [Do not build the asdf command-line tool])],
  [asdf_build_tool=$enableval],
  [asdf_build_tool=yes])

AM_CONDITIONAL([ASDF_BUILD_TOOL], [test "x$asdf_build_tool" = xyes])

# Checks for libraries.
# Preferably using pkg-config, could fall back on AC_SEARCH_LIBS if not
AC_PATH_PROG([PKG_CONFIG], [pkg-config], [no])
if test "$PKG_CONFIG" = "no"; then
  AC_MSG_ERROR([pkg-config is required but was not found])
fi

PKG_CHECK_MODULES([FYAML], [libfyaml], [], [
  AC_MSG_ERROR([libfyaml is required but was not found])
])

# Check for homebrew packages in macOS
AS_IF([test "x$asdf_build_tool" = "xyes"], [
  ASDF_CHECK_HOMEBREW_PKG([argp-standalone])

# Shouldn't be needed with glibc but is if we're using the homebrew package, e.g.
  AC_CHECK_LIB([argp], [argp_parse], [
    AC_SUBST([ARGP_LIB], [-largp])
  ])
])

AC_CONFIG_HEADERS([config.h]) # use config.h instead of passing -D in the command line
AC_CONFIG_FILES([Makefile tests/Makefile])
AC_OUTPUT

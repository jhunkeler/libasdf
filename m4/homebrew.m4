AC_DEFUN([ASDF_CHECK_HOMEBREW], [
  AC_REQUIRE([AC_CANONICAL_HOST])
  AS_IF([test "x$host_os" = xdarwin*], [
    AC_PATH_PROG([ASDF_HOMEBREW], [brew], [no])
    AS_IF([test "x$ASDF_HOMEBREW" != xno], [
      AC_MSG_NOTICE([Homebrew found at $ASDF_HOMEBREW])
      ASDF_HAVE_HOMEBREW=yes
    ], [
      AC_MSG_WARN([Homebrew not found in PATH])
      ASDF_HAVE_HOMEBREW=no
    ])
  ])
])


AC_DEFUN([ASDF_CHECK_HOMEBREW_PKG], [
  AC_REQUIRE([ASDF_CHECK_HOMEBREW])

  AS_IF([test "x$ASDF_HAVE_HOMEBREW" = xyes], [
    AC_MSG_CHECKING([for Homebrew package $1])
    asdf_brew_prefix=`$ASDF_HOMEBREW --prefix --installed $1 2>/dev/null`
    AS_IF([test "x$asdf_brew_prefix" != x], [
      AC_MSG_RESULT([found at $asdf_brew_prefix])
      CPPFLAGS="$CPPFLAGS -I$asdf_brew_prefix/include"
      LDFLAGS="$LDFLAGS -L$asdf_brew_prefix/lib"
    ], [
      AC_MSG_RESULT([not found])
      AC_MSG_WARN([Homebrew package '$1' is not installed. Try: brew install $1])
    ])
  ])
])

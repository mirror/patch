# Configure `patch'.
# Copyright 1993, 1997 Free Software Foundation, Inc.
dnl Process this file with autoconf to produce a configure script.

AC_INIT(patch.c)
AC_CONFIG_HEADER(config.h:config.hin)
AC_ARG_PROGRAM

PACKAGE=patch
VERSION=2.1.6
AC_SUBST(PACKAGE)
AC_SUBST(VERSION)

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PATH_PROG(ED_PROGRAM, ed, ed)

AC_AIX
AC_MINIX
AC_ISC_POSIX

AC_C_CONST

AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h limits.h string.h unistd.h varargs.h)

AC_TYPE_OFF_T
AC_TYPE_SIGNAL
AC_TYPE_SIZE_T

# Check for NetBSD 1.0 bug, where memchr(..., 0) returns nonzero.
AC_MSG_CHECKING(for working memchr)
AC_CACHE_VAL(ac_cv_func_memchr,
[AC_TRY_RUN([#include <string.h>
main () { exit (memchr ("", 0, 0) != 0 || memchr ("", 1, 0) != 0); }],
  ac_cv_func_memchr=yes,
  ac_cv_func_memchr=no,
  AC_MSG_WARN([We are cross-compiling so we assume memchr does not work.])
  ac_cv_func_memchr=no)])dnl
AC_MSG_RESULT($ac_cv_func_memchr)
if test $ac_cv_func_memchr = yes; then
  AC_DEFINE(HAVE_MEMCHR)
fi

AC_CHECK_FUNC(getopt_long, , [LIBOBJS="$LIBOBJS getopt1.o getopt.o"])
AC_SUBST(LIBOBJS)
AC_CHECK_FUNCS(_doprintf isascii memcmp mkdir mktemp pathconf sigaction sigprocmask sigsetmask)
AC_REPLACE_FUNCS(memchr rename)
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_VPRINTF

AC_SYS_LONG_FILE_NAMES

AC_OUTPUT(Makefile)

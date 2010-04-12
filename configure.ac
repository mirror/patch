# Copyright (C) 1993, 1997, 1998, 1999, 2002, 2003, 2006, 2009
# Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.

AC_PREREQ([2.65])

AC_INIT([GNU patch],
	m4_esyscmd([build-aux/git-version-gen .tarball-version]),
	[bug-patch@gnu.org])

AC_CONFIG_SRCDIR([src/patch.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AC_ARG_ENABLE([merge],
  [AS_HELP_STRING([--disable-merge],
    [disable support for merging])])
AM_CONDITIONAL([ENABLE_MERGE], [test "$enableval" != no])
AM_CONDITIONAL([ALPHA_VERSION],
  [[echo "$PACKAGE_VERSION" | grep -q -- "-[0-9a-f][0-9a-f]*\\(-dirty\\)\\?$"]])

AM_INIT_AUTOMAKE([1.11.1 -Wall -Werror gnu dist-bzip2 dist-xz color-tests parallel-tests])
AM_SILENT_RULES([yes])

AC_CONFIG_HEADER([config.h:config.hin])

AC_PROG_CC
AC_PROG_CC_STDC
gl_EARLY
gl_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_HEADER_STDC
gl_INIT

AC_TYPE_MODE_T
AC_TYPE_OFF_T
# FIXME: The utime module is obsolete; replace with utimens.
gl_CHECK_TYPE_STRUCT_UTIMBUF

AC_CHECK_HEADERS([fcntl.h utime.h])
AC_CHECK_DECLS([mktemp])
AC_CHECK_FUNCS(geteuid getuid mktemp raise sigaction sigprocmask sigsetmask)
AC_FUNC_SETMODE_DOS
PATCH_FUNC_MKDIR_TAKES_ONE_ARG

AC_PATH_PROG([ED], [ed], [ed])
AC_DEFINE_UNQUOTED([EDITOR_PROGRAM], ["$ED"], [Name of editor program.])
AC_PATH_PROG(TEST_SHELL, [bash sh])

AC_CONFIG_FILES([
  Makefile
  lib/Makefile
  src/Makefile
  tests/Makefile
])
AC_OUTPUT

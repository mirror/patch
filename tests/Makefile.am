# Copyright (C) 1989, 1990, 1991, 1992, 1993, 1997, 1998, 1999, 2002,
# 2003, 2006, 2009, 2010 Free Software Foundation, Inc.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

## Process this file with automake to produce Makefile.in.

TESTS = \
	asymmetric-hunks \
	backup-prefix-suffix \
	copy-rename \
	corrupt-reject-files \
	create-delete \
	crlf-handling \
	dash-o-append \
	file-modes \
	filename-choice \
	git-binary-diff \
	global-reject-files \
	inname \
	line-numbers \
	merge \
	munged-context-format \
	need-filename \
	no-newline-triggers-assert \
	preserve-c-function-names \
	preserve-mode-and-timestamp \
	quoted-filenames \
	read-only-files \
	reject-format \
	remember-backup-files \
	remember-reject-files \
	symlinks \
	unmodified-files

XFAIL_TESTS = \
	dash-o-append

EXTRA_DIST = \
	$(TESTS) \
	test-lib.sh

TESTS_ENVIRONMENT = \
	abs_top_builddir=$(abs_top_builddir); \
	export abs_top_builddir; \
	$(TEST_SHELL)

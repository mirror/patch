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

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = lib src tests
dist_man1_MANS = patch.man
EXTRA_DIST = \
	m4/mkdir.m4 \
	m4/setmode.m4 \
	m4/utimbuf.m4 \
	bootstrap \
	pc

if ALPHA_VERSION
  EXTRA_DIST += README-alpha
  GNU_SERVER = alpha.gnu.org
else
  GNU_SERVER = ftp.gnu.org
endif

tell-upload:
	echo; \
	(echo "Upload with: "; \
	 for archive in $(DIST_ARCHIVES); do \
	   echo "gnupload --to $(GNU_SERVER):patch $$archive"; \
	 done) | \
	 sed -e 1h -e 1s/./=/g -e 1p -e 1x -e '$$p' -e '$$x'; \
	echo
upload: dist tell-upload
uploadcheck: distcheck tell-upload

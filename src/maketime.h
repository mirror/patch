/* Yield time_t from struct partime yielded by partime.  */

/* Copyright (C) 1993, 1994, 1995, 2003, 2006 Paul Eggert
   Distributed under license by the Free Software Foundation, Inc.
   Copyright (C) 2010 Free Software Foundation, Inc..

   This file is also part of RCS.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <time.h>

struct tm *time2tm (time_t, int);
time_t difftm (struct tm const *, struct tm const *);
time_t str2time (char const **, time_t, long);
time_t tm2time (struct tm *, int);
void adjzone (struct tm *, long);

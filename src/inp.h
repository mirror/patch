/* inputting files to be patched */

/* Copyright 1991-2024 Free Software Foundation, Inc.
   Copyright 1986, 1988 Larry Wall

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

extern lin input_lines;		/* how long is input file in lines */

char const *ifetch (lin, bool, size_t *);
bool get_input_file (char *, char const *, mode_t);
void re_input (void);
void scan_input (char *, mode_t);

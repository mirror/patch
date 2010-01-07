/* base85.h  -  encode and decode base85 strings

   Copyright (C) 2010 Free Software Foundation, Inc.
   Written by Andreas Gruenbacher <agruen@gnu.org>.

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

#ifndef BASE85_H
#define BASE85_H

#define BASE85_BUFFER_SIZE(len) (((int)(len) + 3) / 4 * 5 + 2)
extern int base85_encode (char *, char const *, unsigned int);
extern int base85_decode (char *, char const *);

#endif  /* BASE85_H */

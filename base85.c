/* base85.c  -  encode and decode base85 strings

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

#include <sys/types.h>
#include <stdbool.h>

#include "base85.h"

static char const
base85_encode_tab[] = {
  /*  0 */  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  /* 10 */  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
  /* 20 */  'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
  /* 30 */  'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
  /* 40 */  'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
  /* 50 */  'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
  /* 60 */  'y', 'z', '!', '#', '$', '%', '&', '(', ')', '*',
  /* 70 */  '+', '-', ';', '<', '=', '>', '?', '@', '^', '_',
  /* 80 */  '`', '{', '|', '}', '~',
  };

static char const
base85_decode_tab[] = {
  /*   0 */  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /*  16 */  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /*  32 */  -1, 62, -1, 63, 64, 65, 66, -1, 67, 68, 69, 70, -1, 71, -1, -1,
  /*  48 */   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, 72, 73, 74, 75, 76,
  /*  64 */  77, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
  /*  80 */  25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, 78, 79,
  /*  96 */  80, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
  /* 112 */  51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 81, 82, 83, 84, -1,
  /* 128 */  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /* 144 */  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /* 160 */  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /* 176 */  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /* 192 */  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /* 208 */  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /* 224 */  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /* 240 */  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  };

static void
base85_encode_data (char *encoded, char const *buffer, unsigned int len)
{
  while (len)
    {
      unsigned int acc = 0;
      int a, n;

      for (n = 24; n >= 0; n -= 8)
        {
	  a = (unsigned char) *buffer++;
	  acc |= a << n;
	  if (--len == 0)
	    break;
	}
      for (n = 4; n >= 0; n--)
        {
	  encoded[n] = base85_encode_tab[acc % 85];
	  acc /= 85;
	}
      encoded += 5;
    }
  *encoded = 0;
}

static bool
base85_decode_data (char *buffer, char const *encoded, unsigned int len)
{
  while (len)
    {
      unsigned int acc = 0;
      int a, n;

      for (n = 4; n > 0; n--)
	{
	  a = base85_decode_tab[(unsigned char) *encoded++];
	  if (a < 0)
	    return false;
	  acc = (acc * 85) + a;
	}
      a = base85_decode_tab[(unsigned char) *encoded++];
      if (a < 0)
	return false;
      if (acc > 0xffffffff / 85 || 0xffffffff - a < (acc *= 85))
	return false;
      acc += a;

      n = (len > 4) ? 4 : len;
      len -= n;
      for (; n > 0; n--)
        {
	  acc = (acc << 8) | (acc >> 24);
	  *buffer++ = acc;
	}
    }
  return true;
}

static int
base85_encode_length (unsigned int len)
{
  if (len == 0 || len > 52)
    return -1;

  if (len <= 26)
    return (unsigned char) 'A' + len - 1;
  else
    return (unsigned char) 'a' + len - 27;
}

static int
base85_decode_length (char c)
{
  if (c >= 'A' && c <= 'Z')
    return c - 'A' + 1;
  else if (c >= 'a' && c <= 'z')
    return c - 'a' + 27;
  else
    return -1;
}

int
base85_encode (char *encoded, char const *buffer, unsigned int len)
{
  int c = base85_encode_length (len);

  if (c < 0)
    return -1;
  else
    {
      *encoded++ = c;
      base85_encode_data (encoded, buffer, len);
      return 1 + (len + 3) / 4 * 5;
    }
}

int
base85_decode (char *buffer, char const *encoded)
{
  int len = base85_decode_length (*encoded++);

  if (len > 0 && base85_decode_data (buffer, encoded, len))
    return len;
  else
    return -1;
}

#ifdef DEBUG

#include <stdio.h>
#include <string.h>

#define LINE_MAX (2 * 26)

int main (int argc, char *argv[])
{

  if (! strcmp (argv[1], "-e"))
    {
      char buffer[LINE_MAX];
      char encoded[BASE85_BUFFER_SIZE(LINE_MAX)];
      ssize_t len;

      while ((len = fread (buffer, 1, sizeof (buffer), stdin)) > 0)
        {
	  if (base85_encode (encoded, buffer, len) < 0)
	    return 1;
	  puts (encoded);
	}
    }
  else if (! strcmp (argv[1], "-d"))
    {
      char encoded[BASE85_BUFFER_SIZE(LINE_MAX) + 1];  /* + newline */
      char buffer[LINE_MAX];
      int len;

      while (fgets (encoded, sizeof (encoded), stdin))
        {
	  if ((len = base85_decode (buffer, encoded)) < 0)
	    return 1;
	  fwrite (buffer, 1, len, stdout);
	}
    }
  return 0;
}

#endif

#! /bin/sh

aclocal -I m4 --force
autoheader --force
autoconf --force
echo Run ./configure now ...

@echo off
Rem Configure patch for DJGPP v2.
Rem $Id: configure.bat,v 1.1 1997/05/15 17:59:15 eggert Exp $

Rem Where is our source directory?
Rem ?? Need a comment here explaining this SmallEnv gorp.
set srcdir=.
if not "%srcdir%" == "." goto SmallEnv
if not "%1" == "" set srcdir=%1
if not "%1" == "" if not "%srcdir%" == "%1" goto SmallEnv

Rem Create Makefile
sed -f pc/djgpp/config.sed -e "s|@srcdir@|%srcdir%|g" %srcdir%/Makefile.in >Makefile
sed -n -e "/^VERSION/p" %srcdir%/configure.in >>Makefile

goto Exit

:SmallEnv
echo Your environment size is too small.  Please enlarge it and run me again.

:Exit
set srcdir=

/* $Header: /home/agruen/git/patch-h/cvsroot/patch/common.h,v 1.8 1993/07/30 02:02:51 eggert Exp $
 *
 * $Log: common.h,v $
 * Revision 1.8  1993/07/30 02:02:51  eggert
 * (Chmod, Fputc, Write, VOID): New macros.
 * (malloc, realloc): Yield `VOID *', not `char *'.
 *
 * Revision 1.8  1993/07/30 02:02:51  eggert
 * (Chmod, Fputc, Write, VOID): New macros.
 * (malloc, realloc): Yield `VOID *', not `char *'.
 *
 * Revision 2.0.1.2  88/06/22  20:44:53  lwall
 * patch12: sprintf was declared wrong
 * 
 * Revision 2.0.1.1  88/06/03  15:01:56  lwall
 * patch10: support for shorter extensions.
 * 
 * Revision 2.0  86/09/17  15:36:39  lwall
 * Baseline for netwide release.
 * 
 */

#define DEBUGGING

#include "config.h"

/* shut lint up about the following when return value ignored */

#define Chmod (void)chmod
#define Close (void)close
#define Fclose (void)fclose
#define Fflush (void)fflush
#define Fputc (void)fputc
#define Signal (void)signal
#define Sprintf (void)sprintf
#define Strcat (void)strcat
#define Strcpy (void)strcpy
#define Unlink (void)unlink
#define Write (void)write

/* NeXT declares malloc and realloc incompatibly from us in some of
   these files.  Temporarily redefine them to prevent errors.  */
#define malloc system_malloc
#define realloc system_realloc
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <signal.h>
#undef malloc
#undef realloc

#if HAVE_LIMITS_H
#include <limits.h>
#endif

/* constants */

#ifndef INT_MAX
#define INT_MAX 2147483647
#endif
#ifndef INT_MIN
#define INT_MIN (-1 - INT_MAX)
#endif

/* AIX predefines these.  */
#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
#define TRUE (1)
#define FALSE (0)

#define INITHUNKMAX 125			/* initial dynamic allocation size */
#define MAXLINELEN (8 * 1024)		/* initial input line length */

#define SCCSPREFIX "s."
#define GET "get '%s'"
#define GET_LOCKED "get -e '%s'"
#define SCCSDIFF "get -p '%s' | diff - '%s%s' >/dev/null"

#define RCSSUFFIX ",v"
#define CHECKOUT "co '%s%s'"
#define CHECKOUT_LOCKED "co -l '%s%s'"
#define RCSDIFF "rcsdiff '%s%s' > /dev/null"

/* handy definitions */

#define strNE(s1,s2) (strcmp(s1, s2))
#define strEQ(s1,s2) (!strcmp(s1, s2))
#define strnNE(s1,s2,l) (strncmp(s1, s2, l))
#define strnEQ(s1,s2,l) (!strncmp(s1, s2, l))

/* typedefs */

typedef int bool;			/* must promote to itself */
typedef long LINENUM;			/* must be signed */

/* globals */

EXT struct stat filestat;		/* file statistics area */

EXT char *buf;				/* general purpose buffer */
EXT size_t bufsize INIT(MAXLINELEN);	/* allocated size of buf */

EXT bool using_plan_a INIT(TRUE);	/* try to keep everything in memory */
EXT bool out_of_mem;			/* ran out of memory in plan a */

EXT int filec;				/* how many file arguments? */
EXT char **filearg;
EXT bool ok_to_create_file;

EXT char *outname;

EXT char *origprae;

EXT char *TMPOUTNAME;
EXT char *TMPINNAME;
EXT char *TMPREJNAME;
EXT char *TMPPATNAME;
EXT bool toutkeep;
EXT bool trejkeep;

#ifdef DEBUGGING
EXT int debug INIT(0);
#endif
EXT bool force;
EXT bool batch;
EXT bool verbose INIT(TRUE);
EXT bool reverse;
EXT bool skip_rest_of_patch;
EXT int strippath INIT(INT_MAX);
EXT bool canonicalize;

#define CONTEXT_DIFF 1
#define NORMAL_DIFF 2
#define ED_DIFF 3
#define NEW_CONTEXT_DIFF 4
#define UNI_DIFF 5
EXT int diff_type;

EXT char *revision;			/* prerequisite revision, if any */

#if __STDC__
#define VOID void
#else
#define VOID char
#endif

VOID *xmalloc PARAMS((size_t));
EXITING void my_exit PARAMS((int));

#include <errno.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#include <string.h>
#else
#ifndef errno
extern int errno;
#endif
FILE *popen();
VOID *malloc();
VOID *realloc();
long atol();
char *getenv();
char *strcpy();
char *strcat();
#endif
char *mktemp();
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#else
long lseek();
#endif
#if defined(_POSIX_VERSION) || defined(HAVE_FCNTL_H)
#include <fcntl.h>
#endif

#if !defined(S_ISDIR) && defined(S_IFDIR)
#define	S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#if !defined(S_ISREG) && defined(S_IFREG)
#define	S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

#if ! HAVE_MEMCMP
int memcmp PARAMS((const void *, const void *, size_t));
#endif

/* Define Reg* as either `register' or nothing, depending on whether
   the C compiler pays attention to this many register declarations.
   The intent is that you don't have to order your register declarations
   in the order of importance, so you can freely declare register variables
   in sub-blocks of code and as function parameters.
   Do not use Reg<n> more than once per routine.

   These don't really matter a lot, since most modern C compilers ignore
   register declarations and often do a better job of allocating
   registers than people do.  */

#define Reg1 register
#define Reg2 register
#define Reg3 register
#define Reg4 register
#define Reg5 register
#define Reg6 register
#define Reg7
#define Reg8
#define Reg9
#define Reg10
#define Reg11
#define Reg12
#define Reg13
#define Reg14
#define Reg15
#define Reg16

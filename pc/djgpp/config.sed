/* config.h for compiling `patch' with DJGPP for MS-DOS and MS-Windows.
   Please keep this file as similar as possible to ../../config.h
   to simplify maintenance later.  */

/* This does most of the work; the rest of this file defines only those
   symbols that <sys/config.h> doesn't define correctly.  */
#include <sys/config.h>

/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* #undef _ALL_SOURCE */
#endif

/* Define if the closedir function returns void instead of int.  */
/* #undef CLOSEDIR_VOID */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define if you don't have vprintf but do have _doprnt.  */
/* #undef HAVE_DOPRNT */

/* Define if you support file names longer than 14 characters.  */
#define HAVE_LONG_FILE_NAMES 1

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF 1

/* Define if on MINIX.  */
/* #undef _MINIX */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef mode_t */

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef off_t */

/* Define if the system does not provide POSIX.1 features except
   with this defined.  */
/* #undef _POSIX_1_SOURCE */

/* Define if you need to in order for stat and other things to work.  */
/* #undef _POSIX_SOURCE */

/* Define as the return type of signal handlers (int or void).  */
/* #undef RETSIGTYPE */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if you have the ANSI C header files.  */
/* #undef STDC_HEADERS */

/* Define if there is a member named d_ino in the struct describing
   directory headers.  */
/* #undef D_INO_IN_DIRENT */

/* Define if memchr works.  */
/* #undef HAVE_MEMCHR */

/* Define if `struct utimbuf' is declared -- usually in <utime.h>.  */
#define HAVE_STRUCT_UTIMBUF 1

/* Define if you have the _doprintf function.  */
/* #undef HAVE__DOPRINTF */

/* Define if you have the isascii function.  */
/* #undef HAVE_ISASCII */

/* Define if you have the memchr function.  */
/* #undef HAVE_MEMCHR 1 */

/* Define if you have the memcmp function.  */
#define HAVE_MEMCMP 1

/* Define if you have the mkdir function.  */
/* #undef HAVE_MKDIR */

/* Define if you have the mktemp function.  */
#define HAVE_MKTEMP 1

/* Define if you have the pathconf function.  */
#define HAVE_PATHCONF 1

/* Define if you have the raise function.  */
#define HAVE_RAISE 1

/* Define if you have the rename function.  */
/* #undef HAVE_RENAME */

/* Define if you have the sigaction function.  */
/* #undef HAVE_SIGACTION */

/* Define if you have the sigprocmask function.  */
#define HAVE_SIGPROCMASK 1

/* Define if you have the sigsetmask function.  */
/* #undef HAVE_SIGSETMASK */

/* Define if you have the <dirent.h> header file.  */
/* #undef HAVE_DIRENT_H */

/* Define if you have the <fcntl.h> header file.  */
/* #undef HAVE_FCNTL_H */

/* Define if you have the <limits.h> header file.  */
/* #undef HAVE_LIMITS_H */

/* Define if you have the <ndir.h> header file.  */
/* #undef HAVE_NDIR_H */

/* Define if you have the <string.h> header file.  */
/* #undef HAVE_STRING_H */

/* Define if you have the <sys/dir.h> header file.  */
/* #undef HAVE_SYS_DIR_H */

/* Define if you have the <sys/ndir.h> header file.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <unistd.h> header file.  */
/* #undef HAVE_UNISTD_H */

/* Define if you have the <utime.h> header file.  */
#define HAVE_UTIME_H 1

/* Define if you have the <varargs.h> header file.  */
/* #undef HAVE_VARARGS_H */


/* PC-specific definitions */

#define chdir chdir_safer
int chdir_safer (char const *);

#define FILESYSTEM_PREFIX_LEN(f) ((f)[0] && (f)[1] == ':' ? 2 : 0)
#define ISSLASH(c) ((c) == '/'  ||  (c) == '\\')

#define HAVE_DOS_FILE_NAMES 1

#define HAVE_SETMODE 1
#ifdef WIN32
# define setmode _setmode
#endif

#define TMPDIR "c:"

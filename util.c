/* utility functions for `patch' */

/* $Id: util.c,v 1.14 1997/05/15 17:59:15 eggert Exp $ */

/*
Copyright 1986 Larry Wall
Copyright 1992, 1993, 1997 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING.
If not, write to the Free Software Foundation,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#define XTERN extern
#include <common.h>
#include <backupfile.h>
#include <version.h>
#undef XTERN
#define XTERN
#include <util.h>

#include <time.h>
#include <maketime.h>

#include <signal.h>
#if !defined SIGCHLD && defined SIGCLD
#define SIGCHLD SIGCLD
#endif

#ifdef __STDC__
# include <stdarg.h>
# define vararg_start va_start
#else
# define vararg_start(ap,p) va_start (ap)
# if HAVE_VARARGS_H
#  include <varargs.h>
# else
   typedef char *va_list;
#  define va_dcl int va_alist;
#  define va_start(ap) ((ap) = (va_list) &va_alist)
#  define va_arg(ap, t) (((t *) ((ap) += sizeof (t)))  [-1])
#  define va_end(ap)
# endif
#endif

/* Rename a file, copying it if necessary. */

void
move_file (from, fromstat, to, backup)
     char const *from;
     struct stat const *fromstat;
     char const *to;
     int backup;
{
  register char *s;
  register char *bakname;
  struct stat tost, bakst;
  int try_makedirs_errno = 0;

  if (backup && stat (to, &tost) == 0)
    {
      char *simplename;

      if (origprae || origbase)
	{
	  char const *p = origprae ? origprae : "";
	  char const *b = origbase ? origbase : "";
	  char const *o = base_name (to);
	  size_t plen = strlen (p);
	  size_t tlen = o - to;
	  size_t blen = strlen (b);
	  size_t osize = strlen (o) + 1;
	  bakname = xmalloc (plen + tlen + blen + osize);
	  memcpy (bakname, p, plen);
	  memcpy (bakname + plen, to, tlen);
	  memcpy (bakname + plen + tlen, b, blen);
	  memcpy (bakname + plen + tlen + blen, o, osize);
	  if (FILESYSTEM_PREFIX_LEN (p))
	    try_makedirs_errno = ENOENT;
	  else
	    for (; *p; p++)
	      if (ISSLASH (*p))
		{
		  try_makedirs_errno = ENOENT;
		  break;
		}
	}
      else
	{
	  bakname = find_backup_file_name (to);
	  if (!bakname)
	    memory_fatal ();
	}

      simplename = base_name (bakname);
      /* Find a backup name that is not the same file.
	 Change the first lowercase char into uppercase;
	 if that doesn't suffice, chop off the first char and try again.  */
      while (stat (bakname, &bakst) == 0)
	{
	  try_makedirs_errno = 0;
	  if (tost.st_dev != bakst.st_dev
	      || tost.st_ino != bakst.st_ino)
	    break;
	  /* Skip initial non-lowercase chars.  */
	  for (s=simplename; *s && !ISLOWER ((unsigned char) *s); s++)
	    continue;
	  if (*s)
	    *s = toupper ((unsigned char) *s);
	  else
	    remove_prefix (simplename, 1);
	}
      if (debug & 4)
	  say ("Moving %s to %s.\n", to, bakname);
      while (rename (to, bakname) != 0)
	{
	  if (errno != try_makedirs_errno)
	    pfatal ("can't rename `%s' to `%s'", to, bakname);
	  makedirs (bakname);
	  try_makedirs_errno = 0;
	}
      free (bakname);
    }

  if (debug & 4)
    say ("Moving %s to %s.\n", from, to);

  if (rename (from, to) != 0)
    {
#ifdef EXDEV
      if (errno == EXDEV)
	{
	  if (! backup && unlink (to) != 0
	      && errno != ENOENT && errno != ENOTDIR)
	    pfatal ("can't remove `%s'", to);
	  copy_file (from, fromstat, to);
	  if (unlink (from) != 0)
	    pfatal ("can't remove `%s'", from);
	  return;
	}
#endif
      pfatal ("can't rename `%s' to `%s'", from, to);
    }
}

/* Copy a file. */

void
copy_file (from, fromstat, to)
     char const *from;
     struct stat const *fromstat;
     char const *to;
{
  int tofd;
  int fromfd = open (from, O_RDONLY|O_BINARY);
  size_t i;

  if (fromfd < 0)
    pfatal ("can't reopen `%s'", from);
  if (! (O_CREAT && O_TRUNC))
    close (creat (to, fromstat->st_mode));
  tofd = open (to, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,
	       S_IRUSR|S_IWUSR|fromstat->st_mode);
  if (tofd < 0)
    pfatal ("can't create `%s'", to);
  while ((i = read (fromfd, buf, bufsize)) != 0)
    {
      if (i == -1)
	read_fatal ();
      if (write (tofd, buf, i) != i)
	write_fatal ();
    }
  if (close (fromfd) != 0)
    read_fatal ();
  if (close (tofd) != 0)
    write_fatal ();
}

/* Allocate a unique area for a string. */

char *
savebuf (s, size)
     register char const *s;
     register size_t size;
{
  register char *rv;

  assert (s && size);
  rv = malloc (size);

  if (! rv)
    {
      if (! using_plan_a)
	memory_fatal ();
    }
  else
    memcpy (rv, s, size);

  return rv;
}

char *
savestr(s)
     char const *s;
{
  return savebuf (s, strlen (s) + 1);
}

void
remove_prefix (p, prefixlen)
     char *p;
     size_t prefixlen;
{
  char const *s = p + prefixlen;
  while ((*p++ = *s++))
    continue;
}

#if !HAVE_VPRINTF
#define vfprintf my_vfprintf
static int vfprintf PARAMS ((FILE *, char const *, va_list));
static int
vfprintf (stream, format, args)
     FILE *stream;
     char const *format;
     va_list args;
{
#if !HAVE_DOPRNT && HAVE__DOPRINTF
# define _doprnt _doprintf
#endif
#if HAVE_DOPRNT || HAVE__DOPRINTF
  _doprnt (format, args, stream);
  return ferror (stream) ? -1 : 0;
#else
  int *a = (int *) args;
  return fprintf (stream, format,
		  a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9]);
#endif
}
#endif /* !HAVE_VPRINTF */

/* Terminal output, pun intended. */

#ifdef __STDC__
void
fatal (char const *format, ...)
#else
/*VARARGS1*/ void
fatal (format, va_alist)
     char const *format;
     va_dcl
#endif
{
  va_list args;
  fprintf (stderr, "%s: **** ", program_name);
  vararg_start (args, format);
  vfprintf (stderr, format, args);
  va_end (args);
  putc ('\n', stderr);
  fflush (stderr);
  fatal_exit (0);
}

void
memory_fatal ()
{
  fatal ("out of memory");
}

void
read_fatal ()
{
  pfatal ("read error");
}

void
write_fatal ()
{
  pfatal ("write error");
}

/* Say something from patch, something from the system, then silence . . . */

#ifdef __STDC__
void
pfatal (char const *format, ...)
#else
/*VARARGS1*/ void
pfatal (format, va_alist)
     char const *format;
     va_dcl
#endif
{
  int errnum = errno;
  va_list args;
  fprintf (stderr, "%s: **** ", program_name);
  vararg_start (args, format);
  vfprintf (stderr, format, args);
  va_end (args);
  fflush (stderr);
  errno = errnum;
  perror (" ");
  fflush (stderr);
  fatal_exit (0);
}

/* Tell the user something.  */

#ifdef __STDC__
void
say (char const *format, ...)
#else
/*VARARGS1*/ void
say (format, va_alist)
     char const *format;
     va_dcl
#endif
{
  va_list args;
  vararg_start (args, format);
  vfprintf (stdout, format, args);
  va_end (args);
  fflush (stdout);
}

/* Get a response from the user, somehow or other. */

#ifdef __STDC__
void
ask (char const *format, ...)
#else
/*VARARGS1*/ void
ask (format, va_alist)
     char const *format;
     va_dcl
#endif
{
  static int ttyfd = -2;
  int r;
  va_list args;

  vararg_start (args, format);
  vfprintf (stdout, format, args);
  va_end (args);
  fflush (stdout);

  if (ttyfd == -2)
    {
      ttyfd = open ("/dev/tty", O_RDONLY);
      if (ttyfd < 0)
	{
	  close (ttyfd);
	  for (ttyfd = STDERR_FILENO;  0 <= ttyfd;  ttyfd--)
	    if (isatty (ttyfd))
	      break;
	}
    }

  if (ttyfd < 0)
    {
      /* No terminal at all -- default it.  */
      buf[0] = '\n';
      r = 1;
    }
  else
    {
      size_t s = 0;
      while ((r = read (ttyfd, buf + s, bufsize - 1 - s)) == bufsize - 1 - s
	     && buf[bufsize - 2] != '\n')
	{
	  s = bufsize - 1;
	  bufsize *= 2;
	  buf = realloc (buf, bufsize);
	  if (!buf)
	    memory_fatal ();
	}
      if (r == 0)
	printf ("EOF\n");
      else if (r < 0)
	{
	  close (ttyfd);
	  ttyfd = -1;
	  r = 0;
	}
    }

  buf[r] = '\0';
}

/* How to handle certain events when not in a critical region. */

#define NUM_SIGS (sizeof (sigs) / sizeof (*sigs))
static int const sigs[] = {
#ifdef SIGHUP
       SIGHUP,
#endif
#ifdef SIGTERM
       SIGTERM,
#endif
#ifdef SIGXCPU
       SIGXCPU,
#endif
#ifdef SIGXFSZ
       SIGXFSZ,
#endif
       SIGINT,
       SIGPIPE
};

#if !HAVE_SIGPROCMASK
#define sigset_t int
#define sigemptyset(s) (*(s) = 0)
#ifndef sigmask
#define sigmask(sig) (1 << ((sig) - 1))
#endif
#define sigaddset(s, sig) (*(s) |= sigmask (sig))
#define sigismember(s, sig) ((*(s) & sigmask (sig)) != 0)
#ifndef SIG_BLOCK
#define SIG_BLOCK 0
#endif
#ifndef SIG_UNBLOCK
#define SIG_UNBLOCK (SIG_BLOCK + 1)
#endif
#ifndef SIG_SETMASK
#define SIG_SETMASK (SIG_BLOCK + 2)
#endif
#define sigprocmask(how, n, o) \
  ((how) == SIG_BLOCK \
   ? ((o) ? *(o) = sigblock (*(n)) : sigblock (*(n))) \
   : (how) == SIG_UNBLOCK \
   ? sigsetmask (((o) ? *(o) = sigblock (0) : sigblock (0)) & ~*(n)) \
   : (o ? *(o) = sigsetmask (*(n)) : sigsetmask (*(n))))
#if !HAVE_SIGSETMASK
#define sigblock(mask) 0
#define sigsetmask(mask) 0
#endif
#endif

static sigset_t initial_signal_mask;
static sigset_t signals_to_block;

#if ! HAVE_SIGACTION
static RETSIGTYPE fatal_exit_handler PARAMS ((int)) __attribute__ ((noreturn));
static RETSIGTYPE
fatal_exit_handler (sig)
     int sig;
{
  signal (sig, SIG_IGN);
  fatal_exit (sig);
}
#endif

void
set_signals(reset)
int reset;
{
  int i;
#if HAVE_SIGACTION
  struct sigaction initial_act, fatal_act;
  fatal_act.sa_handler = fatal_exit;
  sigemptyset (&fatal_act.sa_mask);
  fatal_act.sa_flags = 0;
#define setup_handler(sig) sigaction (sig, &fatal_act, (struct sigaction *) 0)
#else
#define setup_handler(sig) signal (sig, fatal_exit_handler)
#endif

  if (!reset)
    {
#ifdef SIGCHLD
      /* System V fork+wait does not work if SIGCHLD is ignored.  */
      signal (SIGCHLD, SIG_DFL);
#endif
      sigemptyset (&signals_to_block);
      for (i = 0;  i < NUM_SIGS;  i++)
	{
	  int ignoring_signal;
#if HAVE_SIGACTION
	  if (sigaction (sigs[i], (struct sigaction *) 0, &initial_act) != 0)
	    continue;
	  ignoring_signal = initial_act.sa_handler == SIG_IGN;
#else
	  ignoring_signal = signal (sigs[i], SIG_IGN) == SIG_IGN;
#endif
	  if (! ignoring_signal)
	    {
	      sigaddset (&signals_to_block, sigs[i]);
	      setup_handler (sigs[i]);
	    }
	}
    }
  else
    {
      /* Undo the effect of ignore_signals.  */
#if HAVE_SIGPROCMASK || HAVE_SIGSETMASK
      sigprocmask (SIG_SETMASK, &initial_signal_mask, (sigset_t *) 0);
#else
      for (i = 0;  i < NUM_SIGS;  i++)
	if (sigismember (&signals_to_block, sigs[i]))
	  setup_handler (sigs[i]);
#endif
    }
}

/* How to handle certain events when in a critical region. */

void
ignore_signals()
{
#if HAVE_SIGPROCMASK || HAVE_SIGSETMASK
  sigprocmask (SIG_BLOCK, &signals_to_block, &initial_signal_mask);
#else
  int i;
  for (i = 0;  i < NUM_SIGS;  i++)
    if (sigismember (&signals_to_block, sigs[i]))
      signal (sigs[i], SIG_IGN);
#endif
}

void
exit_with_signal (sig)
     int sig;
{
  sigset_t s;
  signal (sig, SIG_DFL);
  sigemptyset (&s);
  sigaddset (&s, sig);
  sigprocmask (SIG_UNBLOCK, &s, (sigset_t *) 0);
  kill (getpid (), sig);
  exit (2);
}

int
systemic (command)
     char const *command;
{
  if (debug & 8)
    say ("+ %s\n", command);
  return system (command);
}

#if !HAVE_MKDIR
/* These mkdir and rmdir substitutes are good enough for `patch';
   they are not general emulators.  */

#include <quotearg.h>
static int doprogam PARAMS ((char const *, char const *));
static int mkdir PARAMS ((char const *, int));
static int rmdir PARAMS ((char const *));

static int
doprogram (program, arg)
     char const *program;
     char const *arg;
{
  int result;
  static char const DISCARD_OUTPUT[] = " 2>/dev/null";
  size_t program_len = strlen (program);
  char *cmd = xmalloc (program_len + 1 + quote_system_arg (0, arg)
		       + sizeof DISCARD_OUTPUT);
  char *p = cmd;
  strcpy (p, program);
  p += program_len;
  *p++ = ' ';
  p += quote_system_arg (p, arg);
  strcpy (p, DISCARD_OUTPUT);
  result = systemic (cmd);
  free (cmd);
  return result;
}

static int
mkdir (path, mode)
     char const *path;
     int mode; /* ignored */
{
  return doprogram ("mkdir", path);
}

static int
rmdir (path)
     char const *path;
{
  int result = doprogram ("rmdir", path);
  errno = EEXIST;
  return result;
}
#endif

/* Replace '/' with '\0' in FILENAME if it marks a place that
   needs testing for the existence of directory.  Return the address
   of the last location replaced, or 0 if none were replaced.  */
static char *replace_slashes PARAMS ((char *));
static char *
replace_slashes (filename)
     char *filename;
{
  char *f;
  char *last_location_replaced = 0;
  char const *component_start;

  for (f = filename + FILESYSTEM_PREFIX_LEN (filename);  ISSLASH (*f);  f++)
    continue;

  component_start = f;

  for (; *f; f++)
    if (ISSLASH (*f))
      {
	/* "." and ".." need not be tested.  */
	if (! (f - component_start <= 2
	       && component_start[0] == '.' && f[-1] == '.'))
	  {
	    *f = '\0';
	    last_location_replaced = f;
	  }
	while (ISSLASH (f[1]))
	  f++;
	component_start = f + 1;
      }

  return last_location_replaced;
}

/* Make sure we'll have the directories to create a file.
   Ignore the last element of `filename'.  */

void
makedirs (filename)
     register char *filename;
{
  register char *f;
  register char *flim = replace_slashes (filename);

  if (flim)
    {
      /* Create any missing directories, replacing NULs by '/'s.
	 Ignore errors.  We may have to keep going even after an EEXIST,
	 since the path may contain ".."s; and when there is an EEXIST
	 failure the system may return some other error number.
	 Any problems will eventually be reported when we create the file.  */
      for (f = filename;  f <= flim;  f++)
	if (!*f)
	  {
	    mkdir (filename,
		   S_IRUSR|S_IWUSR|S_IXUSR
		   |S_IRGRP|S_IWGRP|S_IXGRP
		   |S_IROTH|S_IWOTH|S_IXOTH);
	    *f = '/';
	  }
    }
}

/* Remove empty ancestor directories of FILENAME.
   Ignore errors, since the path may contain ".."s, and when there
   is an EEXIST failure the system may return some other error number.  */
void
removedirs (filename)
     char *filename;
{
  size_t i;

  for (i = strlen (filename);  i != 0;  i--)
    if (ISSLASH (filename[i])
	&& ! (ISSLASH (filename[i - 1])
	      || (filename[i - 1] == '.'
		  && (i == 1
		      || ISSLASH (filename[i - 2])
		      || (filename[i - 2] == '.'
			  && (i == 2
			      || ISSLASH (filename[i - 3])))))))
      {
	filename[i] = '\0';
	if (rmdir (filename) == 0 && verbosity == VERBOSE)
	  say ("Removed empty directory `%s'.\n", filename);
	filename[i] = '/';
      }
}

static time_t initial_time;

void
init_time ()
{
  time (&initial_time);
}

/* Make filenames more reasonable. */

char *
fetchname (at, strip_leading, head_says_nonexistent)
char *at;
int strip_leading;
int *head_says_nonexistent;
{
    char *name;
    register char *t;
    int sleading = strip_leading;
    int says_nonexistent = 0;

    if (!at)
	return 0;
    while (ISSPACE ((unsigned char) *at))
	at++;
    if (debug & 128)
	say ("fetchname %s %d\n", at, strip_leading);

    name = at;
    /* Strip off up to `sleading' leading slashes and null terminate.  */
    for (t = at;  *t;  t++)
      {
	if (ISSLASH (*t))
	  {
	    while (ISSLASH (t[1]))
	      t++;
	    if (--sleading >= 0)
		name = t+1;
	  }
	else if (ISSPACE ((unsigned char) *t))
	  {
	    /* The head says the file is nonexistent if the timestamp
	       is the epoch; but the listed time is local time, not UTC,
	       and POSIX.1 allows local time to be 24 hours away from UTC.
	       So match any time within 24 hours of the epoch.
	       Use a default time zone 24 hours behind UTC so that any
	       non-zoned time within 24 hours of the epoch is valid.  */
	    time_t stamp = str2time (t, initial_time, -24L * 60 * 60);
	    if (0 <= stamp && stamp <= 2 * 24L * 60 * 60)
	      says_nonexistent = 1;
	    *t = '\0';
	    break;
	  }
      }

    if (!*name)
      return 0;

    /* Allow files to be created by diffing against /dev/null.  */
    if (strcmp (at, "/dev/null") == 0)
      {
	if (head_says_nonexistent)
	  *head_says_nonexistent = 1;
	return 0;
      }

    if (head_says_nonexistent)
      *head_says_nonexistent = says_nonexistent;

    return savestr (name);
}

VOID *
xmalloc (size)
     size_t size;
{
  register VOID *p = malloc (size);
  if (!p)
    memory_fatal ();
  return p;
}

void
Fseek (stream, offset, ptrname)
     FILE *stream;
     file_offset offset;
     int ptrname;
{
  if (file_seek (stream, offset, ptrname) != 0)
    pfatal ("fseek");
}

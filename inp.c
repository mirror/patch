/* inputting files to be patched */

/* $Id: inp.c,v 1.15 1997/06/09 05:36:28 eggert Exp $ */

/*
Copyright 1986, 1988 Larry Wall
Copyright 1991, 1992, 1993, 1997 Free Software Foundation, Inc.

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
#include <pch.h>
#include <quotearg.h>
#include <util.h>
#undef XTERN
#define XTERN
#include <inp.h>

static char const SCCSPREFIX[] = "s.";
static char const GET[] = "get ";
static char const GET_LOCKED[] = "get -e ";
static char const SCCSDIFF1[] = "get -p ";
static char const SCCSDIFF2[] = "|diff - %s";
static char const SCCSDIFF3[] = ">/dev/null";

static char const RCSSUFFIX[] = ",v";
static char const CHECKOUT[] = "co %s";
static char const CHECKOUT_LOCKED[] = "co -l %s";
static char const RCSDIFF1[] = "rcsdiff %s";
#define RCSDIFF2 SCCSDIFF3

/* Input-file-with-indexable-lines abstract type */

static char const **i_ptr;		/* pointers to lines in plan A buffer */

static size_t tibufsize;		/* size of plan b buffers */
#ifndef TIBUFSIZE_MINIMUM
#define TIBUFSIZE_MINIMUM (8 * 1024)	/* minimum value for tibufsize */
#endif
static int tifd = -1;			/* plan b virtual string array */
static char *tibuf[2];			/* plan b buffers */
static LINENUM tiline[2] = {-1, -1};	/* 1st line in each buffer */
static LINENUM lines_per_buf;		/* how many lines per buffer */
static size_t tireclen;			/* length of records in tmp file */
static size_t last_line_size;		/* size of last input line */

static bool plan_a PARAMS ((char const *));/* yield FALSE if memory runs out */
static void plan_b PARAMS ((char const *));
static void report_revision PARAMS ((int));

/* New patch--prepare to edit another file. */

void
re_input()
{
    if (using_plan_a) {
	if (i_ptr) {
	    free (i_ptr);
	    i_ptr = 0;
	}
    }
    else {
	close (tifd);
	tifd = -1;
	free(tibuf[0]);
	tibuf[0] = 0;
	tiline[0] = tiline[1] = -1;
	tireclen = 0;
    }
}

/* Construct the line index, somehow or other. */

void
scan_input(filename)
char *filename;
{
    using_plan_a = ! (debug & 16) && plan_a (filename);
    if (!using_plan_a)
	plan_b(filename);
    switch (verbosity)
      {
      case SILENT:
	break;

      case VERBOSE:
	say ("Patching file `%s' using Plan %s...\n",
	     filename, using_plan_a ? "A" : "B");
	break;

      case DEFAULT_VERBOSITY:
	say ("patching file `%s'\n", filename);
	break;
      }
}

/* Report whether a desired revision was found.  */

static void
report_revision (found_revision)
     int found_revision;
{
  if (found_revision)
    {
      if (verbosity == VERBOSE)
	say ("Good.  This file appears to be the %s version.\n", revision);
    }
  else if (force)
    {
      if (verbosity != SILENT)
	say ("Warning: this file doesn't appear to be the %s version -- patching anyway.\n",
	     revision);
    }
  else if (batch)
    {
      fatal ("This file doesn't appear to be the %s version -- aborting.",
	     revision);
    }
  else
    {
      ask ("This file doesn't appear to be the %s version -- patch anyway? [n] ",
	   revision);
      if (*buf != 'y')
	fatal ("aborted");
    }
}


void
get_input_file (filename, outname)
     char const *filename;
     char const *outname;
{
    int elsewhere = strcmp (filename, outname);

    if (inerrno == -1)
      inerrno = stat (inname, &instat) == 0 ? 0 : errno;

    /* Perhaps look for RCS or SCCS versions.  */
    if (patch_get
	&& (inerrno
	    || (! elsewhere
		&& (/* No one can write to it.  */
		    (instat.st_mode & (S_IWUSR|S_IWGRP|S_IWOTH)) == 0
		    /* Only the owner (who's not me) can write to it.  */
		    || ((instat.st_mode & (S_IWGRP|S_IWOTH)) == 0
			&& instat.st_uid != geteuid ()))))) {
	struct stat cstat;
	char const *cs = 0;
	char const *filebase = base_name (filename);
	char const *dotslash = *filename=='-' ? "./" : "";
	size_t dir_len = filebase - filename;
	size_t filenamelen = strlen (filename);
	size_t maxfixlen = sizeof "SCCS/" - 1 + sizeof SCCSPREFIX - 1;
	size_t maxtrysize = filenamelen + maxfixlen + 1;
	size_t quotelen = quote_system_arg (0, filename);
	size_t maxgetsize = sizeof GET_LOCKED + quotelen + maxfixlen;
	size_t maxdiffsize =
	  (sizeof SCCSDIFF1 + sizeof SCCSDIFF2 + sizeof SCCSDIFF3 - 2
	   + 2 * quotelen + maxfixlen);
	char *trybuf = xmalloc (maxtrysize + maxgetsize + maxdiffsize);
	char *getbuf = trybuf + maxtrysize;
	char *diffbuf = getbuf + maxgetsize;

	strcpy (trybuf, filename);

#define try1(f,a1)    (sprintf (trybuf + dir_len, f, a1),    stat (trybuf, &cstat) == 0)
#define try2(f,a1,a2) (sprintf (trybuf + dir_len, f, a1,a2), stat (trybuf, &cstat) == 0)
	if ((   try2 ("RCS/%s%s", filebase, RCSSUFFIX)
	     || try1 ("RCS/%s"  , filebase)
	     || try2 (    "%s%s", filebase, RCSSUFFIX))
	    &&
	    /* Check that RCS file is not working file.
	       Some hosts don't report file name length errors.  */
	    (inerrno
	     || instat.st_dev != cstat.st_dev
	     || instat.st_ino != cstat.st_ino)) {

	    char *p = getbuf;
	    sprintf (p, elsewhere ? CHECKOUT : CHECKOUT_LOCKED, dotslash);
	    p += strlen (p);
	    p += quote_system_arg (p, filename);
	    *p = '\0';

	    p = diffbuf;
	    sprintf (p, RCSDIFF1, dotslash);
	    p += strlen (p);
	    p += quote_system_arg (p, filename);
	    strcpy (p, RCSDIFF2);

	    cs = "RCS";

	} else if (   try2 ("SCCS/%s%s", SCCSPREFIX, filebase)
		   || try2 (     "%s%s", SCCSPREFIX, filebase)) {

	    char *p = getbuf;
	    sprintf (p, elsewhere ? GET : GET_LOCKED);
	    p += strlen (p);
	    p += quote_system_arg (p, trybuf);
	    *p = '\0';

	    p = diffbuf;
	    strcpy (p, SCCSDIFF1);
	    p += sizeof SCCSDIFF1 - 1;
	    p += quote_system_arg (p, trybuf);
	    sprintf (p, SCCSDIFF2, dotslash);
	    p += strlen (p);
	    p += quote_system_arg (p, filename);
	    strcpy (p, SCCSDIFF3);

	    cs = "SCCS";

	} else if (inerrno && !pch_says_nonexistent (reverse))
	  {
	    errno = inerrno;
	    pfatal ("can't find file `%s'", filename);
	  }
	/* else we can't write to it but it's not under a version
	   control system, so just proceed.  */
	if (cs) {
	    if (!inerrno) {
		if (!elsewhere
		    && (instat.st_mode & (S_IWUSR|S_IWGRP|S_IWOTH)) != 0)
		    /* Somebody can write to it.  */
		    fatal ("file `%s' seems to be locked by somebody else under %s",
			   filename, cs);
		/* It might be checked out unlocked.  See if it's safe to
		   check out the default version locked.  */
		if (verbosity == VERBOSE)
		    say ("Comparing file `%s' to default %s version...\n",
			 filename, cs);
		if (systemic (diffbuf) != 0)
		  {
		    say ("warning: patching file `%s', which does not match default %s version\n",
			 filename, cs);
		    cs = 0;
		  }
	    }

	    if (cs && patch_get < 0)
	      {
		ask ("Get file `%s' from %s%s? [y] ", filename,
		     cs, elsewhere ? "" : " with lock");
		if (*buf == 'n')
		  cs = 0;
	      }

	    if (cs)
	      {
		if (dry_run)
		  {
		    if (inerrno)
		      fatal ("can't do dry run on nonexistent version-controlled file `%s'; invoke `%s' and try again",
			     filename, buf);
		  }
		else
		  {
		    if (verbosity == VERBOSE)
		      say ("Getting file `%s' from %s%s...\n", filename,
			   cs, elsewhere ? "" : " with lock");
		    if (systemic (getbuf) != 0
			|| stat (filename, &instat) != 0)
		      fatal ("can't get file `%s' from %s", filename, cs);
		    inerrno = 0;
		  }
	      }
	}
	free (trybuf);
    }

    if (inerrno)
      {
	instat.st_mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
	instat.st_size = 0;
      }
    else if (! S_ISREG (instat.st_mode))
      fatal ("`%s' is not a regular file -- can't patch", filename);
}


/* Try keeping everything in memory. */

static bool
plan_a(filename)
     char const *filename;
{
  register char const *s;
  register char const *lim;
  register char const **ptr;
  register char *buffer;
  register LINENUM iline;
  size_t size = instat.st_size;
  size_t allocated_bytes_per_input_byte = sizeof *i_ptr + sizeof (char);
  size_t allocated_bytes = (size + 2) * allocated_bytes_per_input_byte;

  /* Fail if arithmetic overflow occurs during size calculations,
     or if storage isn't available.  */
  if (size != instat.st_size
      || size + 2 < 2
      || allocated_bytes / allocated_bytes_per_input_byte != size + 2
      || ! (ptr = (char const **) malloc (allocated_bytes)))
    return FALSE;

  buffer = (char *) (ptr + (size + 2));

  /* Read the input file, but don't bother reading it if it's empty.
     When creating files, the files do not actually exist.  */
  if (size)
    {
      int ifd = open (filename, O_RDONLY|binary_transput);
      size_t buffered = 0, n;
      if (ifd < 0)
	pfatal ("can't open file `%s'", filename);
      while (size - buffered != 0
	     && (n = read (ifd, buffer + buffered, size - buffered)) != 0)
	{
	  if (n == -1)
	    {
	      /* Perhaps size is too large for this host.  */
	      close (ifd);
	      free (ptr);
	      return FALSE;
	    }
	  buffered += n;
	}

      /* Some non-POSIX hosts exaggerate st_size in text mode;
	 or the file may have shrunk!  */
      size = buffered;

      if (close (ifd) != 0)
	read_fatal ();
    }

  /* Scan the buffer and build array of pointers to lines.  */
  iline = 0;
  lim = buffer + size;
  for (s = buffer;  ;  s++)
    {
      ptr[++iline] = s;
      if (! (s = (char *) memchr (s, '\n', lim - s)))
	break;
    }
  if (size && lim[-1] != '\n')
    ptr[++iline] = lim;
  input_lines = iline - 1;

  if (revision)
    {
      char const *rev = revision;
      int rev0 = rev[0];
      int found_revision = 0;
      size_t revlen = strlen (rev);

      if (revlen <= size)
	{
	  char const *limrev = lim - revlen;

	  for (s = buffer;  (s = (char *) memchr (s, rev0, limrev - s));  s++)
	    if (memcmp (s, rev, revlen) == 0
		&& (s == buffer || ISSPACE ((unsigned char) s[-1]))
		&& (s + 1 == limrev || ISSPACE ((unsigned char) s[revlen])))
	      {
		found_revision = 1;
		break;
	      }
	}

      report_revision (found_revision);
    }

  /* Plan A will work.  */
  i_ptr = ptr;
  return TRUE;
}

/* Keep (virtually) nothing in memory. */

static void
plan_b(filename)
     char const *filename;
{
  register FILE *ifp;
  register int c;
  register size_t len;
  register size_t maxlen;
  register int found_revision;
  register size_t i;
  register char const *rev;
  register size_t revlen;
  register LINENUM line;

  if (instat.st_size == 0)
    filename = "/dev/null";
  if (! (ifp = fopen (filename, binary_transput ? "rb" : "r")))
    pfatal ("can't open file `%s'", filename);
  tifd = create_file (TMPINNAME, O_RDWR | O_BINARY, (mode_t) 0);
  i = 0;
  len = 0;
  maxlen = 1;
  rev = revision;
  found_revision = !rev;
  revlen = rev ? strlen (rev) : 0;

  while ((c = getc (ifp)) != EOF)
    {
      len++;

      if (c == '\n')
	{
	  if (maxlen < len)
	      maxlen = len;
	  len = 0;
	}

      if (!found_revision)
	{
	  if (i == revlen)
	    {
	      found_revision = ISSPACE ((unsigned char) c);
	      i = (size_t) -1;
	    }
	  else if (i != (size_t) -1)
	    i = rev[i]==c ? i + 1 : (size_t) -1;

	  if (i == (size_t) -1  &&  ISSPACE ((unsigned char) c))
	    i = 0;
	}
    }

  if (revision)
    report_revision (found_revision);
  Fseek (ifp, (off_t) 0, SEEK_SET);		/* rewind file */
  for (tibufsize = TIBUFSIZE_MINIMUM;  tibufsize < maxlen;  tibufsize <<= 1)
    continue;
  lines_per_buf = tibufsize / maxlen;
  tireclen = maxlen;
  tibuf[0] = xmalloc (2 * tibufsize);
  tibuf[1] = tibuf[0] + tibufsize;

  for (line = 1; ; line++)
    {
      char *p = tibuf[0] + maxlen * (line % lines_per_buf);
      char const *p0 = p;
      if (! (line % lines_per_buf))	/* new block */
	if (write (tifd, tibuf[0], tibufsize) != tibufsize)
	  write_fatal ();
      if ((c = getc (ifp)) == EOF)
	break;

      for (;;)
	{
	  *p++ = c;
	  if (c == '\n')
	    {
	      last_line_size = p - p0;
	      break;
	    }

	  if ((c = getc (ifp)) == EOF)
	    {
	      last_line_size = p - p0;
	      line++;
	      goto EOF_reached;
	    }
	}
    }
 EOF_reached:
  if (ferror (ifp)  ||  fclose (ifp) != 0)
    read_fatal ();

  if (line % lines_per_buf  !=  0)
    if (write (tifd, tibuf[0], tibufsize) != tibufsize)
      write_fatal ();
  input_lines = line - 1;
}

/* Fetch a line from the input file. */

char const *
ifetch (line, whichbuf, psize)
register LINENUM line;
int whichbuf;				/* ignored when file in memory */
size_t *psize;
{
    register char const *q;
    register char const *p;

    if (line < 1 || line > input_lines) {
	*psize = 0;
	return "";
    }
    if (using_plan_a) {
	p = i_ptr[line];
	*psize = i_ptr[line + 1] - p;
	return p;
    } else {
	LINENUM offline = line % lines_per_buf;
	LINENUM baseline = line - offline;

	if (tiline[0] == baseline)
	    whichbuf = 0;
	else if (tiline[1] == baseline)
	    whichbuf = 1;
	else {
	    tiline[whichbuf] = baseline;
	    if (lseek (tifd, (off_t) (baseline/lines_per_buf * tibufsize),
		       SEEK_SET) == -1
		|| read (tifd, tibuf[whichbuf], tibufsize) < 0)
	      read_fatal ();
	}
	p = tibuf[whichbuf] + (tireclen*offline);
	if (line == input_lines)
	    *psize = last_line_size;
	else {
	    for (q = p;  *q++ != '\n';  )
		continue;
	    *psize = q - p;
	}
	return p;
    }
}

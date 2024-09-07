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

#include <common.h>

#include <stdbit.h>

#include <ialloc.h>
#include <quotearg.h>
#include <util.h>
#include <xalloc.h>

#include <inp.h>
#include <safe.h>

/* Input-file-with-indexable-lines abstract type */

static char *i_buffer;			/* plan A buffer */
static char const **i_ptr;		/* pointers to lines in plan A buffer */

static idx_t tibufsize;			/* size of plan b buffers */
#ifndef TIBUFSIZE_MINIMUM
#define TIBUFSIZE_MINIMUM (8 * 1024)	/* minimum value for tibufsize */
#endif
static int tifd = -1;			/* plan b virtual string array */
static char *tibuf[2];			/* plan b buffers */
static lin tiline[2] = {-1, -1};	/* 1st line in each buffer */
static idx_t lines_per_buf;		/* how many lines per buffer */
static idx_t tireclen;			/* length of records in tmp file */
static idx_t last_line_size;		/* size of last input line */
lin input_lines;			/* how long is input file in lines */

static bool plan_a (char *);	/* yield false if memory runs out */
static void plan_b (char *);
static void report_revision (bool);
static void too_many_lines (char const *);
static void lines_too_long (char const *);

/* New patch--prepare to edit another file. */

void
re_input (void)
{
    if (using_plan_a) {
      if (i_buffer)
	{
	  free (i_buffer);
	  i_buffer = 0;
	  free (i_ptr);
	}
    }
    else {
	if (tifd >= 0)
	  close (tifd);
	tifd = -1;
	if (tibuf[0])
	  {
	    free (tibuf[0]);
	    tibuf[0] = 0;
	  }
	tiline[0] = tiline[1] = -1;
	tireclen = 0;
    }
}

/* Construct the line index, somehow or other. */

void
scan_input (char *filename, mode_t file_type)
{
    using_plan_a = ! (debug & 16) && plan_a (filename);
    if (!using_plan_a)
      {
	if (! S_ISREG (file_type))
	  {
	    assert (S_ISLNK (file_type));
	    fatal ("Can't handle %s %s", "symbolic link", quotearg (filename));
	  }
	plan_b(filename);
      }
}

/* Report whether a desired revision was found.  */

static void
report_revision (bool found_revision)
{
  char const *rev = quotearg (revision);

  if (found_revision)
    {
      if (verbosity == VERBOSE)
	say ("Good.  This file appears to be the %s version.\n", rev);
    }
  else if (force)
    {
      if (verbosity != SILENT)
	say ("Warning: this file doesn't appear to be the %s version -- patching anyway.\n",
	     rev);
    }
  else if (batch)
    fatal ("This file doesn't appear to be the %s version -- aborting.",
	   rev);
  else
    {
      ask ("This file doesn't appear to be the %s version -- patch anyway? [n] ",
	   rev);
      if (*patchbuf != 'y')
	fatal ("aborted");
    }
}


static void
too_many_lines (char const *filename)
{
  fatal ("File %s has too many lines", quotearg (filename));
}

static void
lines_too_long (char const *filename)
{
  fatal ("Lines in file %s are too long", quotearg (filename));
}

bool
get_input_file (char *filename, char const *outname, mode_t file_type)
{
    bool elsewhere = strcmp (filename, outname) != 0;
    char const *cs;
    char *diffbuf;
    char *getbuf;

    if (inerrno < 0)
      inerrno = stat_file (filename, &instat);

    /* Perhaps look for RCS or SCCS versions.  */
    if (S_ISREG (file_type)
	&& patch_get
	&& invc != 0
	&& (inerrno
	    || (! elsewhere
		&& (/* No one can write to it.  */
		    (instat.st_mode & (S_IWUSR|S_IWGRP|S_IWOTH)) == 0
		    /* Only the owner (who's not me) can write to it.  */
		    || ((instat.st_mode & (S_IWGRP|S_IWOTH)) == 0
			&& instat.st_uid != geteuid ()))))
	&& (invc = !! (cs = (version_controller
			     (filename, elsewhere,
			      inerrno ? (struct stat *) 0 : &instat,
			      &getbuf, &diffbuf))))) {

	    if (!inerrno) {
		if (!elsewhere
		    && (instat.st_mode & (S_IWUSR|S_IWGRP|S_IWOTH)) != 0)
		    /* Somebody can write to it.  */
		  fatal ("File %s seems to be locked by somebody else under %s",
			 quotearg (filename), cs);
		if (diffbuf)
		  {
		    /* It might be checked out unlocked.  See if it's safe to
		       check out the default version locked.  */

		    if (verbosity == VERBOSE)
		      say ("Comparing file %s to default %s version...\n",
			   quotearg (filename), cs);

		    if (systemic (diffbuf) != 0)
		      {
			say ("warning: Patching file %s, which does not match default %s version\n",
			     quotearg (filename), cs);
			cs = 0;
		      }
		  }
		if (dry_run)
		  cs = 0;
	    }

	    if (cs && version_get (filename, cs, ! inerrno, elsewhere, getbuf,
				   &instat))
	      inerrno = 0;

	    free (getbuf);
	    free (diffbuf);
      }

    if (inerrno)
      {
	instat.st_mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
	instat.st_size = 0;
      }
    else if (! ((S_ISREG (file_type) || S_ISLNK (file_type))
	        && (file_type & S_IFMT) == (instat.st_mode & S_IFMT)))
      {
	say ("File %s is not a %s -- refusing to patch\n",
	     quotearg (filename),
	     S_ISLNK (file_type) ? "symbolic link" : "regular file");
	return false;
      }
    return true;
}


/* Try keeping everything in memory. */

static bool
plan_a (char *filename)
{
  /* Fail if the file size doesn't fit,
     or if storage isn't available.  */
  idx_t size;
  if (ckd_add (&size, instat.st_size, 0))
    return false;
  char *buffer = imalloc (size);
  if (!buffer)
    return false;

  /* Read the input file, but don't bother reading it if it's empty.
     When creating files, the files do not actually exist.  */
  if (size)
    {
      if (S_ISREG (instat.st_mode))
        {
	  int flags = O_RDONLY | binary_transput;
	  idx_t buffered = 0;
	  int ifd;

	  if (! follow_symlinks)
	    flags |= O_NOFOLLOW;
	  ifd = safe_open (filename, flags, 0);
	  if (ifd < 0)
	    pfatal ("can't open file %s", quotearg (filename));

	  while (size - buffered != 0)
	    {
	      ssize_t n = read (ifd, buffer + buffered, size - buffered);
	      if (n == 0)
		{
		  /* Some non-POSIX hosts exaggerate st_size in text mode;
		     or the file may have shrunk!  */
		  size = buffered;
		  break;
		}
	      if (n < 0)
		{
		  /* Perhaps size is too large for this host.  */
		  close (ifd);
		  free (buffer);
		  return false;
		}
	      buffered += n;
	    }

	  if (close (ifd) != 0)
	    read_fatal ();
	}
      else if (S_ISLNK (instat.st_mode))
	{
	  ssize_t n = safe_readlink (filename, buffer, size);
	  if (n < 0)
	    pfatal ("can't read %s %s", "symbolic link", quotearg (filename));
	  size = n;
	}
      else
	{
	  free (buffer);
	  return false;
	}
  }

  /* Scan the buffer and build array of pointers to lines.  */
  char const *lim = buffer + size;
  idx_t iline = 3; /* 1 unused, 1 for SOF,
		      1 for EOF if last line is incomplete.  */
  for (char const *s = buffer;  (s = memchr (s, '\n', lim - s));  s++)
    iline++;
  char const **ptr = ireallocarray (nullptr, iline, sizeof *ptr);
  if (!ptr)
    {
      free (buffer);
      return false;
    }
  iline = 0;
  for (char const *s = buffer; ; s++)
    {
      ptr[++iline] = s;
      if (! (s = memchr (s, '\n', lim - s)))
	break;
    }
  if (size && lim[-1] != '\n')
    ptr[++iline] = lim;
  if (ckd_sub (&input_lines, iline, 1))
    too_many_lines (filename);

  if (revision)
    {
      char const *rev = revision;
      char rev0 = rev[0];
      bool found_revision = false;
      idx_t revlen = strlen (rev);

      if (revlen <= size)
	{
	  char const *limrev = lim - revlen;

	  for (char const *s = buffer; (s = memchr (s, rev0, limrev - s)); s++)
	    if (memcmp (s, rev, revlen) == 0
		&& (s == buffer || c_isspace (s[-1]))
		&& (s + 1 == limrev || c_isspace (s[revlen])))
	      {
		found_revision = true;
		break;
	      }
	}

      report_revision (found_revision);
    }

  /* Plan A will work.  */
  i_buffer = buffer;
  i_ptr = ptr;
  return true;
}

/* Keep (virtually) nothing in memory. */

static void
plan_b (char *filename)
{
  int flags = O_RDONLY | binary_transput;
  int ifd;
  FILE *ifp;
  int c;
  bool found_revision;
  char const *rev;
  lin line = 1;

  if (instat.st_size == 0)
    filename = (char []) { NULL_DEVICE };
  if (! follow_symlinks)
    flags |= O_NOFOLLOW;
  if ((ifd = safe_open (filename, flags, 0)) < 0
      || ! (ifp = fdopen (ifd, binary_transput ? "rb" : "r")))
    pfatal ("Can't open file %s", quotearg (filename));
  if (tmpin.exists)
    {
      /* Reopen the existing temporary file. */
      tifd = create_file (&tmpin, O_RDWR | O_BINARY, 0, true);
    }
  else
    {
      tifd = make_tempfile (&tmpin, 'i', nullptr, O_RDWR | O_BINARY,
			    S_IRUSR | S_IWUSR);
      if (tifd < 0)
	pfatal ("Can't create temporary file %s", tmpin.name);
    }
  ptrdiff_t i = 0;
  idx_t len = 0;
  idx_t maxlen = 1;
  rev = revision;
  found_revision = !rev;
  idx_t revlen = rev ? strlen (rev) : 0;

  while (0 <= (c = getc (ifp)))
    {
      /* Check against SIZE_MAX / 2 + 1 so that the stdc_bit_ceil
	 below has defined behavior.  Check against IDX_MAX / 4 so
	 that the resulting buffer size fits in idx_t.  */
      len++;
      if (MIN (SIZE_MAX / 2, IDX_MAX / 4) + 1 < len)
	lines_too_long (filename);

      if (c == '\n')
	{
	  if (ckd_add (&line, line, 1))
	    too_many_lines (filename);
	  if (maxlen < len)
	      maxlen = len;
	  len = 0;
	}

      if (!found_revision)
	{
	  if (i == revlen)
	    {
	      found_revision = c_isspace (c);
	      i = -1;
	    }
	  else if (0 <= i)
	    i = rev[i] == c ? i + 1 : -1;

	  if (i < 0 && c_isspace (c))
	    i = 0;
	}
    }

  if (ferror (ifp))
    read_fatal ();
  if (revision)
    report_revision (found_revision);
  Fseek (ifp, 0, SEEK_SET);		/* rewind file */

  size_t umaxlen = maxlen;
  tibufsize = stdc_bit_ceil (umaxlen);
  tibufsize = MAX (TIBUFSIZE_MINIMUM, tibufsize);
  lines_per_buf = tibufsize / maxlen;
  tireclen = maxlen;
  tibuf[0] = ximalloc (2 * tibufsize);
  tibuf[1] = tibuf[0] + tibufsize;

  for (line = 1; ; line++)
    {
      char *p = tibuf[0] + maxlen * (line % lines_per_buf);
      char const *p0 = p;
      if (! (line % lines_per_buf))	/* new block */
	if (write (tifd, tibuf[0], tibufsize) != tibufsize)
	  write_fatal ();
      c = getc (ifp);
      if (c < 0)
	break;

      for (;;)
	{
	  *p++ = c;
	  if (c == '\n')
	    {
	      last_line_size = p - p0;
	      break;
	    }

	  c = getc (ifp);
	  if (c < 0)
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

/* Fetch a line from the input file.
   WHICHBUF is ignored when the file is in memory.  */

char const *
ifetch (lin line, bool whichbuf, idx_t *psize)
{
    char const *q;
    char const *p;

    if (line < 1 || line > input_lines) {
	*psize = 0;
	return "";
    }
    if (using_plan_a) {
	p = i_ptr[line];
	*psize = i_ptr[line + 1] - p;
	return p;
    } else {
	idx_t offline = line % lines_per_buf;
	lin baseline = line - offline;

	if (tiline[0] == baseline)
	    whichbuf = false;
	else if (tiline[1] == baseline)
	    whichbuf = true;
	else {
	    tiline[whichbuf] = baseline;
	    if ((lseek (tifd, baseline/lines_per_buf * tibufsize, SEEK_SET)
		 < 0)
		|| read (tifd, tibuf[whichbuf], tibufsize) < 0)
	      read_fatal ();
	}
	p = tibuf[whichbuf] + (tireclen*offline);
	if (line == input_lines)
	    *psize = last_line_size;
	else {
	    for (q = p;  *q++ != '\n';  )
		/* do nothing */ ;
	    *psize = q - p;
	}
	return p;
    }
}

/* $Header: /home/agruen/git/patch-h/cvsroot/patch/inp.c,v 1.6 1993/07/29 20:11:38 eggert Exp $
 *
 * $Log: inp.c,v $
 * Revision 1.6  1993/07/29 20:11:38  eggert
 * (tibufsize): New variable; buffers grow as needed.
 * (TIBUFSIZE_MINIMUM): New macro.
 * (report_revision): New function.
 * (plan_a): Do not search patch as a big string, since that fails
 * if it contains null bytes.
 * Prepend `./' to filenames starting with `-', for RCS and SCCS.
 * If file does not match default RCS/SCCS version, go ahead and patch
 * it anyway; warn about the problem but do not report a fatal error.
 * (plan_b): Do not use a fixed buffer to read lines; read byte by byte
 * instead, so that the lines can be arbitrarily long.  Do not search
 * lines as strings, since they may contain null bytes.
 * (plan_a, plan_b): Report I/O errors.
 * (rev_in_string): Remove.
 * (ifetch): Yield size of line too, since strlen no longer applies.
 * (plan_a, plan_b): No longer exported.
 *
 * Revision 1.6  1993/07/29 20:11:38  eggert
 * (tibufsize): New variable; buffers grow as needed.
 * (TIBUFSIZE_MINIMUM): New macro.
 * (report_revision): New function.
 * (plan_a): Do not search patch as a big string, since that fails
 * if it contains null bytes.
 * Prepend `./' to filenames starting with `-', for RCS and SCCS.
 * If file does not match default RCS/SCCS version, go ahead and patch
 * it anyway; warn about the problem but do not report a fatal error.
 * (plan_b): Do not use a fixed buffer to read lines; read byte by byte
 * instead, so that the lines can be arbitrarily long.  Do not search
 * lines as strings, since they may contain null bytes.
 * (plan_a, plan_b): Report I/O errors.
 * (rev_in_string): Remove.
 * (ifetch): Yield size of line too, since strlen no longer applies.
 * (plan_a, plan_b): No longer exported.
 *
 * Revision 2.0.1.1  88/06/03  15:06:13  lwall
 * patch10: made a little smarter about sccs files
 * 
 * Revision 2.0  86/09/17  15:37:02  lwall
 * Baseline for netwide release.
 * 
 */

#include "EXTERN.h"
#include "common.h"
#include "util.h"
#include "pch.h"
#include "INTERN.h"
#include "inp.h"

/* Input-file-with-indexable-lines abstract type */

static off_t i_size;			/* size of the input file */
static char *i_womp;			/* plan a buffer for entire file */
static char **i_ptr;			/* pointers to lines in i_womp */

static size_t tibufsize;		/* size of plan b buffers */
#define TIBUFSIZE_MINIMUM (8 * 1024)	/* minimum value for tibufsize */
static int tifd = -1;			/* plan b virtual string array */
static char *tibuf[2];			/* plan b buffers */
static LINENUM tiline[2] = {-1, -1};	/* 1st line in each buffer */
static LINENUM lines_per_buf;		/* how many lines per buffer */
static size_t tireclen;			/* length of records in tmp file */
static size_t last_line_size;		/* size of last input line */

static bool plan_a PARAMS((char *));	/* return FALSE if not enough memory */
static void plan_b PARAMS((char *));

/* New patch--prepare to edit another file. */

void
re_input()
{
    if (using_plan_a) {
	i_size = 0;
	if (i_ptr)
	    free(i_ptr);
	if (i_womp)
	    free(i_womp);
	i_womp = 0;
	i_ptr = 0;
    }
    else {
	using_plan_a = TRUE;		/* maybe the next one is smaller */
	Close(tifd);
	tifd = -1;
	free(tibuf[0]);
	tibuf[0] = 0;
	tiline[0] = tiline[1] = -1;
	tireclen = 0;
    }
}

/* Constuct the line index, somehow or other. */

void
scan_input(filename)
char *filename;
{
    if (!plan_a(filename))
	plan_b(filename);
    if (verbose) {
	say3("Patching file %s using Plan %s...\n", filename,
	  (using_plan_a ? "A" : "B") );
    }
}

/* Report whether a desired revision was found.  */

static void
report_revision(found_revision)
     int found_revision;
{
  if (!revision)
    return;
  else if (found_revision)
    {
      if (verbose)
	say2("Good.  This file appears to be the %s version.\n", revision);
    }
  else if (force)
    {
      if (verbose)
	say2("Warning: this file doesn't appear to be the %s version--patching anyway.\n",
	     revision);
    }
  else if (batch)
    {
      fatal2("this file doesn't appear to be the %s version--aborting.",
	     revision);
    }
  else
    {
      ask2("This file doesn't appear to be the %s version--patch anyway? [n] ",
	   revision);
      if (*buf != 'y')
	fatal1("aborted");
    }
}

/* Try keeping everything in memory. */

static bool
plan_a(filename)
char *filename;
{
    int ifd, statfailed;
    Reg1 char *s;
    Reg2 int c;
    Reg5 int found_revision;
    Reg6 size_t i;
    Reg7 char *rev;
    Reg8 size_t revlen;
    Reg9 LINENUM iline;
    char lbuf[MAXLINELEN];
    int elsewhere = strcmp(filename, outname);

    statfailed = stat(filename, &filestat);
    if (statfailed && ok_to_create_file) {
	if (verbose)
	    say2("(Creating file %s...)\n",filename);
	makedirs(filename, TRUE);
	close(creat(filename, 0666));
	statfailed = stat(filename, &filestat);
    }
    /* For nonexistent or read-only files, look for RCS or SCCS versions.  */
    if (statfailed
	|| (! elsewhere
	    && (/* No one can write to it.  */
		(filestat.st_mode & 0222) == 0
		/* I can't write to it.  */
		|| ((filestat.st_mode & 0022) == 0
		    && filestat.st_uid != getuid ())))) {
	struct stat cstat;
	char const *cs = 0;
	char *filebase;
	size_t pathlen;

	filebase = basename(filename);
	pathlen = filebase - filename;

	/* Put any leading path into `s'.
	   Leave room in lbuf for the diff command.  */
	s = lbuf + 20;
	strncpy(s, filename, pathlen);

#define try1(f,a1)	(Sprintf(s + pathlen, f, a1),	stat(s, &cstat) == 0)
#define try2(f,a1,a2)	(Sprintf(s + pathlen, f, a1,a2),stat(s, &cstat) == 0)
	if ((   try2("RCS/%s%s", filebase, RCSSUFFIX)
	     || try1("RCS/%s"  , filebase)
	     || try2(    "%s%s", filebase, RCSSUFFIX))
	    &&
	    /* Check that RCS file is not working file.
	       Some hosts don't report file name length errors.  */
	    (statfailed
	     || (  (filestat.st_dev ^ cstat.st_dev)
		 | (filestat.st_ino ^ cstat.st_ino)))) {
	    char const *dir = *filename=='-' ? "./" : "";
	    Sprintf(buf, elsewhere?CHECKOUT:CHECKOUT_LOCKED, dir, filename);
	    Sprintf(lbuf, RCSDIFF, dir, filename);
	    cs = "RCS";
	} else if (   try2("SCCS/%s%s", SCCSPREFIX, filebase)
		   || try2(     "%s%s", SCCSPREFIX, filebase)) {
	    Sprintf(buf, elsewhere?GET:GET_LOCKED, s);
	    Sprintf(lbuf, SCCSDIFF, s, *filename=='-' ? "./" : "", filename);
	    cs = "SCCS";
	} else if (statfailed)
	    fatal2("can't find %s", filename);
	/* else we can't write to it but it's not under a version
	   control system, so just proceed.  */
	if (cs) {
	    if (!statfailed) {
		if ((filestat.st_mode & 0222) != 0)
		    /* The owner can write to it.  */
		    fatal3("file %s seems to be locked by somebody else under %s",
			   filename, cs);
		/* It might be checked out unlocked.  See if it's safe to
		   check out the default version locked.  */
		if (verbose)
		    say3("Comparing file %s to default %s version...\n",
			 filename, cs);
		if (system(lbuf)) {
		    say3("warning: patching file %s, which does not match default %s version\n",
			   filename, cs);
		    cs = 0;
		}
	    }
	    if (cs) {
		if (verbose)
		    say3("Checking out file %s from %s...\n", filename, cs);
		if (system(buf) || stat(filename, &filestat))
		    fatal3("can't check out file %s from %s", filename, cs);
	    }
	}
    }
    if (!S_ISREG(filestat.st_mode))
	fatal2("%s is not a regular file--can't patch", filename);
    i_size = filestat.st_size;
    if (out_of_mem) {
	set_hunkmax();		/* make sure dynamic arrays are allocated */
	out_of_mem = FALSE;
	return FALSE;			/* force plan b because plan a bombed */
    }
    if ((size_t)(i_size+1) != i_size+1
	|| !(i_womp = malloc((size_t)(i_size+1))))
	return FALSE;
    if ((ifd = open(filename, 0)) < 0)
	pfatal2("can't open file %s", filename);
    if (read(ifd, i_womp, (size_t)i_size) != i_size) {
	Close(ifd);	/* probably means i_size > 15 or 16 bits worth */
	free(i_womp);
	return FALSE;
    }
    if (close (ifd) != 0)
	read_fatal ();

    /* count the lines in the buffer so we know how many pointers we need */

    i = 0;
    iline = 0;
    rev = revision;
    found_revision = !rev;
    revlen = rev ? strlen (rev) : 0;
    for (s = i_womp;  s < i_womp+i_size;  s++) {
	c = *s;
	iline += c == '\n';
	if (!found_revision) {
	    if (i == revlen) {
		found_revision = isspace(c);
		i = (size_t) -1;
	    } else if (i != (size_t) -1)
		i = rev[i]==c ? i + 1 : (size_t) -1;
	    if (i == (size_t) -1  &&  isspace(c))
		i = 0;
	}
    }
    iline += i_size && s[-1] != '\n';
    i_ptr = malloc((size_t)((iline + 2) * sizeof(*i_ptr)));
    if (!i_ptr) {	/* shucks, it was a near thing */
	free(i_womp);
	return FALSE;
    }
    
    /* now scan the buffer and build pointer array */

    iline = 1;
    i_ptr[iline] = i_womp;
    for (s = i_womp;  s < i_womp+i_size;  s++) {
	if (*s == '\n')
	    i_ptr[++iline] = s+1;	/* these are NOT null terminated */
    }
    if (i_size && s[-1] != '\n')
	i_ptr[++iline] = s;
    input_lines = iline - 1;

    report_revision(found_revision);

    return TRUE;			/* plan a will work */
}

/* Keep (virtually) nothing in memory. */

static void
plan_b(filename)
char *filename;
{
    Reg1 FILE *ifp;
    Reg2 int c;
    Reg3 size_t len;
    Reg4 size_t maxlen;
    Reg5 int found_revision;
    Reg6 size_t i;
    Reg7 char *rev;
    Reg8 size_t revlen;
    Reg9 LINENUM line;

    using_plan_a = FALSE;
    if (! (ifp = fopen(filename, "r")))
	pfatal2("can't open file %s", filename);
    if ((tifd = creat(TMPINNAME, 0666)) < 0)
	pfatal2("can't open file %s", TMPINNAME);
    i = 0;
    len = 0;
    maxlen = 1;
    rev = revision;
    found_revision = !rev;
    revlen = rev ? strlen (rev) : 0;
    while ((c = getc(ifp)) != EOF) {
	len++;
	if (c == '\n') {
	    if (maxlen < len)
		maxlen = len;
	    len = 0;
	}
	if (!found_revision) {
	    if (i == revlen) {
		found_revision = isspace(c);
		i = (size_t) -1;
	    } else if (i != (size_t) -1)
		i = rev[i]==c ? i + 1 : (size_t) -1;
	    if (i == (size_t) -1  &&  isspace(c))
		i = 0;
	}
    }
    report_revision(found_revision);
    Fseek(ifp, (off_t)0, SEEK_SET);		/* rewind file */
    for (tibufsize = TIBUFSIZE_MINIMUM;  tibufsize < maxlen;  tibufsize <<= 1)
	continue;
    lines_per_buf = tibufsize / maxlen;
    tireclen = maxlen;
    tibuf[0] = xmalloc (2 * tibufsize);
    tibuf[1] = tibuf[0] + tibufsize;
    for (line=1; ; line++) {
	char *p0 = tibuf[0] + maxlen * (line % lines_per_buf), *p = p0;
	if (! (line % lines_per_buf))	/* new block */
	    if (write(tifd, tibuf[0], tibufsize) != tibufsize)
		write_fatal ();
	if ((c = getc (ifp)) == EOF)
	    break;
	for (;;) {
	    *p++ = c;
	    if (c == '\n') {
		last_line_size = p - p0;
		break;
	    }
	    if ((c = getc (ifp)) == EOF) {
		last_line_size = p - p0;
		line++;
		goto EOF_reached;
	    }
	}
    }
  EOF_reached:
    if (ferror (ifp)  ||  fclose(ifp) != 0)
	read_fatal ();

    if (line % lines_per_buf  !=  0)
	if (write(tifd, tibuf[0], tibufsize) != tibufsize)
	    write_fatal ();
    input_lines = line - 1;
    if (close (tifd) != 0)
	write_fatal ();
    if ((tifd = open(TMPINNAME, 0)) < 0) {
	pfatal2("can't reopen file %s", TMPINNAME);
    }
}

/* Fetch a line from the input file. */

char const *
ifetch(line, whichbuf, psize)
Reg1 LINENUM line;
int whichbuf;				/* ignored when file in memory */
size_t *psize;
{
    Reg1 char *q;
    Reg2 char *p;

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
	    if (lseek(tifd, (off_t)(baseline/lines_per_buf * tibufsize),
		      SEEK_SET) == -1
	        || read(tifd, tibuf[whichbuf], tibufsize) < 0)
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

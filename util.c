#include "EXTERN.h"
#include "common.h"
#include "INTERN.h"
#include "util.h"
#include "backupfile.h"

#ifndef HAVE_STRERROR
static char *
private_strerror (errnum)
     int errnum;
{
  extern char *sys_errlist[];
  extern int sys_nerr;

  if (errnum > 0 && errnum <= sys_nerr)
    return sys_errlist[errnum];
  return "Unknown system error";
}
#define strerror private_strerror
#endif /* !HAVE_STRERROR */

/* Rename a file, copying it if necessary. */

int
move_file(from,to)
char *from, *to;
{
    char bakname[512];
    Reg1 char *s;
    Reg2 int i;
    Reg3 int fromfd;
    struct stat st;

    /* to stdout? */

    if (strEQ(to, "-")) {
#ifdef DEBUGGING
	if (debug & 4)
	    say2("Moving %s to stdout.\n", from);
#endif
	fromfd = open(from, 0);
	if (fromfd < 0)
	    pfatal2("internal error, can't reopen %s", from);
	while ((i=read(fromfd, buf, bufsize)) > 0)
	    if (! fwrite (buf, sizeof (*buf), (size_t) i, stdout))
		write_fatal ();
	if (i < 0  ||  close (fromfd) != 0)
	    read_fatal ();
	return 0;
    }

    if (origprae) {
	Strcpy(bakname, origprae);
	Strcat(bakname, to);
    } else {
#ifndef NODIR
	char *backupname = find_backup_file_name(to);
	if (!backupname)
	    memory_fatal ();
	Strcpy(bakname, backupname);
	free(backupname);
#else /* NODIR */
	Strcpy(bakname, to);
    	Strcat(bakname, simple_backup_suffix);
#endif /* NODIR */
    }

    if (stat(to, &st) == 0) {	/* output file exists */
	dev_t to_device = st.st_dev;
	ino_t to_inode  = st.st_ino;
	char *simplename = bakname;
	
	for (s=bakname; *s; s++) {
	    if (*s == '/')
		simplename = s+1;
	}
	/* Find a backup name that is not the same file.
	   Change the first lowercase char into uppercase;
	   if that isn't sufficient, chop off the first char and try again.  */
	while (stat(bakname, &st) == 0 &&
		to_device == st.st_dev && to_inode == st.st_ino) {
	    /* Skip initial non-lowercase chars.  */
	    for (s=simplename; *s && !islower(*s); s++) ;
	    if (*s)
		*s = toupper(*s);
	    else
		Strcpy(simplename, simplename+1);
	}
	while (unlink(bakname) >= 0) ;	/* while() is for benefit of Eunice */
#ifdef DEBUGGING
	if (debug & 4)
	    say3("Moving %s to %s.\n", to, bakname);
#endif
	if (rename(to, bakname) < 0) {
	    say4("Can't backup %s, output is in %s: %s\n", to, from,
		 strerror(errno));
	    return -1;
	}
	while (unlink(to) >= 0) ;
    }
#ifdef DEBUGGING
    if (debug & 4)
	say3("Moving %s to %s.\n", from, to);
#endif
    if (rename(from, to) < 0) {		/* different file system? */
	Reg4 int tofd;
	
	tofd = creat(to, 0666);
	if (tofd < 0) {
	    say4("Can't create %s, output is in %s: %s\n",
	      to, from, strerror(errno));
	    return -1;
	}
	fromfd = open(from, 0);
	if (fromfd < 0)
	    pfatal2("internal error, can't reopen %s", from);
	while ((i=read(fromfd, buf, bufsize)) > 0)
	    if (write (tofd, buf, (size_t) i) != i)
		write_fatal ();
	if (i < 0  ||  close (fromfd) != 0)
	    read_fatal ();
	if (close(tofd) != 0)
	    write_fatal ();
    }
    Unlink(from);
    return 0;
}

/* Copy a file. */

void
copy_file(from,to)
char *from, *to;
{
    Reg3 int tofd;
    Reg2 int fromfd;
    Reg1 int i;
    
    tofd = creat(to, 0666);
    if (tofd < 0)
	pfatal2("can't create %s", to);
    fromfd = open(from, 0);
    if (fromfd < 0)
	pfatal2("internal error, can't reopen %s", from);
    while ((i=read(fromfd, buf, bufsize)) > 0)
	if (write(tofd, buf, (size_t) i) != i)
	    write_fatal ();
    if (i < 0  ||  close (fromfd) != 0)
	read_fatal ();
    if (close(tofd) != 0)
	write_fatal ();
}

/* Allocate a unique area for a string. */

char *
savebuf(s, size)
Reg1 char *s;
Reg2 size_t size;
{
    Reg3 char *rv;

    assert(s && size);
    rv = malloc(size);
    if (!rv) {
	if (using_plan_a)
	    out_of_mem = TRUE;
	else
	    memory_fatal ();
    }
    else {
	memcpy(rv, s, size);
    }
    return rv;
}

char *
savestr(s)
     char *s;
{
  return savebuf (s, strlen (s) + 1);
}

/* Terminal output, pun intended. */

FILE *
afatal ()
{
    fprintf(stderr, "patch: **** ");
    return stderr;
}

EXITING void
zfatal ()
{
    fputc ('\n', stderr);
    my_exit(1);
}

void
memory_fatal ()
{
  fatal1 ("out of memory");
}

void
read_fatal ()
{
  pfatal1 ("read error");
}

void
write_fatal ()
{
  pfatal1 ("write error");
}

/* Say something from patch, something from the system, then silence . . . */

static int errnum;

FILE *
apfatal ()
{
    errnum = errno;
    return afatal ();
}

EXITING void
zpfatal ()
{
    fprintf(stderr, ": %s\n", strerror(errnum));
    my_exit(1);
}

/* Get a response from the user, somehow or other. */

void
zask ()
{
    int ttyfd;
    int r;
    bool tty_stderr = isatty (STDERR_FILENO);
    size_t buflen = strlen (buf);

    Fflush(stderr);
    write(STDERR_FILENO, buf, buflen);
    if (tty_stderr) {			/* might be redirected to a file */
	r = read(STDERR_FILENO, buf, bufsize);
    }
    else if (isatty(STDOUT_FILENO)) {	/* this may be new file output */
	Fflush(stdout);
	write (STDOUT_FILENO, buf, buflen);
	r = read (STDOUT_FILENO, buf, bufsize);
    }
    else if ((ttyfd = open("/dev/tty", 2)) >= 0 && isatty(ttyfd)) {
					/* might be deleted or unwriteable */
	write(ttyfd, buf, buflen);
	r = read(ttyfd, buf, bufsize);
	Close(ttyfd);
    }
    else if (isatty(STDIN_FILENO)) {	/* this is probably patch input */
	Fflush(stdin);
	write (STDIN_FILENO, buf, buflen);
	r = read (STDIN_FILENO, buf, bufsize);
    }
    else {				/* no terminal at all--default it */
	buf[0] = '\n';
	r = 1;
    }
    if (r <= 0)
	buf[0] = 0;
    else
	buf[r] = '\0';
    if (!tty_stderr)
	say1(buf);
}

/* How to handle certain events when not in a critical region. */

void
set_signals(reset)
int reset;
{
    static RETSIGTYPE (*hupval)(),(*intval)();

    if (!reset) {
#ifdef SIGHUP
	hupval = signal(SIGHUP, SIG_IGN);
	if (hupval != SIG_IGN)
	    hupval = (RETSIGTYPE(*)())my_exit;
#endif
#ifdef SIGINT
	intval = signal(SIGINT, SIG_IGN);
	if (intval != SIG_IGN)
	    intval = (RETSIGTYPE(*)())my_exit;
#endif
    }
#ifdef SIGHUP
    Signal(SIGHUP, hupval);
#endif
#ifdef SIGINT
    Signal(SIGINT, intval);
#endif
}

/* How to handle certain events when in a critical region. */

void
ignore_signals()
{
#ifdef SIGHUP
    Signal(SIGHUP, SIG_IGN);
#endif
#ifdef SIGINT
    Signal(SIGINT, SIG_IGN);
#endif
}

/* Make sure we'll have the directories to create a file.
   If `striplast' is TRUE, ignore the last element of `filename'.  */

void
makedirs(filename,striplast)
Reg1 char *filename;
bool striplast;
{
    char tmpbuf[256];
    Reg2 char *s = tmpbuf;
    char *dirv[20];		/* Point to the NULs between elements.  */
    Reg3 int i;
    Reg4 int dirvp = 0;		/* Number of finished entries in dirv. */

    /* Copy `filename' into `tmpbuf' with a NUL instead of a slash
       between the directories.  */
    while (*filename) {
	if (*filename == '/') {
	    filename++;
	    dirv[dirvp++] = s;
	    *s++ = '\0';
	}
	else {
	    *s++ = *filename++;
	}
    }
    *s = '\0';
    dirv[dirvp] = s;
    if (striplast)
	dirvp--;
    if (dirvp < 0)
	return;

    strcpy(buf, "mkdir");
    s = buf;
    for (i=0; i<=dirvp; i++) {
	struct stat sbuf;

	if (stat(tmpbuf, &sbuf) && errno == ENOENT) {
	    while (*s) s++;
	    *s++ = ' ';
	    strcpy(s, tmpbuf);
	}
	*dirv[i] = '/';
    }
    if (s != buf)
	system(buf);
}

/* Make filenames more reasonable. */

char *
fetchname(at,strip_leading,assume_exists)
char *at;
int strip_leading;
int assume_exists;
{
    char *fullname;
    char *name;
    Reg1 char *t;
    char tmpbuf[200];
    int sleading = strip_leading;
    struct stat st;

    if (!at)
	return 0;
    while (isspace(*at))
	at++;
#ifdef DEBUGGING
    if (debug & 128)
	say4("fetchname %s %d %d\n",at,strip_leading,assume_exists);
#endif
    if (strnEQ(at, "/dev/null", 9))	/* so files can be created by diffing */
	return 0;			/*   against /dev/null. */
    name = fullname = t = savestr(at);

    /* Strip off up to `sleading' leading slashes and null terminate.  */
    for (; *t && !isspace(*t); t++)
	if (*t == '/')
	    if (--sleading >= 0)
		name = t+1;
    *t = '\0';

    /* If no -p option was given (INT_MAX is the default value),
       we were given a relative pathname,
       and the leading directories that we just stripped off all exist,
       put them back on.  */
    if (strip_leading == INT_MAX && name != fullname && *fullname != '/') {
	name[-1] = '\0';
	if (stat(fullname, &st) == 0 && S_ISDIR (st.st_mode)) {
	    name[-1] = '/';
	    name=fullname;
	}
    }

    name = savestr(name);
    free(fullname);

    if (stat(name, &st) && !assume_exists) {
	char *filebase = basename(name);
	size_t pathlen = filebase - name;

	/* Put any leading path into `tmpbuf'.  */
	strncpy(tmpbuf, name, pathlen);

#define try1(f, a1)	(Sprintf(tmpbuf+pathlen,f,a1),	 stat(tmpbuf, &st) == 0)
#define try2(f, a1, a2)	(Sprintf(tmpbuf+pathlen,f,a1,a2),stat(tmpbuf, &st) == 0)
	if (   try2("RCS/%s%s", filebase, RCSSUFFIX)
	    || try1("RCS/%s"  , filebase)
	    || try2(    "%s%s", filebase, RCSSUFFIX)
	    || try2("SCCS/%s%s", SCCSPREFIX, filebase)
	    || try2(     "%s%s", SCCSPREFIX, filebase))
	  return name;
	free(name);
	name = 0;
    }

    return name;
}

char *
xmalloc (size)
     size_t size;
{
  register char *p = malloc (size);
  if (!p)
    memory_fatal ();
  return p;
}

#if ! HAVE_MEMCMP
int
memcmp (a, b, n)
     const void *a, *b;
     size_t n;
{
  const unsigned char *p = a, *q = b;
  while (n--)
    if (*p++ != *q++)
      return p[-1] < q[-1] ? -1 : 1;
  return 0;
}
#endif

void
Fseek (stream, offset, ptrname)
     FILE *stream;
     long offset;
     int ptrname;
{
  if (fseek (stream, offset, ptrname) != 0)
    pfatal1 ("fseek");
}

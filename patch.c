/* $Header: /home/agruen/git/patch-h/cvsroot/patch/patch.c,v 1.8 1993/07/30 02:02:51 eggert Exp $ */

/* patch - a program to apply diffs to original files
 *
 * Copyright 1986, Larry Wall
 *
 * This program may be copied as long as you don't try to make any
 * money off of it, or pretend that you wrote it.
 *
 * $Log: patch.c,v $
 * Revision 1.8  1993/07/30 02:02:51  eggert
 * Remove lint.
 *
 * Revision 1.8  1993/07/30 02:02:51  eggert
 * Remove lint.
 *
 * Revision 2.0.2.0  90/05/01  22:17:50  davison
 * patch12u: unidiff support added
 * 
 * Revision 2.0.1.6  88/06/22  20:46:39  lwall
 * patch12: rindex() wasn't declared
 * 
 * Revision 2.0.1.5  88/06/03  15:09:37  lwall
 * patch10: exit code improved.
 * patch10: better support for non-flexfilenames.
 * 
 * Revision 2.0.1.4  87/02/16  14:00:04  lwall
 * Short replacement caused spurious "Out of sync" message.
 * 
 * Revision 2.0.1.3  87/01/30  22:45:50  lwall
 * Improved diagnostic on sync error.
 * Moved do_ed_script() to pch.c.
 * 
 * Revision 2.0.1.2  86/11/21  09:39:15  lwall
 * Fuzz factor caused offset of installed lines.
 * 
 * Revision 2.0.1.1  86/10/29  13:10:22  lwall
 * Backwards search could terminate prematurely.
 * 
 * Revision 2.0  86/09/17  15:37:32  lwall
 * Baseline for netwide release.
 * 
 * Revision 1.5  86/08/01  20:53:24  lwall
 * Changed some %d's to %ld's.
 * Linted.
 * 
 * Revision 1.4  86/08/01  19:17:29  lwall
 * Fixes for machines that can't vararg.
 * Added fuzz factor.
 * Generalized -p.
 * General cleanup.
 * 
 * 85/08/15 van%ucbmonet@berkeley
 * Changes for 4.3bsd diff -c.
 *
 * Revision 1.3  85/03/26  15:07:43  lwall
 * Frozen.
 * 
 * Revision 1.2.1.9  85/03/12  17:03:35  lwall
 * Changed pfp->_file to fileno(pfp).
 * 
 * Revision 1.2.1.8  85/03/12  16:30:43  lwall
 * Check i_ptr and i_womp to make sure they aren't null before freeing.
 * Also allow ed output to be suppressed.
 * 
 * Revision 1.2.1.7  85/03/12  15:56:13  lwall
 * Added -p option from jromine@uci-750a.
 * 
 * Revision 1.2.1.6  85/03/12  12:12:51  lwall
 * Now checks for normalness of file to patch.
 * 
 * Revision 1.2.1.5  85/03/12  11:52:12  lwall
 * Added -D (#ifdef) option from joe@fluke.
 * 
 * Revision 1.2.1.4  84/12/06  11:14:15  lwall
 * Made smarter about SCCS subdirectories.
 * 
 * Revision 1.2.1.3  84/12/05  11:18:43  lwall
 * Added -l switch to do loose string comparison.
 * 
 * Revision 1.2.1.2  84/12/04  09:47:13  lwall
 * Failed hunk count not reset on multiple patch file.
 * 
 * Revision 1.2.1.1  84/12/04  09:42:37  lwall
 * Branch for sdcrdcf changes.
 * 
 * Revision 1.2  84/11/29  13:29:51  lwall
 * Linted.  Identifiers uniqified.  Fixed i_ptr malloc() bug.  Fixed
 * multiple calls to mktemp().  Will now work on machines that can only
 * read 32767 chars.  Added -R option for diffs with new and old swapped.
 * Various cosmetic changes.
 * 
 * Revision 1.1  84/11/09  17:03:58  lwall
 * Initial revision
 * 
 */

#include "INTERN.h"
#include "common.h"
#include "EXTERN.h"
#include "version.h"
#include "util.h"
#include "pch.h"
#include "inp.h"
#include "backupfile.h"
#include "getopt.h"

/* procedures */

static void reinitialize_almost_everything PARAMS((void));
static EXITING void usage PARAMS((FILE *, int));
static void get_some_switches PARAMS((void));
static LINENUM locate_hunk PARAMS((LINENUM));
static void abort_hunk PARAMS((void));
static void apply_hunk PARAMS((LINENUM));
static void init_output PARAMS((char *));
static void init_reject PARAMS((char *));
static void copy_till PARAMS((LINENUM, bool));
static void spew_output PARAMS((void));
static bool dump_line PARAMS((LINENUM));
static bool patch_match PARAMS((LINENUM, LINENUM, LINENUM));
static bool similar PARAMS((char const *, size_t, char const *, size_t));
static char *make_temp PARAMS((int));

/* TRUE if -E was specified on command line.  */
static int remove_empty_files;

/* TRUE if -R was specified on command line.  */
static int reverse_flag_specified;

static bool noreverse;

static char *do_defines; /* symbol to patch using ifdef, ifndef, etc. */
static char if_defined[] = "#ifdef %s\n";
static char not_defined[] = "#ifndef %s\n";
static char else_defined[] = "#else\n";
static char end_defined[] = "#endif /* %s */\n";

static int Argc;
static char **Argv;

static FILE *ofp;  /* output file pointer */
static FILE *rejfp;  /* reject file pointer */

static int optind_last; /* for restarting plan_b */

static char *rejname;

static LINENUM last_offset;
static LINENUM maxfuzz INIT(2);

static char serrbuf[BUFSIZ];

/* Apply a set of diffs as appropriate. */

int
main(argc,argv)
int argc;
char **argv;
{
    LINENUM where = 0; /* Pacify `gcc -Wall'.  */
    LINENUM newwhere;
    LINENUM fuzz;
    LINENUM mymaxfuzz;
    int hunk = 0;
    int failed = 0;
    int failtotal = 0;
    bool rev_okayed = 0;

    setbuf(stderr, serrbuf);

    buf = xmalloc (bufsize);

    /* Cons up the names of the temporary files.  */
    TMPOUTNAME = make_temp ('o');
    TMPINNAME = make_temp ('i');
    TMPREJNAME = make_temp ('r');
    TMPPATNAME = make_temp ('p');

    {
      char *v;

      v = getenv ("SIMPLE_BACKUP_SUFFIX");
      if (v)
	simple_backup_suffix = v;
#ifndef NODIR
      v = getenv ("VERSION_CONTROL");
      backup_type = get_version (v); /* OK to pass NULL. */
#endif
    }

    /* parse switches */
    Argc = argc;
    Argv = argv;
    filearg = (char **) xmalloc (argc * sizeof (*filearg));
    get_some_switches();
    
    /* make sure we clean up /tmp in case of disaster */
    set_signals(0);

    for (
	open_patch_file(filearg[1]);
	there_is_another_patch();
	reinitialize_almost_everything()
    ) {					/* for each patch in patch file */

	if (!outname)
	    outname = savestr(filearg[0]);
    
	/* for ed script just up and do it and exit */
	if (diff_type == ED_DIFF) {
	    do_ed_script();
	    continue;
	}
    
	/* initialize the patched file */
	if (!skip_rest_of_patch)
	    init_output(TMPOUTNAME);
    
	/* initialize reject file */
	init_reject(TMPREJNAME);
    
	/* find out where all the lines are */
	if (!skip_rest_of_patch)
	    scan_input(filearg[0]);
    
	/* from here on, open no standard i/o files, because malloc */
	/* might misfire and we can't catch it easily */
    
	/* apply each hunk of patch */
	hunk = 0;
	failed = 0;
	rev_okayed = FALSE;
	out_of_mem = FALSE;
	while (another_hunk()) {
	    hunk++;
	    fuzz = 0;
	    mymaxfuzz = pch_context();
	    if (maxfuzz < mymaxfuzz)
		mymaxfuzz = maxfuzz;
	    if (!skip_rest_of_patch) {
		do {
		    where = locate_hunk(fuzz);
		    if (hunk == 1 && !where && !(force|rev_okayed)) {
						/* dwim for reversed patch? */
			if (!pch_swap()) {
			    if (!fuzz)
				say1(
"Not enough memory to try swapped hunk!  Assuming unswapped.\n");
			    continue;
			}
			reverse = !reverse;
			where = locate_hunk(fuzz);  /* try again */
			if (!where) {	    /* didn't find it swapped */
			    if (!pch_swap())         /* put it back to normal */
				fatal1("lost hunk on alloc error!");
			    reverse = !reverse;
			}
			else if (noreverse) {
			    if (!pch_swap())         /* put it back to normal */
				fatal1("lost hunk on alloc error!");
			    reverse = !reverse;
			    say1(
"Ignoring previously applied (or reversed) patch.\n");
			    skip_rest_of_patch = TRUE;
			}
			else if (batch) {
			    if (verbose)
				say3(
"%seversed (or previously applied) patch detected!  %s -R.",
				reverse ? "R" : "Unr",
				reverse ? "Assuming" : "Ignoring");
			}
			else {
			    ask3(
"%seversed (or previously applied) patch detected!  %s -R? [y] ",
				reverse ? "R" : "Unr",
				reverse ? "Assume" : "Ignore");
			    if (*buf == 'n') {
				ask1("Apply anyway? [n] ");
				if (*buf == 'y')
				    rev_okayed = TRUE;
				else
				    skip_rest_of_patch = TRUE;
				where = 0;
				reverse = !reverse;
				if (!pch_swap())  /* put it back to normal */
				    fatal1("lost hunk on alloc error!");
			    }
			}
		    }
		} while (!skip_rest_of_patch && !where && ++fuzz <= mymaxfuzz);

		if (skip_rest_of_patch) {		/* just got decided */
		    Fclose(ofp);
		    ofp = 0;
		}
	    }

	    newwhere = pch_newfirst() + last_offset;
	    if (skip_rest_of_patch) {
		abort_hunk();
		failed++;
		if (verbose)
		    say3("Hunk #%d ignored at %ld.\n", hunk, newwhere);
	    }
	    else if (!where) {
		abort_hunk();
		failed++;
		if (verbose)
		    say3("Hunk #%d failed at %ld.\n", hunk, newwhere);
	    }
	    else {
		apply_hunk(where);
		if (verbose) {
		    say3("Hunk #%d succeeded at %ld", hunk, newwhere);
		    if (fuzz)
			say2(" with fuzz %ld", fuzz);
		    if (last_offset)
			say3(" (offset %ld line%s)",
			    last_offset, last_offset==1?"":"s");
		    say1(".\n");
		}
	    }
	}

	if (out_of_mem && using_plan_a) {
	    optind = optind_last;
	    say1("\n\nRan out of memory using Plan A--trying again...\n\n");
	    if (ofp)
	        Fclose(ofp);
	    ofp = 0;
	    if (rejfp)
	        Fclose(rejfp);
	    rejfp = 0;
	    continue;
	}
    
	assert(hunk);
    
	/* finish spewing out the new file */
	if (!skip_rest_of_patch)
	    spew_output();
	
	/* and put the output where desired */
	ignore_signals();
	if (!skip_rest_of_patch) {
	    struct stat statbuf;
	    char *realout = outname;

	    if (move_file(TMPOUTNAME, outname) < 0) {
		toutkeep = TRUE;
		realout = TMPOUTNAME;
	    }
	    Chmod(realout, filestat.st_mode);

	    if (remove_empty_files && stat(realout, &statbuf) == 0
		&& statbuf.st_size == 0) {
		if (verbose)
		    say2("Removing %s (empty after patching).\n", realout);
	        while (unlink(realout) >= 0) ; /* while is for Eunice.  */
	    }
	}
	if (fclose(rejfp) != 0)
	    write_fatal ();
	rejfp = 0;
	if (failed) {
	    failtotal += failed;
	    if (!rejname) {
		rejname = xmalloc (strlen (outname) + 5);
		Strcpy(rejname, outname);
		addext(rejname, ".rej", '#');
	    }
	    if (skip_rest_of_patch) {
		say4("%d out of %d hunks ignored--saving rejects to %s\n",
		    failed, hunk, rejname);
	    }
	    else {
		say4("%d out of %d hunks failed--saving rejects to %s\n",
		    failed, hunk, rejname);
	    }
	    if (move_file(TMPREJNAME, rejname) < 0)
		trejkeep = TRUE;
	}
	set_signals(1);
    }
    my_exit(failtotal);
}

/* Prepare to find the next patch to do in the patch file. */

static void
reinitialize_almost_everything()
{
    re_patch();
    re_input();

    input_lines = 0;
    last_frozen_line = 0;

    filec = 0;
    if (filearg[0] && !out_of_mem) {
	free(filearg[0]);
	filearg[0] = 0;
    }

    if (outname) {
	free(outname);
	outname = 0;
    }

    last_offset = 0;

    diff_type = 0;

    if (revision) {
	free(revision);
	revision = 0;
    }

    reverse = reverse_flag_specified;
    skip_rest_of_patch = FALSE;

    get_some_switches();

    if (filec >= 2)
	fatal1("you may not change to a different patch file");
}

static char const shortopts[] = "-b:B:cd:D:eEfF:lnNo:p::r:RsStuvV:x:";
static struct option const longopts[] =
{
  {"suffix", required_argument, NULL, 'b'},
  {"prefix", required_argument, NULL, 'B'},
  {"context", no_argument, NULL, 'c'},
  {"directory", required_argument, NULL, 'd'},
  {"ifdef", required_argument, NULL, 'D'},
  {"ed", no_argument, NULL, 'e'},
  {"remove-empty-files", no_argument, NULL, 'E'},
  {"force", no_argument, NULL, 'f'},
  {"fuzz", required_argument, NULL, 'F'},
  {"help", no_argument, NULL, 'h'},
  {"ignore-whitespace", no_argument, NULL, 'l'},
  {"normal", no_argument, NULL, 'n'},
  {"forward", no_argument, NULL, 'N'},
  {"output", required_argument, NULL, 'o'},
  {"strip", optional_argument, NULL, 'p'},
  {"reject-file", required_argument, NULL, 'r'},
  {"reverse", no_argument, NULL, 'R'},
  {"quiet", no_argument, NULL, 's'},
  {"silent", no_argument, NULL, 's'},
  {"skip", no_argument, NULL, 'S'},
  {"batch", no_argument, NULL, 't'},
  {"unified", no_argument, NULL, 'u'},
  {"version", no_argument, NULL, 'v'},
  {"version-control", required_argument, NULL, 'V'},
  {"debug", required_argument, NULL, 'x'},
  {NULL, no_argument, NULL, 0}
};

static EXITING void
usage (stream, status)
     FILE *stream;
     int status;
{
  fprintf(stream, "\
Usage: %s [options] [origfile [patchfile]] [+ [options] [origfile]]...\n",
	  Argv[0]);
  fprintf(stream, "\
Options:\n\
       [-ceEflnNRsStuv] [-b backup-ext] [-B backup-prefix] [-d directory]\n\
       [-D symbol] [-F max-fuzz] [-o out-file] [-p[strip-count]]\n\
       [-r rej-name] [-V {numbered,existing,simple}] [--context]\n\
       [--prefix=backup-prefix] [--suffix=backup-ext] [--ifdef=symbol]\n\
       [--directory=directory] [--ed] [--fuzz=max-fuzz] [--force] [--batch]\n\
       [--ignore-whitespace] [--forward] [--reverse] [--output=out-file]\n");
  fprintf(stream, "\
       [--strip[=strip-count]] [--normal] [--reject-file=rej-name] [--skip]\n\
       [--remove-empty-files] [--quiet] [--silent] [--unified] [--version]\n\
       [--version-control={numbered,existing,simple}] [--help]\n");
  my_exit(status);
}

/* Process switches and filenames up to next '+' or end of list. */

static void
get_some_switches()
{
    Reg1 int optc;

    if (rejname)
	free (rejname);
    rejname = 0;
    optind_last = optind;
    if (optind == Argc)
	return;
    while ((optc = getopt_long (Argc, Argv, shortopts, longopts, (int *) 0))
	   != -1) {
	if (optc == 1) {
	    if (strEQ(optarg, "+"))
		return;
	    filearg[filec++] = savestr(optarg);
	    filearg[filec] = 0;
	}
	else {
	    switch (optc) {
	    case 'b':
		simple_backup_suffix = savestr(optarg);
		break;
	    case 'B':
		origprae = savestr(optarg);
		break;
	    case 'c':
		diff_type = CONTEXT_DIFF;
		break;
	    case 'd':
		if (chdir(optarg) < 0)
		    pfatal2("can't cd to %s", optarg);
		break;
	    case 'D':
		if (!isalpha(*optarg) && '_' != *optarg)
		    fatal1("argument to -D is not an identifier");
	    	do_defines = savestr(optarg);
		break;
	    case 'e':
		diff_type = ED_DIFF;
		break;
	    case 'E':
		remove_empty_files = TRUE;
		break;
	    case 'f':
		force = TRUE;
		break;
	    case 'F':
		maxfuzz = atoi(optarg);
		break;
	    case 'h':
		usage (stdout, 0);
	    case 'l':
		canonicalize = TRUE;
		break;
	    case 'n':
		diff_type = NORMAL_DIFF;
		break;
	    case 'N':
		noreverse = TRUE;
		break;
	    case 'o':
		outname = savestr(optarg);
		break;
	    case 'p':
		if (optarg)
		    strippath = atoi(optarg);
		else
		    strippath = 0;
		break;
	    case 'r':
		rejname = savestr (optarg);
		break;
	    case 'R':
		reverse = TRUE;
		reverse_flag_specified = TRUE;
		break;
	    case 's':
		verbose = FALSE;
		break;
	    case 'S':
		skip_rest_of_patch = TRUE;
		break;
	    case 't':
		batch = TRUE;
		break;
	    case 'u':
		diff_type = UNI_DIFF;
		break;
	    case 'v':
		version();
		my_exit(0);
		break;
	    case 'V':
#ifndef NODIR
		backup_type = get_version (optarg);
#endif
		break;
#ifdef DEBUGGING
	    case 'x':
		debug = atoi(optarg);
		break;
#endif
	    default:
		usage (stderr, 1);
	    }
	}
    }

    /* Process any filename args given after "--".  */
    for (; optind < Argc; ++optind) {
	filearg[filec++] = savestr(Argv[optind]);
    }
    filearg[filec] = 0;
}

/* Attempt to find the right place to apply this hunk of patch. */

static LINENUM
locate_hunk(fuzz)
LINENUM fuzz;
{
    Reg1 LINENUM first_guess = pch_first() + last_offset;
    Reg2 LINENUM offset;
    LINENUM pat_lines = pch_ptrn_lines();
    Reg3 LINENUM max_pos_offset = input_lines - first_guess
				- pat_lines + 1; 
    Reg4 LINENUM max_neg_offset = first_guess - last_frozen_line - 1
				+ pch_context();

    if (!pat_lines)			/* null range matches always */
	return first_guess;
    if (max_neg_offset >= first_guess)	/* do not try lines < 0 */
	max_neg_offset = first_guess - 1;
    if (first_guess <= input_lines
	&& patch_match(first_guess, (LINENUM) 0, fuzz))
	return first_guess;
    for (offset = 1; ; offset++) {
	Reg5 bool check_after = (offset <= max_pos_offset);
	Reg6 bool check_before = (offset <= max_neg_offset);

	if (check_after && patch_match(first_guess, offset, fuzz)) {
#ifdef DEBUGGING
	    if (debug & 1)
		say3("Offset changing from %ld to %ld\n", last_offset, offset);
#endif
	    last_offset = offset;
	    return first_guess+offset;
	}
	else if (check_before && patch_match(first_guess, -offset, fuzz)) {
#ifdef DEBUGGING
	    if (debug & 1)
		say3("Offset changing from %ld to %ld\n", last_offset, -offset);
#endif
	    last_offset = -offset;
	    return first_guess-offset;
	}
	else if (!check_before && !check_after)
	    return 0;
    }
}

/* We did not find the pattern, dump out the hunk so they can handle it. */

static void
abort_hunk()
{
    Reg1 LINENUM i;
    Reg2 LINENUM pat_end = pch_end();
    /* add in last_offset to guess the same as the previous successful hunk */
    LINENUM oldfirst = pch_first() + last_offset;
    LINENUM newfirst = pch_newfirst() + last_offset;
    LINENUM oldlast = oldfirst + pch_ptrn_lines() - 1;
    LINENUM newlast = newfirst + pch_repl_lines() - 1;
    char const *stars = (diff_type >= NEW_CONTEXT_DIFF ? " ****" : "");
    char const *minuses = (diff_type >= NEW_CONTEXT_DIFF ? " ----" : " -----");

    fprintf(rejfp, "***************\n");
    for (i=0; i<=pat_end; i++) {
	switch (pch_char(i)) {
	case '*':
	    if (oldlast < oldfirst)
		fprintf(rejfp, "*** 0%s\n", stars);
	    else if (oldlast == oldfirst)
		fprintf(rejfp, "*** %ld%s\n", oldfirst, stars);
	    else
		fprintf(rejfp, "*** %ld,%ld%s\n", oldfirst, oldlast, stars);
	    break;
	case '=':
	    if (newlast < newfirst)
		fprintf(rejfp, "--- 0%s\n", minuses);
	    else if (newlast == newfirst)
		fprintf(rejfp, "--- %ld%s\n", newfirst, minuses);
	    else
		fprintf(rejfp, "--- %ld,%ld%s\n", newfirst, newlast, minuses);
	    break;
	case ' ': case '-': case '+': case '!':
	    fprintf(rejfp, "%c ", pch_char(i));
	    /* fall into */
	case '\n':
	    pch_write_line (i, rejfp);
	    break;
	default:
	    fatal1("fatal internal error in abort_hunk"); 
	}
    }
}

/* We found where to apply it (we hope), so do it. */

static void
apply_hunk(where)
LINENUM where;
{
    Reg1 LINENUM old = 1;
    Reg2 LINENUM lastline = pch_ptrn_lines();
    Reg3 LINENUM new = lastline+1;
#define OUTSIDE 0
#define IN_IFNDEF 1
#define IN_IFDEF 2
#define IN_ELSE 3
    Reg4 int def_state = OUTSIDE;
    Reg5 char *R_do_defines = do_defines;
    Reg6 LINENUM pat_end = pch_end();

    where--;
    while (pch_char(new) == '=' || pch_char(new) == '\n')
	new++;
    
    while (old <= lastline) {
	if (pch_char(old) == '-') {
	    copy_till(where + old - 1, TRUE);
	    if (R_do_defines) {
		if (def_state == OUTSIDE) {
		    fprintf(ofp, if_defined, R_do_defines);
		    def_state = IN_IFNDEF;
		}
		else if (def_state == IN_IFDEF) {
		    fprintf(ofp, else_defined);
		    def_state = IN_ELSE;
		}
		pch_write_line (old, ofp);
	    }
	    last_frozen_line++;
	    old++;
	}
	else if (new > pat_end) {
	    break;
	}
	else if (pch_char(new) == '+') {
	    copy_till(where + old - 1, TRUE);
	    if (R_do_defines) {
		if (def_state == IN_IFNDEF) {
		    fprintf(ofp, else_defined);
		    def_state = IN_ELSE;
		}
		else if (def_state == OUTSIDE) {
		    fprintf(ofp, if_defined, R_do_defines);
		    def_state = IN_IFDEF;
		}
	    }
	    pch_write_line (new, ofp);
	    new++;
	}
	else if (pch_char(new) != pch_char(old)) {
	    say3("Out-of-sync patch, lines %ld,%ld--mangled text or line numbers, maybe?\n",
		pch_hunk_beg() + old,
		pch_hunk_beg() + new);
#ifdef DEBUGGING
	    say3("oldchar = '%c', newchar = '%c'\n",
		pch_char(old), pch_char(new));
#endif
	    my_exit(1);
	}
	else if (pch_char(new) == '!') {
	    copy_till(where + old - 1, TRUE);
	    if (R_do_defines) {
	       fprintf(ofp, not_defined, R_do_defines);
	       def_state = IN_IFNDEF;
	    }
	    while (pch_char(old) == '!') {
		if (R_do_defines) {
		    pch_write_line (old, ofp);
		}
		last_frozen_line++;
		old++;
	    }
	    if (R_do_defines) {
		fprintf(ofp, else_defined);
		def_state = IN_ELSE;
	    }
	    while (pch_char(new) == '!') {
		pch_write_line (new, ofp);
		new++;
	    }
	}
	else {
	    assert(pch_char(new) == ' ');
	    old++;
	    new++;
	    if (R_do_defines && def_state != OUTSIDE) {
		fprintf(ofp, end_defined, R_do_defines);
		def_state = OUTSIDE;
	    }
	}
    }
    if (new <= pat_end && pch_char(new) == '+') {
	copy_till(where + old - 1, TRUE);
	if (R_do_defines) {
	    if (def_state == OUTSIDE) {
	    	fprintf(ofp, if_defined, R_do_defines);
		def_state = IN_IFDEF;
	    }
	    else if (def_state == IN_IFNDEF) {
		fprintf(ofp, else_defined);
		def_state = IN_ELSE;
	    }
	}
	while (new <= pat_end && pch_char(new) == '+') {
	    pch_write_line (new, ofp);
	    new++;
	}
    }
    if (R_do_defines && def_state != OUTSIDE) {
	fprintf(ofp, end_defined, R_do_defines);
    }
}

/* Open the new file. */

static void
init_output(name)
char *name;
{
    ofp = fopen(name, "w");
    if (!ofp)
	pfatal2("can't create %s", name);
}

/* Open a file to put hunks we can't locate. */

static void
init_reject(name)
char *name;
{
    rejfp = fopen(name, "w");
    if (!rejfp)
	pfatal2("can't create %s", name);
}

/* Copy input file to output, up to wherever hunk is to be applied. */

static void
copy_till(lastline, newline_required)
Reg1 LINENUM lastline;
bool newline_required;
{
    Reg2 LINENUM R_last_frozen_line = last_frozen_line;
    Reg3 missing_newline = FALSE;

    if (R_last_frozen_line > lastline)
	fatal1("misordered hunks! output would be garbled");
    while (R_last_frozen_line < lastline) {
	missing_newline = dump_line (++R_last_frozen_line);
    }
    if (missing_newline & newline_required && putc('\n', ofp) == EOF)
	write_fatal ();
    last_frozen_line = R_last_frozen_line;
}

/* Finish copying the input file to the output file. */

static void
spew_output()
{
#ifdef DEBUGGING
    if (debug & 256)
	say3("il=%ld lfl=%ld\n",input_lines,last_frozen_line);
#endif
    if (input_lines)
	copy_till(input_lines, FALSE);		/* dump remainder of file */
    if (fclose(ofp) != 0)
	write_fatal ();
    ofp = 0;
}

/* Copy one line from input to output. */

static bool
dump_line(line)
LINENUM line;
{
    size_t size;
    char const *s = ifetch(line, 0, &size);

    if (! fwrite(s, sizeof(*s), size, ofp))
	write_fatal ();
    return s[size - 1] != '\n';
}

/* Does the patch pattern match at line base+offset? */

static bool
patch_match(base, offset, fuzz)
LINENUM base;
LINENUM offset;
LINENUM fuzz;
{
    Reg1 LINENUM pline = 1 + fuzz;
    Reg2 LINENUM iline;
    Reg3 LINENUM pat_lines = pch_ptrn_lines() - fuzz;
    size_t size;
    Reg4 char const *p;

    for (iline=base+offset+fuzz; pline <= pat_lines; pline++,iline++) {
	p = ifetch(iline, offset >= 0, &size);
	if (canonicalize) {
	    if (!similar(p, size,
			 pfetch(pline),
			 pch_line_len(pline) ))
		return FALSE;
	}
	else if (size != pch_line_len(pline)
		 || memcmp(p, pfetch(pline), size) != 0)
	    return FALSE;
    }
    return TRUE;
}

/* Do two lines match with canonicalized white space? */

static bool
similar(a, alen, b, blen)
     Reg1 char const *a;
     Reg3 size_t alen;
     Reg2 char const *b;
     Reg4 size_t blen;
{
    while (alen | blen) {
	if (!blen || isspace(*b)) {	/* whitespace (or line end) to match? */
	    if (alen && !isspace(*a))	/* no corresponding whitespace? */
		return FALSE;
	    while (blen && isspace(*b))
		b++, blen--;		/* skip pattern whitespace */
	    while (alen && isspace(*a))
		a++, alen--;		/* skip target whitespace */
	    if (!alen || !blen)
		return alen == blen;	/* should end in sync */
	}
	else if (!alen || *a++ != *b++)	/* match non-whitespace chars */
	    return FALSE;
	else
	    alen--, blen--;
    }
    return TRUE;			/* actually, this is not reached */
					/* since there is always a \n */
}

/* Make a temporary file.  */

static char *
make_temp (letter)
     int letter;
{
  char const *tmpdir = getenv ("TMPDIR");
  char *r;
  if (!tmpdir)
    tmpdir = "/tmp";
  r = xmalloc (strlen (tmpdir) + 14);
  Sprintf (r, "%s/patch%cXXXXXX", tmpdir, letter);
  (void) mktemp (r);
  if (!*r)
    pfatal1 ("mktemp");
  return r;
}

/* Exit with cleanup. */

EXITING void
my_exit(status)
int status;
{
    Unlink(TMPINNAME);
    if (!toutkeep) {
	Unlink(TMPOUTNAME);
    }
    if (!trejkeep) {
	Unlink(TMPREJNAME);
    }
    Unlink(TMPPATNAME);
    exit(status);
}

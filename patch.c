/* patch - a program to apply diffs to original files */

/* $Id: patch.c,v 1.11 1997/04/14 05:32:30 eggert Exp $ */

/*
Copyright 1984, 1985, 1986, 1987, 1988 Larry Wall
Copyright 1989, 1990, 1991, 1992, 1993, 1997 Free Software Foundation, Inc.

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

#define XTERN
#include <common.h>
#undef XTERN
#define XTERN extern
#include <argmatch.h>
#include <backupfile.h>
#include <getopt.h>
#include <inp.h>
#include <pch.h>
#include <util.h>
#include <version.h>

/* procedures */

static LINENUM locate_hunk PARAMS ((LINENUM, LINENUM));
static bool apply_hunk PARAMS ((bool *, LINENUM));
static bool copy_till PARAMS ((bool *, LINENUM));
static bool patch_match PARAMS ((LINENUM, LINENUM, LINENUM, LINENUM));
static bool similar PARAMS ((char const *, size_t, char const *, size_t));
static bool spew_output PARAMS ((bool *));
static char const *make_temp PARAMS ((int));
static int numeric_optarg PARAMS ((char const *));
static void abort_hunk PARAMS ((void));
static void cleanup PARAMS ((void));
static void get_some_switches PARAMS ((void));
static void init_output PARAMS ((char const *));
static void init_reject PARAMS ((char const *));
static void reinitialize_almost_everything PARAMS ((void));
static void usage PARAMS ((FILE *, int)) __attribute__((noreturn));

/* TRUE if -E was specified on command line.  */
static int remove_empty_files;

/* TRUE if -R was specified on command line.  */
static int reverse_flag_specified;

static bool backup;
static bool noreverse;

/* how many input lines have been irretractably output */
static LINENUM last_frozen_line;

static char const *do_defines; /* symbol to patch using ifdef, ifndef, etc. */
static char const if_defined[] = "\n#ifdef %s\n";
static char const not_defined[] = "#ifndef %s\n";
static char const else_defined[] = "\n#else\n";
static char const end_defined[] = "\n#endif /* %s */\n";

static int Argc;
static char * const *Argv;

static FILE *ofp;  /* output file pointer */
static FILE *rejfp;  /* reject file pointer */

static char *output;
static char const *patchname;
static char *rejname;
static char const * volatile TMPREJNAME;

static LINENUM last_offset;
static LINENUM maxfuzz = 2;

static char serrbuf[BUFSIZ];

char const program_name[] = "patch";

/* Apply a set of diffs as appropriate. */

int main PARAMS ((int, char **));

int
main(argc,argv)
int argc;
char **argv;
{
    bool somefailed = FALSE;

    setbuf(stderr, serrbuf);

    bufsize = 8 * 1024;
    buf = xmalloc (bufsize);

    strippath = INT_MAX;

    {
      char const *v;

      v = getenv ("SIMPLE_BACKUP_SUFFIX");
      if (v && *v)
	{
	  simple_backup_suffix = v;
	  backup = TRUE;
	}
      v = getenv ("VERSION_CONTROL");
      if (v && *v)
	backup = TRUE;
      backup_type = get_version (v); /* OK to pass NULL. */
    }

    /* parse switches */
    Argc = argc;
    Argv = argv;
    get_some_switches();

    /* Cons up the names of the global temporary files.  */
    TMPOUTNAME = make_temp ('o');
    TMPINNAME = make_temp ('i');
    TMPREJNAME = make_temp ('r');
    TMPPATNAME = make_temp ('p');

    if (output)
      init_output (output);

    /* Make sure we clean up in case of disaster.  */
    set_signals(0);

    for (
	open_patch_file (patchname);
	there_is_another_patch();
	reinitialize_almost_everything()
    ) {					/* for each patch in patch file */
      int hunk = 0;
      int failed = 0;
      char const *outname = output ? output : inname;

      if (!skip_rest_of_patch)
	get_input_file (inname, outname);

      if (diff_type == ED_DIFF) {
	if (! dry_run)
	  do_ed_script (ofp);
      } else {
	int got_hunk;
	bool rev_okayed = FALSE;
	bool after_newline = TRUE;

	/* initialize the patched file */
	if (!skip_rest_of_patch && !output)
	    init_output(TMPOUTNAME);

	/* initialize reject file */
	init_reject(TMPREJNAME);

	/* find out where all the lines are */
	if (!skip_rest_of_patch)
	    scan_input (inname);

	/* from here on, open no standard i/o files, because malloc */
	/* might misfire and we can't catch it easily */

	/* apply each hunk of patch */
	while (0 < (got_hunk = another_hunk (diff_type))) {
	    LINENUM where = 0; /* Pacify `gcc -Wall'.  */
	    LINENUM newwhere;
	    LINENUM fuzz = 0;
	    LINENUM max_prefix_fuzz = pch_prefix_context ();
	    LINENUM max_suffix_fuzz = pch_suffix_context ();
	    hunk++;
	    if (maxfuzz < max_prefix_fuzz)
		max_prefix_fuzz = maxfuzz;
	    if (maxfuzz < max_suffix_fuzz)
		max_suffix_fuzz = maxfuzz;
	    if (!skip_rest_of_patch) {
		do {
		    LINENUM prefix_fuzz =
			    fuzz < max_prefix_fuzz ? fuzz : max_prefix_fuzz;
		    LINENUM suffix_fuzz =
			    fuzz < max_suffix_fuzz ? fuzz : max_suffix_fuzz;
		    where = locate_hunk (prefix_fuzz, suffix_fuzz);
		    if (hunk == 1 && !where && !(force|rev_okayed)) {
						/* dwim for reversed patch? */
			if (!pch_swap()) {
			    if (!fuzz)
				say (
"Not enough memory to try swapped hunk!  Assuming unswapped.\n");
			    continue;
			}
			reverse = !reverse;
			/* Try again.  */
			where = locate_hunk (prefix_fuzz, suffix_fuzz);
			if (!where) {	    /* didn't find it swapped */
			    if (!pch_swap())	/* put it back to normal */
				fatal ("lost hunk on alloc error!");
			    reverse = !reverse;
			}
			else if (noreverse) {
			    if (!pch_swap())	/* put it back to normal */
				fatal ("lost hunk on alloc error!");
			    reverse = !reverse;
			    say (
"Ignoring previously applied (or reversed) patch.\n");
			    skip_rest_of_patch = TRUE;
			}
			else if (batch) {
			    if (verbosity != SILENT)
				say (
"%seversed (or previously applied) patch detected!  %s -R.\n",
				reverse ? "R" : "Unr",
				reverse ? "Assuming" : "Ignoring");
			}
			else {
			    ask (
"%seversed (or previously applied) patch detected!  %s -R? [y] ",
				reverse ? "R" : "Unr",
				reverse ? "Assume" : "Ignore");
			    if (*buf == 'n') {
				ask ("Apply anyway? [n] ");
				if (*buf == 'y')
				    rev_okayed = TRUE;
				else
				    skip_rest_of_patch = TRUE;
				where = 0;
				reverse = !reverse;
				if (!pch_swap())  /* put it back to normal */
				    fatal ("lost hunk on alloc error!");
			    }
			}
		    }
		} while (!skip_rest_of_patch && !where
			 && (++fuzz <= max_prefix_fuzz
			     || fuzz <= max_suffix_fuzz));

		if (skip_rest_of_patch) {		/* just got decided */
		  if (ofp && !output)
		    {
		      fclose (ofp);
		      ofp = 0;
		    }
		}
	    }

	    newwhere = pch_newfirst() + last_offset;
	    if (skip_rest_of_patch) {
		abort_hunk();
		failed++;
		if (verbosity != SILENT)
		    say ("Hunk #%d ignored at %ld.\n", hunk, newwhere);
	    }
	    else if (!where
		     || (where == 1 && ok_to_create_file && input_lines)) {
		if (where)
		  say ("\nPatch attempted to create file `%s', which already exists.\n", inname);
		abort_hunk();
		failed++;
		if (verbosity != SILENT)
		    say ("Hunk #%d FAILED at %ld.\n", hunk, newwhere);
	    }
	    else {
		if (! apply_hunk (&after_newline, where)) {
		    abort_hunk ();
		    failed++;
		    if (verbosity != SILENT)
			say ("Hunk #%d FAILED at %ld.\n", hunk, newwhere);
		} else if (verbosity == VERBOSE) {
		    say ("Hunk #%d succeeded at %ld", hunk, newwhere);
		    if (fuzz)
			say (" with fuzz %ld", fuzz);
		    if (last_offset)
			say (" (offset %ld line%s)",
			    last_offset, last_offset==1?"":"s");
		    say (".\n");
		}
	    }
	}

	if (got_hunk < 0  &&  using_plan_a) {
	    if (output)
	      fatal ("out of memory using Plan A");
	    say ("\n\nRan out of memory using Plan A--trying again...\n\n");
	    if (ofp)
	      {
		fclose (ofp);
		ofp = 0;
	      }
	    fclose (rejfp);
	    continue;
	}

	assert(hunk);

	/* finish spewing out the new file */
	if (!skip_rest_of_patch)
	  skip_rest_of_patch = ! spew_output (&after_newline);
      }

      /* and put the output where desired */
      ignore_signals ();
      if (!skip_rest_of_patch && !output) {
	  struct stat statbuf;

	  if (remove_empty_files
	      && stat (TMPOUTNAME, &statbuf) == 0
	      && statbuf.st_size == 0)
	    {
	      if (verbosity == VERBOSE)
		say ("Removing %s (empty after patching).\n", outname);
	      if (! dry_run)
		unlink (outname);
	    }
	  else
	    {
	      if (! dry_run)
		{
		  move_file (TMPOUTNAME, outname, backup);
		  chmod (outname, instat.st_mode);
		}
	    }
      }
      if (diff_type != ED_DIFF) {
	if (fclose (rejfp) != 0)
	    write_fatal ();
	if (failed) {
	    somefailed = TRUE;
	    say ("%d out of %d hunk%s %s", failed, hunk, "s" + (hunk == 1),
		 skip_rest_of_patch ? "ignored" : "FAILED");
	    if (outname) {
		char *rej = rejname;
		if (!rejname) {
		    rej = xmalloc (strlen (outname) + 5);
		    strcpy (rej, outname);
		    addext (rej, ".rej", '#');
		}
		say ("--saving rejects to %s", rej);
		if (! dry_run)
		    move_file (TMPREJNAME, rej, FALSE);
		if (!rejname)
		    free (rej);
	    }
	    say ("\n");
	}
      }
      set_signals (1);
    }
    if (ofp && (ferror (ofp) || fclose (ofp) != 0))
      write_fatal ();
    cleanup ();
    if (somefailed)
      exit (1);
    return 0;
}

/* Prepare to find the next patch to do in the patch file. */

static void
reinitialize_almost_everything()
{
    re_patch();
    re_input();

    input_lines = 0;
    last_frozen_line = 0;

    if (inname) {
	free (inname);
	inname = 0;
    }

    last_offset = 0;

    diff_type = NO_DIFF;

    if (revision) {
	free(revision);
	revision = 0;
    }

    reverse = reverse_flag_specified;
    skip_rest_of_patch = FALSE;
}

static char const shortopts[] = "bB:cd:D:eEfF:i:lnNo:p:r:RstuvV:x:Y:z:";
static struct option const longopts[] =
{
  {"backup", no_argument, NULL, 'b'},
  {"prefix", required_argument, NULL, 'B'},
  {"context", no_argument, NULL, 'c'},
  {"directory", required_argument, NULL, 'd'},
  {"ifdef", required_argument, NULL, 'D'},
  {"ed", no_argument, NULL, 'e'},
  {"remove-empty-files", no_argument, NULL, 'E'},
  {"force", no_argument, NULL, 'f'},
  {"fuzz", required_argument, NULL, 'F'},
  {"help", no_argument, NULL, 'h'},
  {"input", required_argument, NULL, 'i'},
  {"ignore-whitespace", no_argument, NULL, 'l'},
  {"normal", no_argument, NULL, 'n'},
  {"forward", no_argument, NULL, 'N'},
  {"output", required_argument, NULL, 'o'},
  {"strip", required_argument, NULL, 'p'},
  {"reject-file", required_argument, NULL, 'r'},
  {"reverse", no_argument, NULL, 'R'},
  {"quiet", no_argument, NULL, 's'},
  {"silent", no_argument, NULL, 's'},
  {"batch", no_argument, NULL, 't'},
  {"unified", no_argument, NULL, 'u'},
  {"version", no_argument, NULL, 'v'},
  {"version-control", required_argument, NULL, 'V'},
  {"debug", required_argument, NULL, 'x'},
  {"basename-prefix", required_argument, NULL, 'Y'},
  {"suffix", required_argument, NULL, 'z'},
  {"dry-run", no_argument, NULL, 129},
  {"verbose", no_argument, NULL, 130},
  {NULL, no_argument, NULL, 0}
};

static char const * const option_help[] = {
"Input options:",
"",
"  -p NUM  --strip=NUM  Strip NUM leading components from file names.",
"  -F LINES  --fuzz LINES  Set the fuzz factor to LINES for inexact matching.",
"  -l  --ignore-whitespace  Ignore white space changes between patch and input.",
"",
"  -c  --context  Interpret the patch as a context difference.",
"  -e  --ed  Interpret the patch as an ed script.",
"  -n  --normal  Interpret the patch as a normal difference.",
"  -u  --unified  Interpret the patch as a unified difference.",
"",
"  -N  --forward  Ignore patches that appear to be reversed or already applied.",
"  -R  --reverse  Assume patches were created with old and new files swapped.",
"",
"  -i PATCHFILE  --input=PATCHFILE  Read patch from PATCHFILE instead of stdin.",
"",
"Output options:",
"",
"  -o FILE  --output=FILE  Output patched files to FILE.",
"  -r FILE  --reject-file=FILE  Output rejects to FILE.",
"",
"  -D NAME  --ifdef=NAME  Make merged if-then-else output using NAME.",
"  -E  --remove-empty-files  Remove output files that are empty after patching.",
"",
"Backup file options:",
"",
"  -V STYLE  --version-control=STYLE  Use STYLE version control.",
"	STYLE is either 'simple', 'numbered', or 'existing'.",
"",
"  -b  --backup  Save the original contents of each file F into F.orig.",
"  -B PREFIX  --prefix=PREFIX  Prepend PREFIX to backup file names.",
"  -Y PREFIX  --basename-prefix=PREFIX  Prepend PREFIX to backup file basenames.",
"  -z SUFFIX  --suffix=SUFFIX  Append SUFFIX to backup file names.",
"",
"Miscellaneous options:",
"",
"  -t  --batch  Ask no questions; skip bad-Prereq patches; assume reversed.",
"  -f  --force  Like -t, but ignore bad-Prereq patches, and assume unreversed.",
"  -s  --quiet  --silent  Work silently unless an error occurs.",
"  --verbose  Output extra information about the work being done.",
"  --dry-run  Do not actually change any files; just print what would happen.",
"",
"  -d DIR  --directory=DIR  Change the working directory to DIR first.",
"",
"  -v  --version  Output version info.",
"  --help  Output this help.",
0
};

static void
usage (stream, status)
     FILE *stream;
     int status;
{
  char const * const *p;

  if (status != 0)
    {
      fprintf (stream, "%s: Try `%s --help' for more information.\n",
	       program_name, Argv[0]);
    }
  else
    {
      fprintf (stream, "Usage: %s [OPTION]... [ORIGFILE [PATCHFILE]]\n\n",
	       Argv[0]);
      for (p = option_help;  *p;  p++)
	fprintf (stream, "%s\n", *p);
    }

  exit (status);
}

/* Process switches and filenames.  */

static void
get_some_switches()
{
    register int optc;

    if (rejname)
	free (rejname);
    rejname = 0;
    if (optind == Argc)
	return;
    while ((optc = getopt_long (Argc, Argv, shortopts, longopts, (int *) 0))
	   != -1) {
	switch (optc) {
	    case 'b':
		 /* Special hack for backward compatibility with CVS 1.9.
		    If the last 4 args are `-b SUFFIX ORIGFILE PATCHFILE',
		    treat `-b' as if it were `-z'.  */
		if (Argc - optind == 3
		    && strcmp (Argv[optind - 1], "-b") == 0
		    && ! (Argv[optind + 0][0] == '-' && Argv[optind + 0][1])
		    && ! (Argv[optind + 1][0] == '-' && Argv[optind + 1][1])
		    && ! (Argv[optind + 2][0] == '-' && Argv[optind + 2][1]))
		  {
		    optarg = Argv[optind++];
		    if (verbosity != SILENT)
		      say ("warning: the `-b %s' option is obsolete; use `-z %s' instead\n",
			   optarg, optarg);
		    goto case_z;
		  }
		backup = TRUE;
		backup_type = simple;
		break;
	    case 'B':
		if (!*optarg)
		  pfatal ("backup prefix is empty");
		origprae = savestr (optarg);
		backup = TRUE;
		backup_type = simple;
		break;
	    case 'c':
		diff_type = CONTEXT_DIFF;
		break;
	    case 'd':
		if (chdir(optarg) < 0)
		    pfatal ("can't cd to %s", optarg);
		break;
	    case 'D':
		do_defines = savestr (optarg);
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
		maxfuzz = numeric_optarg ("fuzz factor");
		break;
	    case 'h':
		usage (stdout, 0);
	    case 'i':
		patchname = savestr (optarg);
		break;
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
		if (strcmp (optarg, "-") == 0)
		  fatal ("cannot output patches to standard output");
		output = savestr (optarg);
		break;
	    case 'p':
		strippath = numeric_optarg ("strip count");
		break;
	    case 'r':
		rejname = savestr (optarg);
		break;
	    case 'R':
		reverse = TRUE;
		reverse_flag_specified = TRUE;
		break;
	    case 's':
		verbosity = SILENT;
		break;
	    case 't':
		batch = TRUE;
		break;
	    case 'u':
		diff_type = UNI_DIFF;
		break;
	    case 'v':
		version();
		exit (0);
		break;
	    case 'V':
		backup = TRUE;
		backup_type = get_version (optarg);
		break;
#if DEBUGGING
	    case 'x':
		debug = numeric_optarg ("debugging option");
		break;
#endif
	    case 'Y':
		if (!*optarg)
		  pfatal ("backup basename prefix is empty");
		origbase = savestr (optarg);
		backup = TRUE;
		backup_type = simple;
		break;
	    case 'z':
	    case_z:
		if (!*optarg)
		  pfatal ("backup suffix is empty");
		simple_backup_suffix = savestr (optarg);
		backup = TRUE;
		backup_type = simple;
		break;
	    case 129:
		dry_run = TRUE;
		break;
	    case 130:
		verbosity = VERBOSE;
		break;
	    default:
		usage (stderr, 2);
	}
    }

    /* Process any filename args.  */
    if (optind < Argc)
      {
	inname = savestr (Argv[optind++]);
	if (optind < Argc)
	  {
	    patchname = savestr (Argv[optind++]);
	    if (optind < Argc)
	      {
		fprintf (stderr, "%s: extra operand `%s'\n",
			 program_name, Argv[optind]);
		usage (stderr, 2);
	      }
	  }
      }
}

/* Handle a numeric option of type ARGTYPE_MSGID by converting
   optarg to a nonnegative integer, returning the result.  */
static int
numeric_optarg (argtype_msgid)
     char const *argtype_msgid;
{
  int value = 0;
  char const *p = optarg;

  do
    {
      int v10 = value * 10;
      int digit = *p - '0';

      if (9 < (unsigned) digit)
	fatal ("%s `%s' is not a number", argtype_msgid, optarg);

      if (v10 / 10 != value || v10 + digit < v10)
	fatal ("%s `%s' is too large", argtype_msgid, optarg);

      value = v10 + digit;
    }
  while (*++p);

  return value;
}

/* Attempt to find the right place to apply this hunk of patch. */

static LINENUM
locate_hunk (prefix_fuzz, suffix_fuzz)
     LINENUM prefix_fuzz;
     LINENUM suffix_fuzz;
{
    register LINENUM first_guess = pch_first () + last_offset;
    register LINENUM offset;
    LINENUM pat_lines = pch_ptrn_lines();
    register LINENUM max_pos_offset
      = input_lines - first_guess - pat_lines + 1;
    register LINENUM max_neg_offset
      = first_guess - last_frozen_line - 1 + pch_prefix_context ();

    if (!pat_lines)			/* null range matches always */
	return first_guess;
    if (max_neg_offset >= first_guess)	/* do not try lines < 0 */
	max_neg_offset = first_guess - 1;
    if (first_guess <= input_lines
	&& patch_match (first_guess, (LINENUM) 0, prefix_fuzz, suffix_fuzz))
	return first_guess;
    for (offset = 1; ; offset++) {
	register bool check_after = offset <= max_pos_offset;
	register bool check_before = offset <= max_neg_offset;

	if (check_after
	    && patch_match (first_guess, offset, prefix_fuzz, suffix_fuzz)) {
	    if (debug & 1)
		say ("Offset changing from %ld to %ld\n", last_offset, offset);
	    last_offset = offset;
	    return first_guess+offset;
	}
	else if (check_before
		 && patch_match (first_guess, -offset,
				 prefix_fuzz, suffix_fuzz)) {
	    if (debug & 1)
		say ("Offset changing from %ld to %ld\n", last_offset, -offset);
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
    register LINENUM i;
    register LINENUM pat_end = pch_end ();
    /* add in last_offset to guess the same as the previous successful hunk */
    LINENUM oldfirst = pch_first() + last_offset;
    LINENUM newfirst = pch_newfirst() + last_offset;
    LINENUM oldlast = oldfirst + pch_ptrn_lines() - 1;
    LINENUM newlast = newfirst + pch_repl_lines() - 1;
    char const *stars =
      (int) NEW_CONTEXT_DIFF <= (int) diff_type ? " ****" : "";
    char const *minuses =
      (int) NEW_CONTEXT_DIFF <= (int) diff_type ? " ----" : " -----";

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
	    fprintf (rejfp, "%c ", pch_char (i));
	    /* fall into */
	case '\n':
	    pch_write_line (i, rejfp);
	    break;
	default:
	    fatal ("fatal internal error in abort_hunk");
	}
	if (ferror (rejfp))
	  write_fatal ();
    }
}

/* We found where to apply it (we hope), so do it. */

static bool
apply_hunk (after_newline, where)
bool *after_newline;
LINENUM where;
{
    register LINENUM old = 1;
    register LINENUM lastline = pch_ptrn_lines ();
    register LINENUM new = lastline+1;
    register enum {OUTSIDE, IN_IFNDEF, IN_IFDEF, IN_ELSE} def_state = OUTSIDE;
    register char const *R_do_defines = do_defines;
    register LINENUM pat_end = pch_end ();
    register FILE *fp = ofp;

    where--;
    while (pch_char(new) == '=' || pch_char(new) == '\n')
	new++;

    while (old <= lastline) {
	if (pch_char(old) == '-') {
	    assert (*after_newline);
	    if (! copy_till (after_newline, where + old - 1))
		return FALSE;
	    if (R_do_defines) {
		if (def_state == OUTSIDE) {
		    fprintf (fp, *after_newline + if_defined, R_do_defines);
		    def_state = IN_IFNDEF;
		}
		else if (def_state == IN_IFDEF) {
		    fprintf (fp, *after_newline + else_defined);
		    def_state = IN_ELSE;
		}
		if (ferror (fp))
		  write_fatal ();
		*after_newline = pch_write_line (old, fp);
	    }
	    last_frozen_line++;
	    old++;
	}
	else if (new > pat_end) {
	    break;
	}
	else if (pch_char(new) == '+') {
	    if (! copy_till (after_newline, where + old - 1))
		return FALSE;
	    if (R_do_defines) {
		if (def_state == IN_IFNDEF) {
		    fprintf (fp, *after_newline + else_defined);
		    def_state = IN_ELSE;
		}
		else if (def_state == OUTSIDE) {
		    fprintf (fp, *after_newline + if_defined, R_do_defines);
		    def_state = IN_IFDEF;
		}
		if (ferror (fp))
		  write_fatal ();
	    }
	    *after_newline = pch_write_line (new, fp);
	    new++;
	}
	else if (pch_char(new) != pch_char(old)) {
	    if (debug & 1)
	      say ("oldchar = '%c', newchar = '%c'\n",
		   pch_char (old), pch_char (new));
	    fatal ("Out-of-sync patch, lines %ld,%ld--mangled text or line numbers, maybe?",
		pch_hunk_beg() + old,
		pch_hunk_beg() + new);
	}
	else if (pch_char(new) == '!') {
	    assert (*after_newline);
	    if (! copy_till (after_newline, where + old - 1))
		return FALSE;
	    assert (*after_newline);
	    if (R_do_defines) {
	       fprintf (fp, not_defined, R_do_defines);
	       if (ferror (fp))
		write_fatal ();
	       def_state = IN_IFNDEF;
	    }

	    do
	      {
		if (R_do_defines) {
		    *after_newline = pch_write_line (old, fp);
		}
		last_frozen_line++;
		old++;
	      }
	    while (pch_char (old) == '!');

	    if (R_do_defines) {
		fprintf (fp, *after_newline + else_defined);
		if (ferror (fp))
		  write_fatal ();
		def_state = IN_ELSE;
	    }

	    do
	      {
		*after_newline = pch_write_line (new, fp);
		new++;
	      }
	    while (pch_char (new) == '!');
	}
	else {
	    assert(pch_char(new) == ' ');
	    old++;
	    new++;
	    if (R_do_defines && def_state != OUTSIDE) {
		fprintf (fp, *after_newline + end_defined, R_do_defines);
		if (ferror (fp))
		  write_fatal ();
		*after_newline = TRUE;
		def_state = OUTSIDE;
	    }
	}
    }
    if (new <= pat_end && pch_char(new) == '+') {
	if (! copy_till (after_newline, where + old - 1))
	    return FALSE;
	if (R_do_defines) {
	    if (def_state == OUTSIDE) {
		fprintf (fp, *after_newline + if_defined, R_do_defines);
		def_state = IN_IFDEF;
	    }
	    else if (def_state == IN_IFNDEF) {
		fprintf (fp, *after_newline + else_defined);
		def_state = IN_ELSE;
	    }
	    if (ferror (fp))
	      write_fatal ();
	}

	do
	  {
	    if (!*after_newline  &&  putc ('\n', fp) == EOF)
	      write_fatal ();
	    *after_newline = pch_write_line (new, fp);
	    new++;
	  }
	while (new <= pat_end && pch_char (new) == '+');
    }
    if (R_do_defines && def_state != OUTSIDE) {
	fprintf (fp, *after_newline + end_defined, R_do_defines);
	if (ferror (fp))
	  write_fatal ();
	*after_newline = TRUE;
    }
    return TRUE;
}

/* Open the new file. */

static void
init_output(name)
     char const *name;
{
    ofp = fopen(name, "w");
    if (! ofp)
	pfatal ("can't create %s", name);
}

/* Open a file to put hunks we can't locate. */

static void
init_reject(name)
     char const *name;
{
    rejfp = fopen(name, "w");
    if (!rejfp)
	pfatal ("can't create %s", name);
}

/* Copy input file to output, up to wherever hunk is to be applied. */

static bool
copy_till (after_newline, lastline)
     register bool *after_newline;
     register LINENUM lastline;
{
    register LINENUM R_last_frozen_line = last_frozen_line;
    register FILE *fp = ofp;
    register char const *s;
    size_t size;

    if (R_last_frozen_line > lastline)
      {
	say ("misordered hunks! output would be garbled\n");
	return FALSE;
      }
    while (R_last_frozen_line < lastline)
      {
	s = ifetch (++R_last_frozen_line, 0, &size);
	if (size)
	  {
	    if ((!*after_newline  &&  putc ('\n', fp) == EOF)
		|| ! fwrite (s, sizeof *s, size, fp))
	      write_fatal ();
	    *after_newline = s[size - 1] == '\n';
	  }
      }
    last_frozen_line = R_last_frozen_line;
    return TRUE;
}

/* Finish copying the input file to the output file. */

static bool
spew_output (after_newline)
     bool *after_newline;
{
    if (debug & 256)
      say ("il=%ld lfl=%ld\n", input_lines, last_frozen_line);

    if (last_frozen_line < input_lines)
      if (! copy_till (after_newline, input_lines))
	return FALSE;

    if (ofp && !output)
      {
	if (fclose (ofp) != 0)
	  write_fatal ();
	ofp = 0;
      }

    return TRUE;
}

/* Does the patch pattern match at line base+offset? */

static bool
patch_match (base, offset, prefix_fuzz, suffix_fuzz)
LINENUM base;
LINENUM offset;
LINENUM prefix_fuzz;
LINENUM suffix_fuzz;
{
    register LINENUM pline = 1 + prefix_fuzz;
    register LINENUM iline;
    register LINENUM pat_lines = pch_ptrn_lines () - suffix_fuzz;
    size_t size;
    register char const *p;

    for (iline=base+offset+prefix_fuzz; pline <= pat_lines; pline++,iline++) {
	p = ifetch (iline, offset >= 0, &size);
	if (canonicalize) {
	    if (!similar(p, size,
			 pfetch(pline),
			 pch_line_len(pline) ))
		return FALSE;
	}
	else if (size != pch_line_len (pline)
		 || memcmp (p, pfetch (pline), size) != 0)
	    return FALSE;
    }
    return TRUE;
}

/* Do two lines match with canonicalized white space? */

static bool
similar (a, alen, b, blen)
     register char const *a;
     register size_t alen;
     register char const *b;
     register size_t blen;
{
  /* Ignore presence or absence of trailing newlines.  */
  alen  -=  alen && a[alen - 1] == '\n';
  blen  -=  blen && b[blen - 1] == '\n';

  for (;;)
    {
      if (!blen || (*b == ' ' || *b == '\t'))
	{
	  while (blen && (*b == ' ' || *b == '\t'))
	    b++, blen--;
	  if (alen)
	    {
	      if (!(*a == ' ' || *a == '\t'))
		return FALSE;
	      do a++, alen--;
	      while (alen && (*a == ' ' || *a == '\t'));
	    }
	  if (!alen || !blen)
	    return alen == blen;
	}
      else if (!alen || *a++ != *b++)
	return FALSE;
      else
	alen--, blen--;
    }
}

/* Make a temporary file.  */

#if HAVE_MKTEMP
char *mktemp PARAMS ((char *));
#endif

static char const *
make_temp (letter)
     int letter;
{
  char *r;
#if HAVE_MKTEMP
  char const *tmpdir = getenv ("TMPDIR");
  if (!tmpdir)
    tmpdir = "/tmp";
  r = xmalloc (strlen (tmpdir) + 14);
  sprintf (r, "%s/patch%cXXXXXX", tmpdir, letter);
  mktemp (r);
  if (!*r)
    pfatal ("mktemp");
#else
  r = xmalloc (L_tmpnam);
  if (! (tmpnam (r) == r && *r))
    pfatal ("tmpnam");
#endif
  return r;
}

/* Fatal exit with cleanup. */

void
fatal_exit (sig)
     int sig;
{
  cleanup ();

  if (sig)
    exit_with_signal (sig);

  exit (2);
}

static void
cleanup ()
{
  unlink (TMPINNAME);
  unlink (TMPOUTNAME);
  unlink (TMPPATNAME);
  unlink (TMPREJNAME);
}

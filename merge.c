#define XTERN extern
#include <common.h>
#include <inp.h>
#include <pch.h>
#include <util.h>

static LINENUM count_context_lines (void);
static bool context_matches_file (LINENUM, LINENUM);

#define OFFSET LINENUM
#define EQUAL_IDX(x, y) (context_matches_file (x, y))
#include "bestmatch.h"

static LINENUM
locate_merge (LINENUM *matched)
{
    LINENUM first_guess = pch_first () + in_offset;
    LINENUM pat_lines = pch_ptrn_lines ();
    LINENUM max_where = input_lines - (pat_lines - pch_suffix_context ()) + 1;
    LINENUM min_where = last_frozen_line + 1;
    LINENUM max_pos_offset = max_where - first_guess;
    LINENUM max_neg_offset = first_guess - min_where;
    LINENUM max_offset = (max_pos_offset < max_neg_offset
			  ? max_neg_offset : max_pos_offset);
    LINENUM context_lines = count_context_lines ();
    LINENUM where = first_guess, max_matched = 0;
    LINENUM min, max;
    LINENUM offset;

    /* - Allow at most MAX changes so that no more than FUZZ_LINES lines
	 may change (insert + delete).
       - Require the remaining lines to match.  */
    if (context_lines == 0)
      goto out;
    max = 2 * context_lines;
    min = pat_lines - context_lines;

    if (debug & 1)
      {
	char numbuf0[LINENUM_LENGTH_BOUND + 1];
	char numbuf1[LINENUM_LENGTH_BOUND + 1];
	say ("locating merge: min=%s max=%s ",
	     format_linenum (numbuf0, min),
	     format_linenum (numbuf1, max));
      }

    /* Do not try lines <= 0.  */
    if (first_guess <= max_neg_offset)
      max_neg_offset = first_guess - 1;

    for (offset = 0; offset <= max_offset; offset++)
      {
	if (offset <= max_pos_offset)
	  {
	    LINENUM guess = first_guess + offset;
	    LINENUM last;
	    LINENUM changes;

	    changes = bestmatch (1, pat_lines + 1, guess, input_lines + 1,
				 min, max, &last);
	    if (changes <= max && max_matched < last - guess)
	      {
		max_matched = last - guess;
		where = guess;
		if (changes == 0)
		  break;
		min = last - guess;
		max = changes - 1;
	      }
	  }
	if (0 < offset && offset <= max_neg_offset)
	  {
	    LINENUM guess = first_guess - offset;
	    LINENUM last;
	    LINENUM changes;

	    changes = bestmatch (1, pat_lines + 1, guess, input_lines + 1,
				 min, max, &last);
	    if (changes <= max && max_matched < last - guess)
	      {
		max_matched = last - guess;
		where = guess;
		if (changes == 0)
		  break;
		min = last - guess;
		max = changes - 1;
	      }
	  }
      }
    if (debug & 1)
      {
	char numbuf0[LINENUM_LENGTH_BOUND + 1];
	char numbuf1[LINENUM_LENGTH_BOUND + 1];
	say ("where=%s matched=%s\n",
	     format_linenum (numbuf0, where),
	     format_linenum (numbuf1, max_matched));
      }

  out:
    *matched = max_matched;
    return where;
}

static void
merge_result (int hunk, char const *what, LINENUM from, LINENUM to)
{
  char numbuf0[LINENUM_LENGTH_BOUND + 1];
  char numbuf1[LINENUM_LENGTH_BOUND + 1];

  if (verbosity != SILENT)
    {
      printf ("Hunk #%d %s at ", hunk, what);
      if (to <= from)
	printf ("%s",
		format_linenum (numbuf0, from));
      else
	printf ("%s-%s",
		format_linenum (numbuf0, from),
		format_linenum (numbuf1, to));
      if (in_offset != 0)
	printf (" (offset %s lines)",
		format_linenum (numbuf0, in_offset));
      fputs (".\n", stdout);
      fflush (stdout);
    }
}

bool
merge_hunk (int hunk, struct outstate *outstate, LINENUM first_where,
	    bool *somefailed)
{
  LINENUM found_where;
  LINENUM where;
  LINENUM lastwhere;
  LINENUM new = pch_ptrn_lines () + 1;
  LINENUM lastnew = pch_end ();
  FILE *fp = outstate->ofp;
  LINENUM lines;

  while (pch_char (new) == '=' || pch_char (new) == '\n')
    new++;

  if (first_where)
    {
      where = found_where = first_where;
      lastwhere = where + pch_ptrn_lines () - 1;
    }
  else
    {
      where = found_where = locate_merge (&lastwhere);
      lastwhere += where - 1;
    }

  /* Hide common prefix context */
  while (new <= lastnew && where <= lastwhere &&
	 context_matches_file (new, where))
    {
      new++;
      where++;
    }

  /* Hide common suffix context */
  while (new <= lastnew && where <= lastwhere &&
	 context_matches_file (lastnew, lastwhere))
    {
      lastnew--;
      lastwhere--;
    }

  /* Has this hunk been applied already? */
  if (lastwhere < where && lastnew < new)
    {
      where = found_where + pch_prefix_context ();
      lastwhere = found_where + pch_repl_lines() - pch_suffix_context() - 1;
      merge_result (hunk, "already applied",
		    where + out_offset,
		    lastwhere + out_offset);
      return true;
    }

  if (first_where)
    {
      LINENUM offs = out_offset;
      if (! apply_hunk (outstate, first_where))
	return false;
      merge_result (hunk, "merged",
		    where + offs,
		    where + (lastnew - new) + offs);
      return true;
    }

  lines = 3 + (lastwhere - where + 1) + (lastnew - new + 1);
  assert (outstate->after_newline);
  if (where >= last_frozen_line
      && ! copy_till (outstate, where - 1))
    return false;
  outstate->zero_output = false;
  fputs (outstate->after_newline + "\n<<<<<<<\n", fp);
  outstate->after_newline = true;
  if (lastwhere >= last_frozen_line
      && ! copy_till (outstate, lastwhere))
    return false;
  fputs (outstate->after_newline + "\n=======\n", fp);
  outstate->after_newline = true;
  while (new <= lastnew)
    {
      outstate->after_newline = pch_write_line (new, fp);
      new++;
    }
  fputs (outstate->after_newline + "\n>>>>>>>\n", fp);
  outstate->after_newline = true;
  if (ferror (fp))
    write_fatal ();

  merge_result (hunk, "UNMERGED",
		where + out_offset,
		where + out_offset + lines - 1);
  out_offset += 3 + (lastnew - new + 1);
  *somefailed = true;

  return true;
}

static LINENUM
count_context_lines (void)
{
  LINENUM old;
  LINENUM lastold = pch_ptrn_lines ();
  LINENUM context;

  for (context = 0, old = 1; old <= lastold; old++)
    if (pch_char (old) == ' ')
      context++;
  return context;
}

static bool
context_matches_file (LINENUM old, LINENUM where)
{
  size_t size;
  const char *line;

  line = ifetch (where, false, &size);
  return size &&
	 (canonicalize ?
	  similar (pfetch (old), pch_line_len (old), line, size) :
	  (size == pch_line_len (old) &&
	   memcmp (line, pfetch (old), size) == 0));
}

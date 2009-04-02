#define XTERN extern
#include <common.h>
#include <minmax.h>
#include <xalloc.h>
#include <inp.h>
#include <pch.h>
#include <util.h>

static LINENUM count_context_lines (void);
static bool context_matches_file (LINENUM, LINENUM);
static void compute_changes (LINENUM, LINENUM, LINENUM, LINENUM, char *,
			     char *);

#define OFFSET LINENUM
#define EQUAL_IDX(x, y) (context_matches_file (x, y))
#include "bestmatch.h"

#define EQUAL_IDX(ctxt, x, y) (context_matches_file (x, y))
#define OFFSET LINENUM
#define EXTRA_CONTEXT_FIELDS \
	char *xchar; \
	char *ychar;
#define NOTE_DELETE(ctxt, xoff) ctxt->xchar[xoff] = '-';
#define NOTE_INSERT(ctxt, yoff) ctxt->ychar[yoff] = '+';
#define USE_HEURISTIC 1
#include "diffseq.h"

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
merge_result (bool *first_result, char const *what, LINENUM from, LINENUM to)
{
  if (verbosity != SILENT)
    {
      static char const *last_what;
      char numbuf0[LINENUM_LENGTH_BOUND + 1];
      char numbuf1[LINENUM_LENGTH_BOUND + 1];

      if (*first_result)
	{
	  last_what = what;
	  printf ("%s at ", what);
	}
      else if (last_what == what)
	fputs (",", stdout);
      else
	printf (", %s at ", what);
      *first_result = false;

      if (to <= from)
	printf ("%s",
		format_linenum (numbuf0, from + out_offset));
      else
	printf ("%s-%s",
		format_linenum (numbuf0, from + out_offset),
		format_linenum (numbuf1, to + out_offset));
      if (in_offset != 0)
	printf (" (offset %s lines)",
		format_linenum (numbuf0, in_offset));
    }
}

bool
merge_hunk (int hunk, struct outstate *outstate, LINENUM where,
	    bool *somefailed)
{
  bool first_result = true;
  FILE *fp = outstate->ofp;
  LINENUM old = 1;
  LINENUM firstold = pch_ptrn_lines ();
  LINENUM new = firstold + 1;
  LINENUM firstnew = pch_end ();
  LINENUM in;
  LINENUM firstin;
  char *oldin;
  LINENUM matched;
  LINENUM lastwhere;

  /* Convert '!' markers into '-' and '+' to simplify things here.  */
  pch_normalize (UNI_DIFF);

  assert (pch_char (firstnew + 1) == '^');
  while (pch_char (new) == '=' || pch_char (new) == '\n')
    new++;

  if (where)
    matched = pch_ptrn_lines ();
  else
    where = locate_merge (&matched);

  in = firstold + 2;
  oldin = xmalloc (in + matched + 1);
  memset (oldin, ' ', in + matched);
  oldin[0] = '*';
  oldin[in - 1] = '=';
  oldin[in + matched] = '^';
  compute_changes (old, in - 1, where, where + matched,
		   oldin + old, oldin + in);

  if (debug & 2)
    {
      char numbuf0[LINENUM_LENGTH_BOUND + 1];
      LINENUM n;

      fputc ('\n', stderr);
      for (n = 0; n <= in + matched; n++)
	{
	  fprintf (stderr, "%s %c",
		  format_linenum (numbuf0, n),
		  oldin[n]);
	  if (n > 0 && n <= firstold)
	    fprintf (stderr, " |%.*s",
		     (int) pch_line_len (n), pfetch (n));
	  else if (n >= in && n < in + matched)
	    {
	      size_t size;
	      const char *line;

	      line = ifetch (where + n - in, false, &size);
	      fprintf (stderr, " |%.*s",
		       (int) size, line);
	    }
	  else
	    fputc('\n', stderr);
	}
      fflush (stderr);
    }

  if (last_frozen_line < where - 1)
    if (! copy_till (outstate, where - 1))
      return false;

  if (verbosity != SILENT)
    printf("Hunk #%d ", hunk);

  for (;;)
    {
      firstold = old;
      firstnew = new;
      firstin = in;

      if (pch_char (old) == '-' || pch_char (new) == '+')
	{
	  LINENUM lines;

	  while (pch_char (old) == '-')
	    {
	      if (oldin[old] == '-')
		goto conflict;
	      else if (oldin[old] == ' ')
		{
		  assert (oldin[in] == ' ');
		  in++;
		}
	      old++;
	    }
	  if (oldin[old] == '-' || oldin[in] == '+')
	    goto conflict;
	  while (pch_char (new) == '+')
	    new++;

	  lines = new - firstnew;
	  merge_result (&first_result, "merged", where, where + lines - 1);
	  last_frozen_line += (old - firstold);
	  where += (old - firstold);
	  out_offset += new - firstnew;

	  while (firstnew < new)
	    {
	      outstate->after_newline = pch_write_line (firstnew, fp);
	      firstnew++;
	    }
	}
      else if (pch_char (old) == ' ')
	{
	  if (oldin[old] == '-')
	    {
	      while (pch_char (old) == ' ')
		{
		  if (oldin[old] != '-')
		    break;
		  assert (pch_char (new) == ' ');
		  old++;
		  new++;
		}
	      if (pch_char (old) == '-' || pch_char (new) == '+')
		goto conflict;
	    }
	  else if (oldin[in] == '+')
	    {
	      while (oldin[in] == '+')
		in++;

	      /* Take these lines from the input file.  */
	      where += in - firstin;
	      if (! copy_till (outstate, where - 1))
		return false;
	    }
	  else if (oldin[old] == ' ')
	    {
	      while (pch_char (old) == ' '
		     && oldin[old] == ' '
		     && pch_char (new) == ' '
		     && oldin[in] == ' ')
		{
		  old++;
		  new++;
		  in++;
		}

	      /* Take these lines from the input file.  */
	      where += (in - firstin);
	      if (! copy_till (outstate, where - 1))
		return false;
	    }
	}
      else
	{
	  assert (pch_char (old) == '=' && pch_char (new) == '^');
	  /* Nothing more left to merge.  */
	  break;
	}
      continue;

    conflict:
      /* Find the end of the conflict.  */
      for (;;)
	{
	  if (pch_char (old) == '-')
	    {
	      while (oldin[in] == '+')
		in++;
	      if (oldin[old] == ' ')
		{
		  assert (oldin[in] == ' ');
		  in++;
		}
	      old++;
	    }
	  else if (oldin[old] == '-')
	    {
	      while (pch_char (new) == '+')
		new++;
	      if (pch_char (old) == ' ')
		{
		  assert (pch_char (new) == ' ');
		  new++;
		}
	      old++;
	    }
	  else if (pch_char (new) == '+')
	    while (pch_char (new) == '+')
	      new++;
	  else if (oldin[in] == '+')
	    while (oldin[in] == '+')
	      in++;
	  else
	    break;
	}
      assert (((pch_char (old) == ' ' && pch_char (new) == ' ')
	       || (pch_char (old) == '=' && pch_char (new) == '^'))
	      && ((oldin[old] == ' ' && oldin[in] == ' ')
		  || (oldin[old] == '=' && oldin[in] == '^')));

      /* Output common prefix lines.  */
      for (lastwhere = where;
	   firstin < in && firstnew < new
	     && context_matches_file (firstnew, lastwhere);
	   firstin++, firstnew++, lastwhere++)
	continue;
      if (firstin == in && firstnew == new)
	merge_result (&first_result, "already applied", where, lastwhere - 1);
      if (where != lastwhere)
	{
	  where = lastwhere;
	  if (! copy_till (outstate, where - 1))
	    return false;
	}

      if (firstin < in || firstnew < new)
	{
	  LINENUM common_suffix;
	  LINENUM lines;

	  /* Remember common suffix lines.  */
	  for (common_suffix = 0,
	       lastwhere = where + (in - firstin);
	       firstin < in && firstnew < new
		 && context_matches_file (new - 1, lastwhere - 1);
	       in--, new--, lastwhere--, common_suffix++)
	    continue;

	  lines = 3 + (in - firstin) + (new - firstnew);
	  merge_result (&first_result, "UNMERGED", where,
			where + lines - 1);
	  out_offset += 3 + (new - firstnew);

	  fputs (outstate->after_newline + "\n<<<<<<<\n", fp);
	  outstate->after_newline = true;
	  if (firstin < in)
	    {
	      where += (in - firstin);
	      if (! copy_till (outstate, where - 1))
		return false;
	    }
	  fputs (outstate->after_newline + "\n=======\n", fp);
	  outstate->after_newline = true;
	  while (firstnew < new)
	    {
	      outstate->after_newline = pch_write_line (firstnew, fp);
	      firstnew++;
	    }
	  fputs (outstate->after_newline + "\n>>>>>>>\n", fp);
	  outstate->after_newline = true;
	  outstate->zero_output = false;
	  if (ferror (fp))
	    write_fatal ();

	  /* Output common suffix lines.  */
	  if (common_suffix)
	    {
	      where += common_suffix;
	      if (! copy_till (outstate, where - 1))
		return false;
	      in += common_suffix;
	      new += common_suffix;
	    }
	  *somefailed = true;
	}
    }
  if (verbosity != SILENT)
    {
      fputs (".\n", stdout);
      fflush (stdout);
    }

  assert (last_frozen_line == where - 1);
  free (oldin);
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

static void
compute_changes (LINENUM xmin, LINENUM xmax, LINENUM ymin, LINENUM ymax,
		 char *xchar, char *ychar)
{
  struct context ctxt;
  LINENUM diags;

  ctxt.xchar = xchar - xmin;
  ctxt.ychar = ychar - ymin;

  diags = xmax + ymax + 3;
  ctxt.fdiag = xmalloc (2 * diags * sizeof (*ctxt.fdiag));
  ctxt.bdiag = ctxt.fdiag + diags;
  ctxt.fdiag += ymax + 1;
  ctxt.bdiag += ymax + 1;

  ctxt.heuristic = true;
  ctxt.too_expensive = xmax + ymax;

  compareseq (xmin, xmax, ymin, ymax, true, &ctxt);

  ctxt.fdiag -= ymax + 1;
  free (ctxt.fdiag);
}

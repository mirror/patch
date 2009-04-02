#define XTERN extern
#include <common.h>
#include <inp.h>
#include <pch.h>
#include <util.h>

static bool context_matches_file (LINENUM, LINENUM);

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
      /* Just guessing ...  */
      where = found_where = pch_first () + in_offset;
      if (context_matches_file (new, where))
	{
	  lastwhere = where + pch_ptrn_lines () - 1;
	  if (! context_matches_file (lastnew, lastwhere))
	    {
	      lastwhere = where + pch_repl_lines () - 1;
	      if (! context_matches_file (lastnew, lastwhere))
		lastwhere = where - 1;
	    }
	}
      else
	lastwhere = where - 1;
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

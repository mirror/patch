static mode_t p_mode[2];		/* file modes */
	    diff_type == GIT_BINARY_DIFF ? "a git binary diff" :
	    if (lstat (inname, &instat) == 0)
static mode_t
fetchmode (char const *str)
{
   const char *s;
   mode_t mode;

   while (ISSPACE ((unsigned char) *str))
     str++;

   for (s = str, mode = 0; s < str + 6; s++)
     {
       if (*s >= '0' && *s <= '7')
	mode = (mode << 3) + (*s - '0');
       else
	{
	 mode = 0;
	 break;
	}
     }
   if (*s == '\r')
     s++;
   if (*s != '\n')
     mode = 0;

   return mode;
}

    file_offset this_line = 0;
    file_offset first_command_line = -1;
    bool this_is_a_command = false;
    bool stars_this_line = false;
    bool git_diff = false;
    bool extended_headers = false;
    enum diff retval;
    for (i = OLD; i <= NEW; i++)
      p_mode[i] = 0;
	char *s;
	char *t;
	file_offset previous_line = this_line;
	bool last_line_was_command = this_is_a_command;
	bool stars_last_line = stars_this_line;
	size_t indent = 0;
		if (extended_headers)
		  {
		    /* Patch contains no hunks; any diff type will do. */
		    retval = UNI_DIFF;
		    goto scan_exit;
		  }
	    free (p_name[OLD]);
	    free (p_name[OLD]);
	    free (p_name[INDEX]);
	else if (strnEQ (s, "diff --git ", 11))
	  {
	    char const *u;

	    if (extended_headers)
	      {
		p_start = this_line;
		p_sline = p_input_line;
		/* Patch contains no hunks; any diff type will do. */
		retval = UNI_DIFF;
		goto scan_exit;
	      }

	    if (! ((free (p_name[OLD]),
		    (p_name[OLD] = parse_name (s + 11, strippath, &u)))
		   && ISSPACE (*u)
		   && (free (p_name[NEW]),
		       (p_name[NEW] = parse_name (u, strippath, &u)))
		   && (u = skip_spaces (u), ! *u)))
	      for (i = OLD; i <= NEW; i++)
		{
		  free (p_name[i]);
		  p_name[i] = 0;
		}
	    git_diff = true;
	  }
	else if (git_diff && strnEQ (s, "index ", 6))
	  {
	    char const *u;

	    for (u = s + 6;  *u && ! ISSPACE ((unsigned char) *u);  u++)
	      /* do nothing */ ;
	    if (*(u = skip_spaces (u)))
	      p_mode[OLD] = p_mode[NEW] = fetchmode (u);
	    extended_headers = true;
	  }
	else if (git_diff && strnEQ (s, "old mode ", 9))
	  {
	    p_mode[OLD] = fetchmode (s + 9);
	    extended_headers = true;
	  }
	else if (git_diff && strnEQ (s, "new mode ", 9))
	  {
	    p_mode[NEW] = fetchmode (s + 9);
	    extended_headers = true;
	  }
	else if (git_diff && strnEQ (s, "deleted file mode ", 18))
	  {
	    p_mode[OLD] = fetchmode (s + 18);
	    p_says_nonexistent[NEW] = 2;
	    extended_headers = true;
	  }
	else if (git_diff && strnEQ (s, "new file mode ", 14))
	  {
	    p_mode[NEW] = fetchmode (s + 14);
	    p_says_nonexistent[OLD] = 2;
	    extended_headers = true;
	  }
	else if (git_diff && strnEQ (s, "GIT binary patch", 16))
	  {
	    p_start = this_line;
	    p_sline = p_input_line;
	    retval = GIT_BINARY_DIFF;
	    goto scan_exit;
	  }
		free (p_name[NEW]);
	      else if (lstat (p_name[i], &st[i]) != 0)
		&& (i == NONE || S_ISREG (st[i].st_mode))
	    inerrno = lstat (inname, &instat) == 0 ? 0 : errno;
	    if (inerrno || S_ISREG (instat.st_mode))
	      maybe_reverse (inname, inerrno, inerrno || instat.st_size == 0);
	      stat_result = lstat (filename, &stat_buf);
    FILE *i = pfp;
    FILE *o = stdout;
    int c;
    char *s;
    lin context = 0;
    size_t chars_read;
	lin fillcnt = 0;	/* #lines of missing ptrn or repl */
	lin fillsrc;		/* index of first line to copy */
	lin filldst;		/* index of first missing line */
	bool repl_could_be_missing = true;
	lin fillsrc;  /* index of old lines */
	lin filldst;  /* index of new lines */
	int i;
  FILE *fp = pfp;
  int c;
  size_t i;
  char *b;
  size_t s;
  FILE *fp = pfp;
  int c;
  file_offset line_beginning = file_tell (fp);
    lin i;
    lin n;
    char *s;
mode_t
pch_mode (bool which)
{
  return p_mode[which];
}

    file_offset beginning_of_this_line;
    FILE *pipefp = 0;
    size_t chars_read;
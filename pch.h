/* $Header: /home/agruen/git/patch-h/cvsroot/patch/pch.h,v 1.4 1993/07/29 20:11:38 eggert Exp $
 *
 * $Log: pch.h,v $
 * Revision 1.4  1993/07/29 20:11:38  eggert
 * (pfp, grow_hunkmax, intuit_diff_type, next_intuit_at, skip_to,
 * pfetch, pgetline): No longer exported.
 * (pch_write_line): New declaration.
 * (getline): Removed.
 * n
 *
 * Revision 1.4  1993/07/29 20:11:38  eggert
 * (pfp, grow_hunkmax, intuit_diff_type, next_intuit_at, skip_to,
 * pfetch, pgetline): No longer exported.
 * (pch_write_line): New declaration.
 * (getline): Removed.
 * n
 *
 * Revision 2.0.1.1  87/01/30  22:47:16  lwall
 * Added do_ed_script().
 * 
 * Revision 2.0  86/09/17  15:39:57  lwall
 * Baseline for netwide release.
 * 
 */

void re_patch PARAMS((void));
void open_patch_file PARAMS((char *));
void set_hunkmax PARAMS((void));
bool there_is_another_patch PARAMS((void));
bool another_hunk PARAMS((void));
bool pch_swap PARAMS((void));
char *pfetch PARAMS((LINENUM));
void pch_write_line PARAMS((LINENUM, FILE *));
size_t pch_line_len PARAMS((LINENUM));
LINENUM pch_first PARAMS((void));
LINENUM pch_ptrn_lines PARAMS((void));
LINENUM pch_newfirst PARAMS((void));
LINENUM pch_repl_lines PARAMS((void));
LINENUM pch_end PARAMS((void));
LINENUM pch_context PARAMS((void));
LINENUM pch_hunk_beg PARAMS((void));
char pch_char PARAMS((LINENUM));
void do_ed_script PARAMS((void));

/* $Header: /home/agruen/git/patch-h/cvsroot/patch/pch.h,v 1.3 1993/07/22 19:21:29 djm Exp $
 *
 * $Log: pch.h,v $
 * Revision 1.3  1993/07/22 19:21:29  djm
 * entered into RCS
 *
 * Revision 1.3  1993/07/22 19:21:29  djm
 * entered into RCS
 *
 * Revision 2.0.1.1  87/01/30  22:47:16  lwall
 * Added do_ed_script().
 * 
 * Revision 2.0  86/09/17  15:39:57  lwall
 * Baseline for netwide release.
 * 
 */

EXT FILE *pfp INIT(Nullfp);		/* patch file pointer */

void re_patch PARAMS((void));
void open_patch_file PARAMS((char *));
void set_hunkmax PARAMS((void));
void grow_hunkmax PARAMS((void));
bool there_is_another_patch PARAMS((void));
int intuit_diff_type PARAMS((void));
void next_intuit_at PARAMS((long, long));
void skip_to PARAMS((long, long));
bool another_hunk PARAMS((void));
bool pch_swap PARAMS((void));
char *pfetch PARAMS((LINENUM));
short pch_line_len PARAMS((LINENUM));
LINENUM pch_first PARAMS((void));
LINENUM pch_ptrn_lines PARAMS((void));
LINENUM pch_newfirst PARAMS((void));
LINENUM pch_repl_lines PARAMS((void));
LINENUM pch_end PARAMS((void));
LINENUM pch_context PARAMS((void));
LINENUM pch_hunk_beg PARAMS((void));
char pch_char PARAMS((LINENUM));
char *pfetch PARAMS((LINENUM));
int getline PARAMS((char **, int *, FILE *));
int pgetline PARAMS((char **, int *, FILE *));
void do_ed_script PARAMS((void));

/* $Header: /home/agruen/git/patch-h/cvsroot/patch/inp.h,v 1.2 1993/07/22 19:17:29 djm Exp $
 *
 * $Log: inp.h,v $
 * Revision 1.2  1993/07/22 19:17:29  djm
 * entered into RCS
 *
 * Revision 1.2  1993/07/22 19:17:29  djm
 * entered into RCS
 *
 * Revision 2.0  86/09/17  15:37:25  lwall
 * Baseline for netwide release.
 * 
 */

EXT LINENUM input_lines INIT(0);	/* how long is input file in lines */
EXT LINENUM last_frozen_line INIT(0);	/* how many input lines have been */
					/* irretractibly output */

bool rev_in_string PARAMS((char *));
void scan_input PARAMS((char *));
bool plan_a PARAMS((char *));	/* returns false if insufficient memory */
void plan_b PARAMS((char *));
char *ifetch PARAMS((LINENUM, int));


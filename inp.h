/* $Header: /home/agruen/git/patch-h/cvsroot/patch/inp.h,v 1.3 1993/07/29 20:11:38 eggert Exp $
 *
 * $Log: inp.h,v $
 * Revision 1.3  1993/07/29 20:11:38  eggert
 * (rev_in_string): Remove.
 * (ifetch): Yield size of line too, since strlen no longer applies.
 * (plan_a, plan_b): No longer exported.
 *
 * Revision 1.3  1993/07/29 20:11:38  eggert
 * (rev_in_string): Remove.
 * (ifetch): Yield size of line too, since strlen no longer applies.
 * (plan_a, plan_b): No longer exported.
 *
 * Revision 2.0  86/09/17  15:37:25  lwall
 * Baseline for netwide release.
 * 
 */

EXT LINENUM input_lines;		/* how long is input file in lines */
EXT LINENUM last_frozen_line;		/* how many input lines have been */
					/* irretractibly output */

char const *ifetch PARAMS((LINENUM, int, size_t *));
void re_input PARAMS((void));
void scan_input PARAMS((char *));

/* $Header: /home/agruen/git/patch-h/cvsroot/patch/version.c,v 1.2 1993/06/11 04:25:56 eggert Exp $
 *
 * $Log: version.c,v $
 * Revision 1.2  1993/06/11 04:25:56  eggert
 * entered into RCS
 *
 * Revision 1.2  1993/06/11 04:25:56  eggert
 * entered into RCS
 *
 * Revision 2.0  86/09/17  15:40:11  lwall
 * Baseline for netwide release.
 * 
 */

#include "EXTERN.h"
#include "common.h"
#include "util.h"
#include "INTERN.h"
#include "patchlevel.h"
#include "version.h"

void my_exit();

/* Print out the version number and die. */

void
version()
{
    fprintf(stderr, "Patch version %s\n", PATCH_VERSION);
    my_exit(0);
}

/* $Header: /home/agruen/git/patch-h/cvsroot/patch/version.c,v 1.3 1993/07/29 20:11:38 eggert Exp $
 *
 * $Log: version.c,v $
 * Revision 1.3  1993/07/29 20:11:38  eggert
 * Don't exit.
 *
 * Revision 1.3  1993/07/29 20:11:38  eggert
 * Don't exit.
 *
 * Revision 2.0  86/09/17  15:40:11  lwall
 * Baseline for netwide release.
 * 
 */

#include "EXTERN.h"
#include "common.h"
#include "INTERN.h"
#include "patchlevel.h"
#include "version.h"

/* Print the version number.  */

void
version()
{
    fprintf(stderr, "Patch version %s\n", PATCH_VERSION);
}

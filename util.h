/* $Header: /home/agruen/git/patch-h/cvsroot/patch/util.h,v 1.4 1993/07/29 20:11:38 eggert Exp $
 *
 * $Log: util.h,v $
 * Revision 1.4  1993/07/29 20:11:38  eggert
 * (say*, fatal*, pfatal*, ask*): Delete; these
 * pseudo-varargs functions weren't ANSI C.  Replace by macros
 * that invoke [fs]printf directly, and invoke new functions
 * [az]{say,fatal,pfatal,ask} before and after.
 * (savebuf, read_fatal, write_fatal, memory_fatal, Fseek): New functions.
 * (fatal*): Output trailing newline after message.  All invokers changed.
 *
 * Revision 1.4  1993/07/29 20:11:38  eggert
 * (say*, fatal*, pfatal*, ask*): Delete; these
 * pseudo-varargs functions weren't ANSI C.  Replace by macros
 * that invoke [fs]printf directly, and invoke new functions
 * [az]{say,fatal,pfatal,ask} before and after.
 * (savebuf, read_fatal, write_fatal, memory_fatal, Fseek): New functions.
 * (fatal*): Output trailing newline after message.  All invokers changed.
 *
 * Revision 2.0  86/09/17  15:40:06  lwall
 * Baseline for netwide release.
 * 
 */

#define say1(a) (fprintf (stderr, a), Fflush (stderr))
#define say2(a,b) (fprintf (stderr, a,b), Fflush (stderr))
#define say3(a,b,c) (fprintf (stderr, a,b,c), Fflush (stderr))
#define say4(a,b,c,d) (fprintf (stderr, a,b,c,d), Fflush (stderr))
#define fatal1(a) (fprintf (afatal (), a), zfatal ())
#define fatal2(a,b) (fprintf (afatal (), a,b), zfatal ())
#define fatal3(a,b,c) (fprintf (afatal (), a,b,c), zfatal ())
#define fatal4(a,b,c,d) (fprintf (afatal (), a,b,c,d), zfatal ())
#define pfatal1(a) (fprintf (apfatal (), a), zpfatal ())
#define pfatal2(a,b) (fprintf (apfatal (), a,b), zpfatal ())
#define pfatal3(a,b,c) (fprintf (apfatal (), a,b,c), zpfatal ())
#define pfatal4(a,b,c,d) (fprintf (apfatal (), a,b,c,d), zpfatal ())
#define ask1(a) (Sprintf (buf, a), zask())
#define ask2(a,b) (Sprintf (buf, a,b), zask())
#define ask3(a,b,c) (Sprintf (buf, a,b,c), zask())
#define ask4(a,b,c,d) (Sprintf (buf, a,b,c,d), zask())

char *fetchname PARAMS((char *, int, int));
int move_file PARAMS((char *, char *));
void copy_file PARAMS((char *, char *));
char *savebuf PARAMS((char *, size_t));
char *savestr PARAMS((char *));
FILE *afatal PARAMS((void));
EXITING void zfatal PARAMS((void));
void memory_fatal PARAMS((void));
void read_fatal PARAMS((void));
void write_fatal PARAMS((void));
FILE *apfatal PARAMS((void));
EXITING void zpfatal PARAMS((void));
FILE *aask PARAMS((void));
void zask PARAMS((void));
void set_signals PARAMS((int));
void ignore_signals PARAMS((void));
void makedirs PARAMS((char *, bool));
char *basename PARAMS((char *));
void Fseek PARAMS((FILE *, long, int));

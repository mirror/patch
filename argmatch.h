/* argmatch.h -- declarations for matching arguments against option lists */

#ifndef __P
# if defined __STDC__ || __GNUC__
#  define __P(args) args
# else
#  define __P(args) ()
# endif
#endif

int argmatch __P ((const char *, const char * const *));
void invalid_arg __P ((const char *, const char *, int));

extern char const program_name[];

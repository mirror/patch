/* basename.c -- return the last element in a path */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <backupfile.h>

/* In general, we can't use the builtin `basename' function if available,
   since it has different meanings in different environments.
   In some environments the builtin `basename' modifies its argument.  */

char *
base_name (name)
     char const *name;
{
  char const *base = name;

  while (*name)
    if (*name++ == '/')
      base = name;

  return (char *) base;
}

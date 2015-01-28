/* safe path traversal functions for 'patch' */

/* Copyright (C) 2015 Free Software Foundation, Inc.

   Written by Tim Waugh <twaugh@redhat.com> and
   Andreas Gruenbacher <agruenba@redhat.com>.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <alloca.h>
#include <safe.h>
#include "dirname.h"

#include <hash.h>
#include <xalloc.h>
#include <minmax.h>

#define XTERN extern
#include "common.h"

/* Path lookup results are cached in a hash table + LRU list. When the
   cache is full, the oldest entries are removed.  */

unsigned long dirfd_cache_misses;

struct cached_dirfd {
  /* lru list */
  struct cached_dirfd *prev, *next;

  /* key (openat arguments) */
  int dirfd;
  char *name;

  /* value (openat result) */
  int fd;
};

static Hash_table *cached_dirfds = NULL;
static size_t max_cached_fds;
static struct cached_dirfd lru_list = {
  .prev = &lru_list,
  .next = &lru_list,
};

static size_t hash_cached_dirfd (const void *entry, size_t table_size)
{
  const struct cached_dirfd *d = entry;
  size_t strhash = hash_string (d->name, table_size);
  return (strhash * 31 + d->dirfd) % table_size;
}

static bool compare_cached_dirfds (const void *_a,
				   const void *_b)
{
  const struct cached_dirfd *a = _a;
  const struct cached_dirfd *b = _b;

  return (a->dirfd == b->dirfd &&
	  !strcmp (a->name, b->name));
}

static void free_cached_dirfd (struct cached_dirfd *d)
{
  close (d->fd);
  free (d->name);
  free (d);
}

static void init_dirfd_cache (void)
{
  struct rlimit nofile;

  max_cached_fds = 8;
  if (getrlimit (RLIMIT_NOFILE, &nofile) == 0)
    max_cached_fds = MAX (nofile.rlim_cur / 4, max_cached_fds);

  cached_dirfds = hash_initialize (max_cached_fds,
				   NULL,
				   hash_cached_dirfd,
				   compare_cached_dirfds,
				   NULL);

  if (!cached_dirfds)
    xalloc_die ();
}

static void lru_list_add (struct cached_dirfd *entry, struct cached_dirfd *head)
{
  struct cached_dirfd *next = head->next;
  entry->prev = head;
  entry->next = next;
  head->next = next->prev = entry;
}

static void lru_list_del (struct cached_dirfd *entry)
{
  struct cached_dirfd *prev = entry->prev;
  struct cached_dirfd *next = entry->next;
  prev->next = next;
  next->prev = prev;
}

static struct cached_dirfd *lookup_cached_dirfd (int dirfd, const char *name)
{
  struct cached_dirfd *entry = NULL;

  if (cached_dirfds)
    {
      struct cached_dirfd key;
      key.dirfd = dirfd;
      key.name = (char *) name;
      entry = hash_lookup (cached_dirfds, &key);
      if (entry)
	{
	  /* Move this most recently used entry to the head of the lru list */
	  lru_list_del (entry);
	  lru_list_add (entry, &lru_list);
	}
    }

  return entry;
}

static void remove_cached_dirfd (struct cached_dirfd *entry)
{
  lru_list_del (entry);
  hash_delete (cached_dirfds, entry);
  free_cached_dirfd (entry);
}

static void insert_cached_dirfd (struct cached_dirfd *entry, int keepfd)
{
  if (cached_dirfds == NULL)
    init_dirfd_cache ();

  /* Trim off the least recently used entries */
  while (hash_get_n_entries (cached_dirfds) >= max_cached_fds)
    {
      struct cached_dirfd *last = lru_list.prev;
      assert (last != &lru_list);
      if (last->fd == keepfd)
	{
	  last = last->prev;
	  assert (last != &lru_list);
	}
      remove_cached_dirfd (last);
    }

  assert (hash_insert (cached_dirfds, entry) == entry);
  lru_list_add (entry, &lru_list);
}

static void invalidate_cached_dirfd (int dirfd, const char *name)
{
  struct cached_dirfd key, *entry;
  if (!cached_dirfds)
    return;

  key.dirfd = dirfd;
  key.name = (char *) name;
  entry = hash_lookup (cached_dirfds, &key);
  if (entry)
    remove_cached_dirfd (entry);
}

static int openat_cached (int dirfd, const char *name, int keepfd)
{
  int fd;
  struct cached_dirfd *entry = lookup_cached_dirfd (dirfd, name);

  if (entry)
    return entry->fd;
  dirfd_cache_misses++;

  /* Actually get the new directory file descriptor. Don't follow
     symbolic links. */
  fd = openat (dirfd, name, O_DIRECTORY | O_NOFOLLOW);

  /* Don't cache errors. */
  if (fd < 0)
    return fd;

  /* Store new cache entry */
  entry = xmalloc (sizeof (struct cached_dirfd));
  entry->dirfd = dirfd;
  entry->name = xstrdup (name);
  entry->fd = fd;
  insert_cached_dirfd (entry, keepfd);

  return fd;
}

/* Resolve the next path component in PATH inside DIRFD. */
static int traverse_next (int dirfd, const char **path, int keepfd)
{
  const char *p = *path;
  char *name;

  while (*p && ! ISSLASH (*p))
    p++;
  if (**path == '.' && *path + 1 == p)
    goto skip;
  name = alloca (p - *path + 1);
  memcpy(name, *path, p - *path);
  name[p - *path] = 0;

  dirfd = openat_cached (dirfd, name, keepfd);
  if (dirfd < 0 && dirfd != AT_FDCWD)
    {
      *path = p;
      return -1;
    }
skip:
  while (ISSLASH (*p))
    p++;
  *path = p;
  return dirfd;
}

/* Traverse PATHNAME.  Updates PATHNAME to point to the last path component and
   returns a file descriptor to its parent directory (which can be AT_FDCWD).
   When KEEPFD is given, make sure that the cache entry for DIRFD is not
   removed from the cache (and KEEPFD remains open) even if the cache grows
   beyond MAX_CACHED_FDS entries. */
static int traverse_another_path (const char **pathname, int keepfd)
{
  unsigned long misses = dirfd_cache_misses;
  const char *path = *pathname, *last;
  int dirfd = AT_FDCWD;

  if (! *path || IS_ABSOLUTE_FILE_NAME (path))
    return dirfd;

  /* Find the last pathname component */
  last = strrchr (path, 0) - 1;
  if (ISSLASH (*last))
    {
      while (last != path)
	if (! ISSLASH (*--last))
	  break;
    }
  while (last != path && ! ISSLASH (*(last - 1)))
    last--;
  if (last == path)
    return dirfd;

  if (debug & 32)
    printf ("Resolving path \"%.*s\"", (int) (last - path), path);

  while (path != last)
    {
      dirfd = traverse_next (dirfd, &path, keepfd);
      if (dirfd < 0 && dirfd != AT_FDCWD)
	{
	  if (debug & 32)
	    {
	      printf (" (failed)\n");
	      fflush (stdout);
	    }
	  if (errno == ELOOP)
	    {
	      fprintf (stderr, "Refusing to follow symbolic link %.*s\n",
		       (int) (path - *pathname), *pathname);
	      fatal_exit (0);
	    }
	  return dirfd;
	}
    }
  *pathname = last;
  if (debug & 32)
    {
      misses = dirfd_cache_misses - misses;
      if (! misses)
	printf(" (cached)\n");
      else
	printf (" (%lu miss%s)\n", misses, misses == 1 ? "" : "es");
      fflush (stdout);
    }
  return dirfd;
}

/* Just traverse PATHNAME; see traverse_another_path(). */
static int traverse_path (const char **pathname)
{
  return traverse_another_path (pathname, -1);
}

static int safe_xstat (const char *pathname, struct stat *buf, int flags)
{
  int dirfd;

  dirfd = traverse_path (&pathname);
  if (dirfd < 0 && dirfd != AT_FDCWD)
    return dirfd;
  return fstatat (dirfd, pathname, buf, flags);
}

/* Replacement for stat() */
int safe_stat (const char *pathname, struct stat *buf)
{
  return safe_xstat (pathname, buf, 0);
}

/* Replacement for lstat() */
int safe_lstat (const char *pathname, struct stat *buf)
{
  return safe_xstat (pathname, buf, AT_SYMLINK_NOFOLLOW);
}

/* Replacement for open() */
int safe_open (const char *pathname, int flags, mode_t mode)
{
  int dirfd;

  dirfd = traverse_path (&pathname);
  if (dirfd < 0 && dirfd != AT_FDCWD)
    return dirfd;
  return openat (dirfd, pathname, flags, mode);
}

/* Replacement for rename() */
int safe_rename (const char *oldpath, const char *newpath)
{
  int olddirfd, newdirfd;
  int ret;

  olddirfd = traverse_path (&oldpath);
  if (olddirfd != AT_FDCWD && olddirfd < 0)
    return olddirfd;

  newdirfd = traverse_another_path (&newpath, olddirfd);
  if (newdirfd != AT_FDCWD && newdirfd < 0)
    return newdirfd;

  ret = renameat (olddirfd, oldpath, newdirfd, newpath);
  invalidate_cached_dirfd (olddirfd, oldpath);
  invalidate_cached_dirfd (newdirfd, newpath);
  return ret;
}

/* Replacement for mkdir() */
int safe_mkdir (const char *pathname, mode_t mode)
{
  int dirfd;

  dirfd = traverse_path (&pathname);
  if (dirfd < 0 && dirfd != AT_FDCWD)
    return dirfd;
  return mkdirat (dirfd, pathname, mode);
}

/* Replacement for rmdir() */
int safe_rmdir (const char *pathname)
{
  int dirfd;
  int ret;

  dirfd = traverse_path (&pathname);
  if (dirfd < 0 && dirfd != AT_FDCWD)
    return dirfd;

  ret = unlinkat (dirfd, pathname, AT_REMOVEDIR);
  invalidate_cached_dirfd (dirfd, pathname);
  return ret;
}

/* Replacement for unlink() */
int safe_unlink (const char *pathname)
{
  int dirfd;

  dirfd = traverse_path (&pathname);
  if (dirfd < 0 && dirfd != AT_FDCWD)
    return dirfd;
  return unlinkat (dirfd, pathname, 0);
}

/* Replacement for symlink() */
int safe_symlink (const char *target, const char *linkpath)
{
  int dirfd;

  dirfd = traverse_path (&linkpath);
  if (dirfd < 0 && dirfd != AT_FDCWD)
    return dirfd;
  return symlinkat (target, dirfd, linkpath);
}

/* Replacement for chmod() */
int safe_chmod (const char *pathname, mode_t mode)
{
  int dirfd;

  dirfd = traverse_path (&pathname);
  if (dirfd < 0 && dirfd != AT_FDCWD)
    return dirfd;
  return fchmodat (dirfd, pathname, mode, 0);
}

/* Replacement for lchown() */
int safe_lchown (const char *pathname, uid_t owner, gid_t group)
{
  int dirfd;

  dirfd = traverse_path (&pathname);
  if (dirfd < 0 && dirfd != AT_FDCWD)
    return dirfd;
  return fchownat (dirfd, pathname, owner, group, AT_SYMLINK_NOFOLLOW);
}

/* Replacement for lutimens() */
int safe_lutimens (const char *pathname, struct timespec const times[2])
{
  int dirfd;

  dirfd = traverse_path (&pathname);
  if (dirfd < 0 && dirfd != AT_FDCWD)
    return dirfd;
  return utimensat (dirfd, pathname, times, AT_SYMLINK_NOFOLLOW);
}

/* Replacement for readlink() */
ssize_t safe_readlink(const char *pathname, char *buf, size_t bufsiz)
{
  int dirfd;

  dirfd = traverse_path (&pathname);
  if (dirfd < 0 && dirfd != AT_FDCWD)
    return dirfd;
  return readlinkat (dirfd, pathname, buf, bufsiz);
}

/* Replacement for access() */
int safe_access(const char *pathname, int mode)
{
  int dirfd;

  dirfd = traverse_path (&pathname);
  if (dirfd < 0 && dirfd != AT_FDCWD)
    return dirfd;
  return faccessat (dirfd, pathname, mode, 0);
}

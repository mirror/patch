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

#include "util.h"

#ifndef EFTYPE
# define EFTYPE 0
#endif

/* Path lookup results are cached in a hash table + LRU list. When the
   cache is full, the oldest entries are removed.  */

unsigned long dirfd_cache_misses;

struct cached_dirfd {
  /* lru list */
  struct cached_dirfd *prev, *next;
  struct cached_dirfd *parent;

  char *name;
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
  return (strhash * 31 + d->parent->fd) % table_size;
}

static bool compare_cached_dirfds (const void *_a,
				   const void *_b)
{
  const struct cached_dirfd *a = _a;
  const struct cached_dirfd *b = _b;

  return (a->parent->fd == b->parent->fd &&
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

static void lru_list_del_init (struct cached_dirfd *entry)
{
  lru_list_del (entry);
  entry->next = entry->prev = entry;
}

static struct cached_dirfd *lookup_cached_dirfd (struct cached_dirfd *dir, const char *name)
{
  struct cached_dirfd *entry = NULL;

  if (cached_dirfds)
    {
      struct cached_dirfd key;
      key.parent = dir;
      key.name = (char *) name;
      entry = hash_lookup (cached_dirfds, &key);
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
      if (last == &lru_list)
	break;
      if (last->fd == keepfd)
	{
	  last = last->prev;
	  if (last == &lru_list)
	    break;
	}
      remove_cached_dirfd (last);
    }

  assert (hash_insert (cached_dirfds, entry) == entry);
}

static void invalidate_cached_dirfd (int dirfd, const char *name)
{
  struct cached_dirfd dir, key, *entry;
  if (!cached_dirfds)
    return;

  dir.fd = dirfd;
  key.parent = &dir;
  key.name = (char *) name;
  entry = hash_lookup (cached_dirfds, &key);
  if (entry)
    remove_cached_dirfd (entry);
}

/* Put the looked up path back onto the lru list.  Return the file descriptor
   of the top entry.  */
static int put_path (struct cached_dirfd *entry)
{
  int fd = entry->fd;

  while (entry)
    {
      struct cached_dirfd *parent = entry->parent;
      if (! parent)
	break;
      lru_list_add (entry, &lru_list);
      entry = parent;
    }

  return fd;
}

static struct cached_dirfd *openat_cached (struct cached_dirfd *dir, const char *name, int keepfd)
{
  int fd;
  struct cached_dirfd *entry = lookup_cached_dirfd (dir, name);

  if (entry)
    {
      lru_list_del_init (entry);
      goto out;
    }
  dirfd_cache_misses++;

  /* Actually get the new directory file descriptor. Don't follow
     symbolic links. */
  fd = openat (dir->fd, name, O_DIRECTORY | O_NOFOLLOW);

  /* Don't cache errors. */
  if (fd < 0)
    return NULL;

  /* Store new cache entry */
  entry = xmalloc (sizeof (struct cached_dirfd));
  entry->prev = entry->next = entry;
  entry->parent = dir;
  entry->name = xstrdup (name);
  entry->fd = fd;
  insert_cached_dirfd (entry, keepfd);

out:
  return entry;
}

/* Resolve the next path component in PATH inside DIRFD. */
static struct cached_dirfd *traverse_next (struct cached_dirfd *dir, const char **path, int keepfd)
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

  dir = openat_cached (dir, name, keepfd);
  if (! dir)
    {
      *path = p;
      return NULL;
    }
skip:
  while (ISSLASH (*p))
    p++;
  *path = p;
  return dir;
}

/* Traverse PATHNAME.  Updates PATHNAME to point to the last path component and
   returns a file descriptor to its parent directory (which can be AT_FDCWD).
   When KEEPFD is given, make sure that the cache entry for DIRFD is not
   removed from the cache (and KEEPFD remains open) even if the cache grows
   beyond MAX_CACHED_FDS entries. */
static int traverse_another_path (const char **pathname, int keepfd)
{
  static struct cached_dirfd cwd = {
    .fd = AT_FDCWD,
  };

  unsigned long misses = dirfd_cache_misses;
  const char *path = *pathname, *last;
  struct cached_dirfd *dir = &cwd;

  if (! *path || IS_ABSOLUTE_FILE_NAME (path))
    return AT_FDCWD;

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
    return AT_FDCWD;

  if (debug & 32)
    printf ("Resolving path \"%.*s\"", (int) (last - path), path);

  while (path != last)
    {
      struct cached_dirfd *entry = traverse_next (dir, &path, keepfd);
      if (! entry)
	{
	  if (debug & 32)
	    {
	      printf (" (failed)\n");
	      fflush (stdout);
	    }
	  if (errno == ELOOP
	      || errno == EMLINK  /* FreeBSD 10.1: Too many links */
	      || errno == EFTYPE  /* NetBSD 6.1: Inappropriate file type or format */
	      || errno == ENOTDIR)
	    {
	      say ("file %.*s is not a directory\n",
		   (int) (path - *pathname), *pathname);
	      skip_rest_of_patch = true;
	    }
	  goto fail;
	}
      dir = entry;
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
  return put_path (dir);

fail:
  put_path (dir);
  return -1;
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
  if (olddirfd < 0 && olddirfd != AT_FDCWD)
    return olddirfd;

  newdirfd = traverse_another_path (&newpath, olddirfd);
  if (newdirfd < 0 && newdirfd != AT_FDCWD)
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
ssize_t safe_readlink (const char *pathname, char *buf, size_t bufsiz)
{
  int dirfd;

  dirfd = traverse_path (&pathname);
  if (dirfd < 0 && dirfd != AT_FDCWD)
    return dirfd;
  return readlinkat (dirfd, pathname, buf, bufsiz);
}

/* Replacement for access() */
int safe_access (const char *pathname, int mode)
{
  int dirfd;

  dirfd = traverse_path (&pathname);
  if (dirfd < 0 && dirfd != AT_FDCWD)
    return dirfd;
  return faccessat (dirfd, pathname, mode, 0);
}

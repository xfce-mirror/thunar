/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-scandir.h>
#include <thunar-vfs/thunar-vfs-alias.h>

/* Use g_access() if possible */
#if GLIB_CHECK_VERSION(2,8,0)
#include <glib/gstdio.h>
#else
#define g_access(path, mode) (access ((path), (mode)))
#endif



/* %&§$!# IRIX and Solaris */
#if defined(__sgi__) && !defined(dirfd)
#define dirfd(dp) (((DIR *) (dp))->__dd_fd)
#elif defined(__sun__) && !defined(dirfd)
#define dirfd(dp) (((DIR *) (dp))->dd_fd)
#endif



typedef struct _ThunarVfsScandirHandle ThunarVfsScandirHandle;



#ifdef __FreeBSD__
static gboolean thunar_vfs_scandir_collect_fast (ThunarVfsScandirHandle *handle,
                                                 ThunarVfsPath          *path,
                                                 GList                 **directoriesp);
#endif
static gboolean thunar_vfs_scandir_collect_slow (ThunarVfsScandirHandle *handle,
                                                 ThunarVfsPath          *path,
                                                 GList                 **directoriesp);
static gboolean thunar_vfs_scandir_collect      (ThunarVfsScandirHandle *handle,
                                                 volatile gboolean      *cancelled,
                                                 ThunarVfsPath          *path);



struct _ThunarVfsScandirHandle
{
  ThunarVfsScandirFlags flags;
  GCompareFunc          func;
  GList                *path_list;
  gchar                 fname[THUNAR_VFS_PATH_MAXSTRLEN];
};



#ifdef __FreeBSD__
static gboolean
thunar_vfs_scandir_collect_fast (ThunarVfsScandirHandle *handle,
                                 ThunarVfsPath          *path,
                                 GList                 **directoriesp)
{
#define statfsp ((struct statfs *) (dbuf))

  ThunarVfsPath *child;
  struct dirent *dp;
  struct stat    statb;
  gboolean       succeed = FALSE;
  gchar         *dbuf;
  guint          dlen;
  glong          base;
  gint           sverrno;
  gint           size;
  gint           loc;
  gint           fd;
  gint           n;

  /* try to open the file (the file name is placed in handle->fname) */
  fd = open (handle->fname, O_RDONLY | ((handle->flags & THUNAR_VFS_SCANDIR_FOLLOW_LINKS) ? 0 : O_NOFOLLOW));
  if (G_UNLIKELY (fd < 0))
    {
      /* translate EMLINK to ENOTDIR */
      if (G_UNLIKELY (errno == EMLINK))
        errno = ENOTDIR;
      return FALSE;
    }

  /* stat the file */
  if (fstat (fd, &statb) < 0)
    goto done;

  /* verify that we have a directory here */
  if (!S_ISDIR (statb.st_mode))
    {
      errno = ENOTDIR;
      goto done;
    }

  /* verify that we can enter the directory (else
   * we won't get any useful infos about the dir
   * contents either, so no need to continue). See
   * http://bugzilla.xfce.org/show_bug.cgi?id=1408.
   */
  if ((statb.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != (S_IXUSR | S_IXGRP | S_IXOTH) && (g_access (handle->fname, X_OK) < 0))
    {
      errno = EACCES;
      goto done;
    }

  /* calculate the directory buffer size */
  dlen = statb.st_blksize * 8;
  if (G_UNLIKELY ((dlen % DIRBLKSIZ) != 0))
    dlen = ((dlen + DIRBLKSIZ - 1) / DIRBLKSIZ) * DIRBLKSIZ;
  if (G_UNLIKELY (dlen < sizeof (struct statfs)))
    dlen = sizeof (struct statfs);

  /* allocate the directory buffer, which is
   * also used to store the statfs(2) results.
   */
  dbuf = alloca (dlen);

  /* determine the file system details */
  if (fstatfs (fd, statfsp) < 0)
    goto done;

  /* check if we have a unionfs here, which requires special processing (not provided by the fast collector) */
  if (memcmp (statfsp->f_fstypename, "unionfs", 8) == 0 || (statfsp->f_flags & MNT_UNION) != 0)
    {
      /* fallback to the slower collector */
      succeed = thunar_vfs_scandir_collect_slow (handle, path, directoriesp);
    }
  else
    {
      /* read the directory content */
      for (base = loc = size = 0;;)
        {
          /* check if we need to fill the buffer again */
          if (loc >= size)
            {
              /* read the next chunk */
              size = getdirentries (fd, dbuf, dlen, &base);
              if (size <= 0)
                break;
              loc = 0;
            }

          /* grab the pointer to the next entry */
          dp = (struct dirent *) (dbuf + loc);
          if (G_UNLIKELY (((gulong) dp & 0x03) != 0))
            break;

          /* verify the next record length */
          if (G_UNLIKELY (dp->d_reclen <= 0 || dp->d_reclen > dlen + 1 - loc))
            break;

          /* adjust the location pointer */
          loc += dp->d_reclen;

          /* verify the inode and type */
          if (G_UNLIKELY (dp->d_ino == 0 || dp->d_type == DT_WHT))
            continue;

          /* ignore '.' and '..' entries */
          if (G_UNLIKELY (dp->d_name[0] == '.' && (dp->d_name[1] == '\0' || (dp->d_name[1] == '.' && dp->d_name[2] == '\0'))))
            continue;

          /* add the child path to the path list */
          child = thunar_vfs_path_relative (path, dp->d_name);
          if (handle->func != NULL)
            handle->path_list = g_list_insert_sorted (handle->path_list, child, handle->func);
          else
            handle->path_list = g_list_prepend (handle->path_list, child);

          /* check if we want to collect children for recursive scanning */
          if (G_UNLIKELY (directoriesp != NULL))
            {
              /* DT_UNKNOWN must be handled for certain file systems */
              if (G_UNLIKELY (dp->d_type == DT_UNKNOWN))
                {
                  /* determine the absolute path to the child */
                  if (!thunar_vfs_path_to_string (child, handle->fname, sizeof (handle->fname), NULL) < 0)
                    {
                      errno = ENAMETOOLONG;
                      goto done;
                    }

                  /* stat the child (according to the FOLLOW_LINKS policy) */
                  n = ((handle->flags & THUNAR_VFS_SCANDIR_FOLLOW_LINKS) == 0)
                    ? lstat (handle->fname, &statb) : stat (handle->fname, &statb);

                  /* check if we have a directory here */
                  if (n == 0 && S_ISDIR (statb.st_mode))
                    dp->d_type = DT_DIR;
                }

              /* check if we have a directory */
              if (dp->d_type == DT_DIR)
                *directoriesp = g_list_prepend (*directoriesp, child);
            }
        }

      succeed = TRUE;
    }

done:
  sverrno = errno;
  close (fd);
  errno = sverrno;
  return succeed;

#undef statfsp
}
#endif



static gboolean
thunar_vfs_scandir_collect_slow (ThunarVfsScandirHandle *handle,
                                 ThunarVfsPath          *path,
                                 GList                 **directoriesp)
{
  ThunarVfsPath *child;
  struct dirent  dbuf;
  struct dirent *dp;
  struct stat    fstatb;
  gint           sverrno;
  gint           n;
  DIR           *dirp;

  /* try to open the directory (handle->buffer still
   * contains the absolute path when we get here!).
   */
  dirp = opendir (handle->fname);
  if (G_UNLIKELY (dirp == NULL))
    return FALSE;

  /* stat the just opened directory */
  if (fstat (dirfd (dirp), &fstatb) < 0)
    goto error;

  /* verify that the directory is really the directory we want
   * to open. If not, we've probably detected a race condition,
   * so we'll better stop rather than doing anything stupid
   * (remember, this method is also used in collecting
   * files for the unlink job!!). Better safe than sorry!
   */
  if (G_UNLIKELY ((handle->flags & THUNAR_VFS_SCANDIR_FOLLOW_LINKS) == 0))
    {
      struct stat lstatb;

      /* stat the path (without following links) */
      if (lstat (handle->fname, &lstatb) < 0)
        goto error;

      /* check that we have the same file here */
      if (fstatb.st_ino != lstatb.st_ino || fstatb.st_dev != lstatb.st_dev)
        {
          errno = ENOTDIR;
          goto error;
        }
    }

  /* verify that we can enter the directory (else
   * we won't get any useful infos about the dir
   * contents either, so no need to continue). See
   * http://bugzilla.xfce.org/show_bug.cgi?id=1408.
   */
  if ((fstatb.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != (S_IXUSR | S_IXGRP | S_IXOTH) && (g_access (handle->fname, X_OK) < 0))
    {
      errno = EACCES;
      goto error;
    }

  /* read the directory content */
  for (;;)
    {
      /* read the next directory entry */
      if (readdir_r (dirp, &dbuf, &dp) < 0 || dp == NULL)
        break;

      /* ignore '.' and '..' entries */
      if (G_UNLIKELY (dp->d_name[0] == '.' && (dp->d_name[1] == '\0' || (dp->d_name[1] == '.' && dp->d_name[2] == '\0'))))
        continue;

      /* add the child path to the path list */
      child = thunar_vfs_path_relative (path, dp->d_name);
      if (handle->func != NULL)
        handle->path_list = g_list_insert_sorted (handle->path_list, child, handle->func);
      else
        handle->path_list = g_list_prepend (handle->path_list, child);

      /* check if we want to collect children for recursive scanning */
      if (G_UNLIKELY (directoriesp != NULL))
        {
          struct stat statb;

          /* determine the absolute path to the child */
          if (thunar_vfs_path_to_string (child, handle->fname, sizeof (handle->fname), NULL) < 0)
            {
              errno = ENAMETOOLONG;
              goto error;
            }

          /* stat the child (according to the FOLLOW_LINKS policy) */
          n = ((handle->flags & THUNAR_VFS_SCANDIR_FOLLOW_LINKS) == 0)
            ? lstat (handle->fname, &statb) : stat (handle->fname, &statb);

          /* check if we have a directory here */
          if (n == 0 && S_ISDIR (statb.st_mode))
            *directoriesp = g_list_prepend (*directoriesp, child);
        }
    }

  closedir (dirp);
  return TRUE;

error:
  sverrno = errno;
  closedir (dirp);
  errno = sverrno;
  return FALSE;
}



static gboolean
thunar_vfs_scandir_collect (ThunarVfsScandirHandle *handle,
                            volatile gboolean      *cancelled,
                            ThunarVfsPath          *path)
{
  gboolean succeed = FALSE;
  GList   *directories = NULL;
  GList   *lp;
  gint     sverrno;

  /* determine the absolute path */
  if (thunar_vfs_path_to_string (path, handle->fname, sizeof (handle->fname), NULL) < 0)
    {
      errno = ENAMETOOLONG;
      return FALSE;
    }

#ifdef __FreeBSD__
  /* We can some nice things in FreeBSD */
  succeed = thunar_vfs_scandir_collect_fast (handle, path, (handle->flags & THUNAR_VFS_SCANDIR_RECURSIVE) ? &directories : NULL);
#else
  /* use the generic (slower) collector */
  succeed = thunar_vfs_scandir_collect_slow (handle, path, (handle->flags & THUNAR_VFS_SCANDIR_RECURSIVE) ? &directories : NULL);
#endif

  /* check if we want to recurse */
  if (G_UNLIKELY (directories != NULL))
    {
      /* perform the recursion */
      for (lp = directories; lp != NULL && succeed; lp = lp->next)
        {
          /* check if the user cancelled the scanning */
          if (G_UNLIKELY (cancelled != NULL && *cancelled))
            {
              succeed = FALSE;
              errno = EINTR;
              break;
            }

          /* collect the files for this directory */
          succeed = thunar_vfs_scandir_collect (handle, cancelled, lp->data);
          if (G_UNLIKELY (!succeed))
            {
              /* we can ignore certain errors here */
              if (errno == EACCES || errno == EMLINK || errno == ENOTDIR || errno == ENOENT || errno == EPERM)
                succeed = TRUE;
            }
        }

      /* release the directory list */
      sverrno = errno;
      g_list_free (directories);
      errno = sverrno;
    }

  return succeed;
}



/**
 * thunar_vfs_scandir:
 * @path
 * @cancelled : pointer to a volatile boolean variable, which if
 *              %TRUE means to cancel the scan operation. May be
 *              %NULL in which case the scanner cannot be
 *              cancelled.
 * @flags
 * @func
 * @error     : return location for errors or %NULL.
 *
 * The caller is responsible to free the returned list
 * using thunar_vfs_path_list_free() when no longer
 * needed.
 *
 * If @cancelled becomes true during the scan operation, %NULL
 * will be returned and @error will be set to %G_FILE_ERROR_INTR.
 *
 * Return value:
 **/
GList*
thunar_vfs_scandir (ThunarVfsPath        *path,
                    volatile gboolean    *cancelled,
                    ThunarVfsScandirFlags flags,
                    GCompareFunc          func,
                    GError              **error)
{
  ThunarVfsScandirHandle handle;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* initialize the scandir handle */
  handle.flags = flags;
  handle.func = func;
  handle.path_list = NULL;

  /* collect the paths */
  if (!thunar_vfs_scandir_collect (&handle, cancelled, path))
    {
      /* forward the error */
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
      thunar_vfs_path_list_free (handle.path_list);
      return NULL;
    }

  return handle.path_list;
}



#define __THUNAR_VFS_SCANDIR_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>

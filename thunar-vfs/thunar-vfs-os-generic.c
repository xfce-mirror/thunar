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

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-os.h>
#include <thunar-vfs/thunar-vfs-path-private.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>

/* Use g_access() on win32 */
#if GLIB_CHECK_VERSION(2,8,0) && defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_access(path, mode) (access ((path), (mode)))
#endif

/* %&§$!# IRIX and Solaris */
#if defined(__sgi__) && !defined(dirfd)
#define dirfd(dp) (((DIR *) (dp))->__dd_fd)
#elif (defined(__sun__) || defined(__sun)) && !defined(dirfd)
#define dirfd(dp) (((DIR *) (dp))->dd_fd)
#endif



/**
 * _thunar_vfs_os_is_dir_empty:
 * @absolute_path : an absolute path to a folder.
 *
 * Returns %TRUE if the directory at the @absolute_path
 * does not contain any files or folders, or if the
 * @absolute_path does not refer to a directory. Otherwise
 * if the @absolute_path contains atleast one entry except
 * '.' and '..', %FALSE is returned.
 *
 * Return value: %TRUE if the directory at @absolute_path
 *               is empty.
 **/
gboolean
_thunar_vfs_os_is_dir_empty (const gchar *absolute_path)
{
  struct dirent *dp;
  DIR           *dirp;

  dirp = opendir (absolute_path);
  if (G_LIKELY (dirp != NULL))
    {
      /* ignore '.' and '..' */
      dp = readdir (dirp);
      dp = readdir (dirp);
      dp = readdir (dirp);
      closedir (dirp);
      return (dp == NULL);
    }

  return TRUE;
}



/**
 * _thunar_vfs_os_scandir:
 * @path               : the #ThunarVfsPath for the directory, which does
 *                       not need to be a local path. The returned path
 *                       list will be made of paths relative to this one.
 * @absolute_path      : the absolute local path of the directory to scan.
 * @follow_links       : %TRUE to follow symlinks to directories.
 * @directories_return : pointer to a list into which the direct subfolders
 *                       found during scanning will be placed (for recursive
 *                       scanning), or %NULL if you are not interested in a
 *                       separate list of subfolders. Note that the returned
 *                       list items need to be freed, but the #ThunarVfsPath<!---->s
 *                       in the list do not have an extra reference.
 * @error              : return location for errors or %NULL.
 *
 * The list returned in @directories_return, if not %NULL, must be freed using
 * g_list_free() when no longer needed.
 *
 * The returned list of #ThunarVfsPath<!---->s must be freed by the caller using
 * thunar_vfs_path_list_unref() when no longer needed.
 *
 * Return value: the list of #ThunarVfsPath<!---->s in the folder at the @absolute_path,
 *               or %NULL in case of an error. Note that %NULL may also mean that the
 *               folder is empty.
 **/
GList*
_thunar_vfs_os_scandir (ThunarVfsPath *path,
                        const gchar   *absolute_path,
                        gboolean       follow_links,
                        GList        **directories_return,
                        GError       **error)
{
  struct dirent *dp;
  struct stat    fstatb;
  struct stat    statb;
  GList         *path_list = NULL;
  gchar         *filename;
  DIR           *dirp;

  _thunar_vfs_return_val_if_fail (g_path_is_absolute (absolute_path), NULL);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* try to open the directory */
  dirp = opendir (absolute_path);
  if (G_UNLIKELY (dirp == NULL))
    {
      _thunar_vfs_set_g_error_from_errno3 (error);
      return NULL;
    }

  /* stat the just opened directory */
  if (fstat (dirfd (dirp), &fstatb) < 0)
    goto error;

  /* verify that the directory is really the directory we want
   * to open. If not, we've probably detected a race condition,
   * so we'll better stop rather than doing anything stupid
   * (remember, this method is also used in collecting
   * files for the unlink job!!). Better safe than sorry!
   */
  if (G_UNLIKELY (!follow_links))
    {
      /* stat the path (without following links) */
      if (lstat (absolute_path, &statb) < 0)
        goto error;

      /* check that we have the same file here */
      if (fstatb.st_ino != statb.st_ino || fstatb.st_dev != statb.st_dev)
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
  if ((fstatb.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != (S_IXUSR | S_IXGRP | S_IXOTH) && (g_access (absolute_path, X_OK) < 0))
    {
      errno = EACCES;
      goto error;
    }

  /* read the directory content */
  for (;;)
    {
      /* read the next directory entry */
      dp = readdir (dirp);
      if (G_UNLIKELY (dp == NULL))
        break;

      /* ignore '.' and '..' entries */
      if (G_UNLIKELY (dp->d_name[0] == '.' && (dp->d_name[1] == '\0' || (dp->d_name[1] == '.' && dp->d_name[2] == '\0'))))
        continue;

      /* add the child path to the path list */
      path_list = g_list_prepend (path_list, _thunar_vfs_path_child (path, dp->d_name));

      /* check if we want to collect children for recursive scanning */
      if (G_UNLIKELY (directories_return != NULL))
        {
          /* determine the absolute path to the child */
          filename = g_build_filename (absolute_path, dp->d_name, NULL);

          /* check if we have a directory here (according to the FOLLOW_LINKS policy) */
          if ((follow_links ? stat (filename, &statb) : lstat (filename, &statb)) == 0 && S_ISDIR (statb.st_mode))
            *directories_return = g_list_prepend (*directories_return, path_list->data);

          /* cleanup */
          g_free (filename);
        }
    }

done:
  closedir (dirp);
  return path_list;

error:
  _thunar_vfs_set_g_error_from_errno3 (error);
  goto done;
}



#define __THUNAR_VFS_OS_GENERIC_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>

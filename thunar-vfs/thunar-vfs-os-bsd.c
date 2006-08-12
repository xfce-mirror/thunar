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
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-os.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>



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
#ifndef HAVE_GETDENTS
  glong          basep = 0;
#endif
  gchar          dbuf[8 * DIRBLKSIZ];
  gint           size = 0;
  gint           loc = 0;
  gint           fd;

  /* try to open the directory */
  fd = open (absolute_path, O_NONBLOCK | O_RDONLY);
  if (G_LIKELY (fd >= 0))
    {
      /* read the directory content */
      for (;;)
        {
          /* check if we need to fill the buffer again */
          if (loc >= size)
            {
#ifdef HAVE_GETDENTS
              /* read the next chunk (no base pointer needed) */
              size = getdents (fd, dbuf, sizeof (dbuf));
#else
              /* read the next chunk (OpenBSD fallback) */
              size = getdirentries (fd, dbuf, sizeof (dbuf), &basep);
#endif

              /* check for eof/error */
              if (size <= 0)
                break;
              loc = 0;
            }

          /* grab the pointer to the next entry */
          dp = (struct dirent *) (dbuf + loc);
          if (G_UNLIKELY (((gulong) dp & 0x03) != 0))
            {
invalid:
              size = 0;
              break;
            }

          /* verify the next record length */
          if (G_UNLIKELY (dp->d_reclen <= 0 || dp->d_reclen > sizeof (dbuf) + 1 - loc))
            goto invalid;

          /* adjust the location pointer */
          loc += dp->d_reclen;

          /* verify the inode */
          if (G_UNLIKELY (dp->d_fileno == 0))
            continue;

#ifdef DT_WHT
          /* verify the type (OpenBSD lacks whiteout) */
          if (G_UNLIKELY (dp->d_type == DT_WHT))
            continue;
#endif

          /* ignore '.' and '..' entries */
          if (G_UNLIKELY (dp->d_name[0] == '.' && (dp->d_name[1] == '\0' || (dp->d_name[1] == '.' && dp->d_name[2] == '\0'))))
            continue;

          /* jep, the directory is not empty */
          break;
        }

      /* close the directory */
      close (fd);
    }

  return (size <= 0);
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
 * Note that folders in a filesystem mounted via mount_union(8) may
 * not be handled properly in all cases. But since mount_union(8) do
 * not seem to be widely used today, we will ignore the fact and wait
 * for the first user to complain.
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
_thunar_vfs_os_scandir (ThunarVfsPath  *path,
                        const gchar    *absolute_path,
                        gboolean        follow_links,
                        GList         **directories_return,
                        GError        **error)
{
  struct dirent *dp;
  struct stat    statb;
#ifndef HAVE_GETDENTS
  glong          basep = 0;
#endif
  GList         *path_list = NULL;
  gchar         *filename;
  gchar         *dbuf;
  guint          dlen;
  gint           size;
  gint           loc;
  gint           fd;

  _thunar_vfs_return_val_if_fail (g_path_is_absolute (absolute_path), NULL);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* try to open the file (the file name is placed in handle->fname) */
  fd = open (absolute_path, (follow_links ? 0 : O_NOFOLLOW) | O_NONBLOCK | O_RDONLY);
  if (G_UNLIKELY (fd < 0))
    {
      /* translate EMLINK to ENOTDIR */
      _thunar_vfs_set_g_error_from_errno (error, (errno == EMLINK) ? ENOTDIR : errno);
      return NULL;
    }

  /* stat the file */
  if (G_UNLIKELY (fstat (fd, &statb) < 0))
    {
error0:
      _thunar_vfs_set_g_error_from_errno3 (error);
      goto done;
    }

  /* verify that we have a directory here */
  if (G_UNLIKELY (!S_ISDIR (statb.st_mode)))
    {
      _thunar_vfs_set_g_error_from_errno (error, ENOTDIR);
      goto done;
    }

  /* verify that we can enter the directory (else
   * we won't get any useful infos about the dir
   * contents either, so no need to continue). See
   * http://bugzilla.xfce.org/show_bug.cgi?id=1408.
   */
  if (G_UNLIKELY ((statb.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != (S_IXUSR | S_IXGRP | S_IXOTH) && (access (absolute_path, X_OK) < 0)))
    {
      _thunar_vfs_set_g_error_from_errno (error, EACCES);
      goto done;
    }

  /* close the directory on exec */
  if (G_UNLIKELY (fcntl (fd, F_SETFD, FD_CLOEXEC) < 0))
    goto error0;

  /* calculate the directory buffer size */
  dlen = statb.st_blksize * 8;
  if (G_UNLIKELY ((dlen % DIRBLKSIZ) != 0))
    dlen = ((dlen + DIRBLKSIZ - 1) / DIRBLKSIZ) * DIRBLKSIZ;

  /* allocate the directory buffer, which is
   * also used to store the statfs(2) results.
   */
  dbuf = alloca (dlen);

  /* read the directory content */
  for (loc = size = 0;;)
    {
      /* check if we need to fill the buffer again */
      if (loc >= size)
        {
#ifdef HAVE_GETDENTS
          /* read the next chunk (no need for a base pointer) */
          size = getdents (fd, dbuf, dlen);
#else
          /* read the next chunk (OpenBSD fallback) */
          size = getdirentries (fd, dbuf, dlen, &basep);
#endif

          /* check for eof/error */
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

      /* verify the inode */
      if (G_UNLIKELY (dp->d_fileno == 0))
        continue;

#ifdef DT_WHT
      /* verify the type (OpenBSD lacks whiteout) */
      if (G_UNLIKELY (dp->d_type == DT_WHT))
        continue;
#endif

      /* ignore '.' and '..' entries */
      if (G_UNLIKELY (dp->d_name[0] == '.' && (dp->d_name[1] == '\0' || (dp->d_name[1] == '.' && dp->d_name[2] == '\0'))))
        continue;

      /* add the child path to the path list */
      path_list = g_list_prepend (path_list, thunar_vfs_path_relative (path, dp->d_name));

      /* check if we want to collect children for recursive scanning */
      if (G_UNLIKELY (directories_return != NULL))
        {
          /* DT_UNKNOWN must be handled for certain file systems */
          if (G_UNLIKELY (dp->d_type == DT_UNKNOWN))
            {
              /* stat the child (according to the FOLLOW_LINKS policy) to see if we have a directory here */
              filename = g_build_filename (absolute_path, dp->d_name, NULL);
              if ((follow_links ? stat (filename, &statb) : lstat (filename, &statb)) == 0 && S_ISDIR (statb.st_mode))
                dp->d_type = DT_DIR;
              g_free (filename);
            }

          /* check if we have a directory */
          if (dp->d_type == DT_DIR)
            *directories_return = g_list_prepend (*directories_return, path_list->data);
        }
    }

done:
  close (fd);
  return path_list;
}



#define __THUNAR_VFS_OS_BSD_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>

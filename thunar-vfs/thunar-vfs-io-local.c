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
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif
#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-io-local-xfer.h>
#include <thunar-vfs/thunar-vfs-io-local.h>
#include <thunar-vfs/thunar-vfs-mime-database-private.h>
#include <thunar-vfs/thunar-vfs-os.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>

/* Use g_access(), g_lstat(), g_rename() and g_stat() on win32 */
#if defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_access(path, mode) (access ((path), (mode)))
#define g_lstat(path, statb) (lstat ((path), (statb)))
#define g_rename(from, to) (rename ((from), (to)))
#define g_stat(path, statb) (stat ((path), (statb)))
#endif

/* Use native strlcpy() if available */
#if defined(HAVE_STRLCPY)
#define g_strlcpy(dst, src, size) (strlcpy ((dst), (src), (size)))
#endif



/**
 * _thunar_vfs_io_local_get_free_space:
 * @path              : a #ThunarVfsPath for a file:-URI.
 * @free_space_return : return location for the amount of free space or %NULL.
 *
 * Determines the amount of free space available on the volume on which the
 * file to which @path refers resides. If the system is able to determine the
 * amount of free space, it will be placed into the location to which
 * @free_space_return points and %TRUE will be returned, else the function
 * will return %FALSE indicating that the system is unable to determine the
 * amount of free space.
 *
 * Return value: %TRUE if the amount of free space could be determined, else
 *               %FALSE:
 **/
gboolean
_thunar_vfs_io_local_get_free_space (const ThunarVfsPath *path,
                                     ThunarVfsFileSize   *free_space_return)
{
#if defined(HAVE_STATFS) && !defined(HAVE_STATVFS1) && !defined(__sgi__) && !defined(__sun__) && !defined(__sun)
  struct statfs  statfsb;
#elif defined(HAVE_STATVFS) || defined(HAVE_STATVFS1)
  struct statvfs statfsb;
#endif
  gboolean       succeed;
  gchar          absolute_path[THUNAR_VFS_PATH_MAXSTRLEN];

  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_local (path), FALSE);

  /* determine the absolute local path */
  if (thunar_vfs_path_to_string (path, absolute_path, sizeof (absolute_path), NULL) < 0)
    return FALSE;

  /* determine the amount of free space for the mount point */
#if defined(HAVE_STATFS) && !defined(HAVE_STATVFS1) && !defined(__sgi__) && !defined(__sun__) && !defined(__sun)
  succeed = (statfs (absolute_path, &statfsb) == 0);   /* the good old BSD way */
#elif defined(HAVE_STATVFS1)
  succeed = (statvfs1 (absolute_path, &statfsb, ST_WAIT) == 0); /* the new NetBSD way */
#elif defined(HAVE_STATVFS)
  succeed = (statvfs (absolute_path, &statfsb) == 0);  /* the Linux, IRIX, Solaris way */
#else
#error "Add support for your operating system here"
#endif

  /* return the free space */
  if (G_LIKELY (succeed && free_space_return != NULL))
    *free_space_return = (statfsb.f_bavail * statfsb.f_bsize);

  return succeed;
}



/**
 * _thunar_vfs_io_local_get_info:
 * @path          : the #ThunarVfsPath to use for the return info.
 * @absolute_path : the absolute, local path to the file.
 * @error         : return location for errors or %NULL.
 *
 * @absolute_path does not need to be the string representation of the
 * @path. For example, @path could be a trash path, while @absolute_path
 * represents the file refered to by @path, but as a local file system
 * path.
 *
 * The caller is responsible to free the returned object using
 * thunar_vfs_info_unref() when no longer needed.
 *
 * Return value: the #ThunarVfsInfo for the file at the @absolute_path
 *               or %NULL in case of an error.
 **/
ThunarVfsInfo*
_thunar_vfs_io_local_get_info (ThunarVfsPath *path,
                               const gchar   *absolute_path,
                               GError       **error)
{
  ThunarVfsMimeInfo *fake_mime_info;
  ThunarVfsInfo     *info;
  const guchar      *s;
  const gchar       *name;
  const gchar       *str;
  struct stat        lsb;
  struct stat        sb;
  XfceRc            *rc;
  GList             *mime_infos;
  GList             *lp;
  gchar             *p;

  _thunar_vfs_return_val_if_fail (g_path_is_absolute (absolute_path), NULL);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, NULL);
  _thunar_vfs_return_val_if_fail (path != NULL, NULL);

  /* try to stat the file */
  if (G_UNLIKELY (g_lstat (absolute_path, &lsb) < 0))
    {
      _thunar_vfs_set_g_error_from_errno3 (error);
      return NULL;
    }

  /* allocate a new info object */
  info = _thunar_vfs_slice_new (ThunarVfsInfo);
  info->path = thunar_vfs_path_ref (path);
  info->ref_count = 1;

  info->custom_icon = NULL;

  /* determine the display name of the file */
  name = thunar_vfs_path_get_name (path);
  for (s = (const guchar *) name; *s >= 32 && *s <= 127; ++s)
    ;
  if (G_LIKELY (*s == '\0'))
    {
      /* we don't need to perform any transformation if
       * the file contains only valid ASCII characters.
       */
      info->display_name = (gchar *) name;
    }
  else
    {
      /* determine the displayname using various tricks */
      info->display_name = g_filename_display_name (name);

      /* go on until s reaches the end of the string, as
       * we need it for the hidden file detection below.
       */
      for (; *s != '\0'; ++s)
        ;
    }

  /* check whether we have a hidden file here */
  if ((s - (const guchar *) name) > 1 && (*name == '.' || *(s - 1) == '~'))
    info->flags = THUNAR_VFS_FILE_FLAGS_HIDDEN;
  else
    info->flags = THUNAR_VFS_FILE_FLAGS_NONE;

  /* determine the POSIX file attributes */
  if (G_LIKELY (!S_ISLNK (lsb.st_mode)))
    {
      info->type = (lsb.st_mode & S_IFMT) >> 12;
      info->mode = lsb.st_mode & 07777;
      info->uid = lsb.st_uid;
      info->gid = lsb.st_gid;
      info->size = lsb.st_size;
      info->atime = lsb.st_atime;
      info->ctime = lsb.st_ctime;
      info->mtime = lsb.st_mtime;
      info->device = lsb.st_dev;
    }
  else
    {
      /* whatever comes, we have a symlink here */
      info->flags |= THUNAR_VFS_FILE_FLAGS_SYMLINK;

      /* check if it's a broken link */
      if (g_stat (absolute_path, &sb) == 0)
        {
          info->type = (sb.st_mode & S_IFMT) >> 12;
          info->mode = sb.st_mode & 07777;
          info->uid = sb.st_uid;
          info->gid = sb.st_gid;
          info->size = sb.st_size;
          info->atime = sb.st_atime;
          info->ctime = sb.st_ctime;
          info->mtime = sb.st_mtime;
          info->device = sb.st_dev;
        }
      else
        {
          info->type = THUNAR_VFS_FILE_TYPE_SYMLINK;
          info->mode = lsb.st_mode & 07777;
          info->uid = lsb.st_uid;
          info->gid = lsb.st_gid;
          info->size = lsb.st_size;
          info->atime = lsb.st_atime;
          info->ctime = lsb.st_ctime;
          info->mtime = lsb.st_mtime;
          info->device = lsb.st_dev;
        }
    }

  /* check if we can read the file */
  if ((info->mode & 00444) != 0 && g_access (absolute_path, R_OK) == 0)
    info->flags |= THUNAR_VFS_FILE_FLAGS_READABLE;

  /* check if we can write to the file */
  if ((info->mode & 00222) != 0 && g_access (absolute_path, W_OK) == 0)
    info->flags |= THUNAR_VFS_FILE_FLAGS_WRITABLE;

  /* determine the file's mime type */
  switch (info->type)
    {
    case THUNAR_VFS_FILE_TYPE_PORT:
      info->mime_info = thunar_vfs_mime_database_get_info (_thunar_vfs_mime_database, "inode/port");
      break;

    case THUNAR_VFS_FILE_TYPE_DOOR:
      info->mime_info = thunar_vfs_mime_database_get_info (_thunar_vfs_mime_database, "inode/door");
      break;

    case THUNAR_VFS_FILE_TYPE_SOCKET:
      info->mime_info = thunar_vfs_mime_database_get_info (_thunar_vfs_mime_database, "inode/socket");
      break;

    case THUNAR_VFS_FILE_TYPE_SYMLINK:
      info->mime_info = thunar_vfs_mime_database_get_info (_thunar_vfs_mime_database, "inode/symlink");
      break;

    case THUNAR_VFS_FILE_TYPE_BLOCKDEV:
      info->mime_info = thunar_vfs_mime_database_get_info (_thunar_vfs_mime_database, "inode/blockdevice");
      break;

    case THUNAR_VFS_FILE_TYPE_DIRECTORY:
      /* mime type for directories is cached */
      info->mime_info = thunar_vfs_mime_info_ref (_thunar_vfs_mime_inode_directory);

      /* check if we have the root folder here */
      if (G_UNLIKELY (absolute_path[0] == G_DIR_SEPARATOR && absolute_path[1] == '\0'))
        {
          /* root folder gets a special custom icon... */
          info->custom_icon = g_strdup ("gnome-dev-harddisk");

          /* ...and a special display name */
          info->display_name = g_strdup (_("File System"));
        }
      break;

    case THUNAR_VFS_FILE_TYPE_CHARDEV:
      info->mime_info = thunar_vfs_mime_database_get_info (_thunar_vfs_mime_database, "inode/chardevice");
      break;

    case THUNAR_VFS_FILE_TYPE_FIFO:
      info->mime_info = thunar_vfs_mime_database_get_info (_thunar_vfs_mime_database, "inode/fifo");
      break;

    case THUNAR_VFS_FILE_TYPE_REGULAR:
      /* determine the MIME-type for the regular file */
      info->mime_info = thunar_vfs_mime_database_get_info_for_file (_thunar_vfs_mime_database, absolute_path, info->display_name);

      /* check if the file is executable (for security reasons
       * we only allow execution of well known file types).
       */
      if ((info->mode & 0444) != 0 && g_access (absolute_path, X_OK) == 0)
        {
          mime_infos = thunar_vfs_mime_database_get_infos_for_info (_thunar_vfs_mime_database, info->mime_info);
          for (lp = mime_infos; lp != NULL; lp = lp->next)
            {
              if (lp->data == _thunar_vfs_mime_application_x_executable || lp->data == _thunar_vfs_mime_application_x_shellscript)
                {
                  info->flags |= THUNAR_VFS_FILE_FLAGS_EXECUTABLE;
                  break;
                }
            }
          thunar_vfs_mime_info_list_free (mime_infos);
        }

      /* check if we have a .desktop (and NOT a .directory) file here */
      if (G_UNLIKELY (info->mime_info == _thunar_vfs_mime_application_x_desktop && strcmp (thunar_vfs_path_get_name (path), ".directory") != 0))
        {
          /* try to query the hints from the .desktop file */
          rc = xfce_rc_simple_open (absolute_path, TRUE);
          if (G_LIKELY (rc != NULL))
            {
              /* we're only interested in the desktop data */
              xfce_rc_set_group (rc, "Desktop Entry");

              /* check if we have a valid icon info */
              str = xfce_rc_read_entry_untranslated (rc, "Icon", NULL);
              if (G_LIKELY (str != NULL && *str != '\0'))
                {
                  /* setup the custom icon */
                  info->custom_icon = g_strdup (str);

                  /* drop any suffix (e.g. '.png') from themed icons */
                  if (!g_path_is_absolute (info->custom_icon))
                    {
                      p = strrchr (info->custom_icon, '.');
                      if (G_UNLIKELY (p != NULL))
                        *p = '\0';
                    }
                }

              /* determine the type of the .desktop file */
              str = xfce_rc_read_entry_untranslated (rc, "Type", "Application");

              /* check if the desktop file refers to an application
               * and has a non-NULL Exec field set, or it's a Link
               * with a valid URL field.
               */
              if (G_LIKELY (exo_str_is_equal (str, "Application"))
                  && xfce_rc_read_entry (rc, "Exec", NULL) != NULL)
                {
                  info->flags |= THUNAR_VFS_FILE_FLAGS_EXECUTABLE;
                }
              else if (G_LIKELY (exo_str_is_equal (str, "Link"))
                  && xfce_rc_read_entry (rc, "URL", NULL) != NULL)
                {
                  info->flags |= THUNAR_VFS_FILE_FLAGS_EXECUTABLE;
                }

              /* check if we have a valid name info */
              name = xfce_rc_read_entry (rc, "Name", NULL);
              if (G_LIKELY (name != NULL && *name != '\0' && g_utf8_validate (name, -1, NULL)))
                {
                  /* check if we declared the file as executable */
                  if ((info->flags & THUNAR_VFS_FILE_FLAGS_EXECUTABLE) != 0)
                    {
                      /* if the name contains a dir separator, use only the part after
                       * the dir separator for checking.
                       */
                      str = strrchr (name, G_DIR_SEPARATOR);
                      if (G_LIKELY (str == NULL))
                        str = (gchar *) name;
                      else
                        str += 1;

                      /* check if the file tries to look like a regular document (i.e.
                       * a display name of 'file.png'), maybe a virus or other malware.
                       */
                      fake_mime_info = thunar_vfs_mime_database_get_info_for_name (_thunar_vfs_mime_database, str);
                      if (fake_mime_info != _thunar_vfs_mime_application_octet_stream && fake_mime_info != info->mime_info)
                        {
                          /* release the previous mime info */
                          thunar_vfs_mime_info_unref (info->mime_info);

                          /* set the MIME type of the file to 'x-thunar/suspected-malware' to indicate that
                           * it's not safe to trust the file content and execute it or otherwise operate on it.
                           */
                          info->mime_info = thunar_vfs_mime_database_get_info (_thunar_vfs_mime_database, "x-thunar/suspected-malware");

                          /* reset the executable flag */
                          info->flags &= ~THUNAR_VFS_FILE_FLAGS_EXECUTABLE;

                          /* reset the custom icon */
                          g_free (info->custom_icon);
                          info->custom_icon = NULL;

                          /* reset the name str, so we display the real file name */
                          name = NULL;
                        }
                      thunar_vfs_mime_info_unref (fake_mime_info);
                    }

                  /* check if the name str wasn't reset */
                  if (G_LIKELY (name != NULL))
                    {
                      /* release the previous display name */
                      if (G_UNLIKELY (info->display_name != thunar_vfs_path_get_name (path)))
                        g_free (info->display_name);

                      /* use the name specified by the .desktop file as display name */
                      info->display_name = g_strdup (name);
                    }
                }

              /* close the file */
              xfce_rc_close (rc);
            }
        }
      break;

    default:
      _thunar_vfs_assert_not_reached ();
      break;
    }

  return info;
}



/**
 * _thunar_vfs_io_local_get_metadata:
 * @path     : a #ThunarVfsPath to a local file.
 * @metadata : a #ThunarVfsInfoMetadata.
 * @error    : return location for errors or %NULL.
 *
 * Returns the @metadata for the @path, which must be a local
 * path, or %NULL if the @metadata is not available for the
 * @path.
 *
 * The caller is responsible to free the returned string using
 * g_free() when no longer needed.
 *
 * Return value: the @metadata for @path or %NULL.
 **/
gchar*
_thunar_vfs_io_local_get_metadata (ThunarVfsPath        *path,
                                   ThunarVfsInfoMetadata metadata,
                                   GError              **error)
{
  gchar *absolute_path;
  gchar *result = NULL;

  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_local (path), NULL);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* check the type of metadata we're interested in */
  switch (metadata)
    {
    case THUNAR_VFS_INFO_METADATA_FILE_LINK_TARGET:
      /* read the link target of the file */
      absolute_path = thunar_vfs_path_dup_string (path);
      result = g_file_read_link (absolute_path, error);
      g_free (absolute_path);
      break;

    default:
      _thunar_vfs_set_g_error_not_supported (error);
      break;
    }

  return result;
}



/**
 * _thunar_vfs_io_local_listdir:
 * @path      : the path to the local directory to list its contents.
 * @error     : return location for errors or %NULL.
 *
 * Scans the folder pointed to by the specified @path and returns the list of
 * #ThunarVfsInfo<!---->s for the files and folders in @path.
 *
 * The caller is responsible to free the returned list of #ThunarVfsInfo<!---->s
 * using thunar_vfs_info_list_free() when no longer needed.
 *
 * Return value: a #GList of #ThunarVfsInfo<!---->s for the items in the local
 *               folder pointed to by @path. May be %NULL in case of an error,
 *               while on the other hand, %NULL may also indicate that the
 *               folder contains no files.
 **/
GList*
_thunar_vfs_io_local_listdir (ThunarVfsPath *path,
                              GError       **error)
{
  ThunarVfsInfo *info;
  gchar          absolute_path[THUNAR_VFS_PATH_MAXSTRLEN + 128];
  GList         *list;
  GList         *sp;
  GList         *tp;
  gint           n;

  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_local (path), NULL);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* try to determine the absolute path to the folder */
  n = thunar_vfs_path_to_string (path, absolute_path, THUNAR_VFS_PATH_MAXSTRLEN, error);
  if (G_UNLIKELY (n < 0))
    return NULL;

  /* try to scan the specified directory */
  list = _thunar_vfs_os_scandir (path, absolute_path, TRUE, NULL, error);
  if (G_UNLIKELY (list == NULL))
    return NULL;

  /* append a dir separator to the absolute path */
  absolute_path[n - 1] = G_DIR_SEPARATOR;

  /* associate file infos with the paths in the folder */
  for (sp = tp = list; sp != NULL; sp = sp->next)
    {
      /* generate the absolute path to the file, relative to the folder path */
      g_strlcpy (absolute_path + n, thunar_vfs_path_get_name (sp->data), sizeof (absolute_path) - n);

      /* try to determine the file info */
      info = _thunar_vfs_io_local_get_info (sp->data, absolute_path, NULL);

      /* replace the path with the info on the list */
      if (G_LIKELY (info != NULL))
        {
          /* just decrease the ref_count on path (info holds a reference now) */
          _thunar_vfs_path_unref_nofree (sp->data);

          /* add the info to the list */
          tp->data = info;
          tp = tp->next;
        }
      else
        {
          /* no info, may need to free the path */
          thunar_vfs_path_unref (sp->data);
        }
    }

  /* release the not-filled list items (only non-NULL in case of an info error) */
  if (G_UNLIKELY (tp != NULL))
    {
      if (G_LIKELY (tp->prev != NULL))
        tp->prev->next = NULL;
      else
        list = NULL;
      g_list_free (tp);
    }

  return list;
}



/**
 * _thunar_vfs_io_local_copy_file:
 * @source_path        : the #ThunarVfsPath to the source file.
 * @target_path        : the #ThunarVfsPath to the target location.
 * @target_path_return : the final #ThunarVfsPath of the target location, which may be
 *                       different than the @target_path.
 * @callback           : the progress callback, invoked whenever a new chunk of data is copied.
 * @callback_data      : additional data to pass to @callback.
 * @error              : return location for errors or %NULL.
 *
 * Copies the file at the @source_path to the location specified by the
 * @target_path. If @source_path and @target_path are equal a new target
 * file name is choosen automatically, which indicates that its a copy of
 * an existing file.
 *
 * The final target path, which may be different than @target_path, is
 * placed in the location pointed to by @target_path_return. The caller
 * is responsible to free the object using thunar_vfs_path_unref().
 * @target_path_return will only be set if %TRUE is returned.
 *
 * As a special case, if @callback returns %FALSE (which means the
 * operation should be cancelled), this method returns %FALSE and
 * @error is set to #G_FILE_ERROR_INTR.
 *
 * Return value: %TRUE if the file was successfully copied, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_io_local_copy_file (const ThunarVfsPath           *source_path,
                                ThunarVfsPath                 *target_path,
                                ThunarVfsPath                **target_path_return,
                                ThunarVfsIOOpsProgressCallback callback,
                                gpointer                       callback_data,
                                GError                       **error)
{
  ThunarVfsPath *path;
  GError        *err = NULL;
  guint          n;

  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_local (source_path), FALSE);
  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_local (target_path), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_vfs_return_val_if_fail (target_path_return != NULL, FALSE);
  _thunar_vfs_return_val_if_fail (callback != NULL, FALSE);

  /* check if source and target are the same */
  if (G_LIKELY (!thunar_vfs_path_equal (source_path, target_path)))
    {
      /* not the same, just a simple copy operation */
      if (_thunar_vfs_io_local_xfer_copy (source_path, target_path, callback, callback_data, TRUE, error))
        {
          /* target path didn't change */
          *target_path_return = thunar_vfs_path_ref (target_path);
          return TRUE;
        }
      return FALSE;
    }

  /* generate a duplicate name for the target path */
  for (n = 1;; ++n)
    {
      /* try to generate the next duplicate path */
      path = _thunar_vfs_io_local_xfer_next_path (source_path, target_path->parent, n, THUNAR_VFS_IO_LOCAL_XFER_COPY, &err);
      if (G_UNLIKELY (path == NULL))
        goto error;

      /* try to copy to the duplicate file */
      if (_thunar_vfs_io_local_xfer_copy (source_path, path, callback, callback_data, FALSE, &err))
        {
          /* return the generated target path */
          *target_path_return = path;
          return TRUE;
        }

      /* release the generated path */
      thunar_vfs_path_unref (path);

      /* check if we cannot continue */
      if (err->domain != G_FILE_ERROR || err->code != G_FILE_ERROR_EXIST)
        {
error:    /* propagate the error */
          g_propagate_error (error, err);
          return FALSE;
        }

      /* and once again... */
      g_clear_error (&err);
    }

  /* we never get here */
  _thunar_vfs_assert_not_reached ();
  return FALSE;
}



/**
 * _thunar_vfs_io_local_link_file:
 * @source_path        : the #ThunarVfsPath to the source file.
 * @target_path        : the #ThunarVfsPath to the target file.
 * @target_path_return : return location for the final target path or %NULL.
 * @error              : return location for errors or %NULL.
 *
 * The final target path, which may be different than @target_path, is placed in
 * the location pointed to by @target_path_return. The caller is responsible to
 * free the object using thunar_vfs_path_unref(). @target_path_return will only
 * be set if %TRUE is returned.
 *
 * Return value: %FALSE if @error is set, else %TRUE.
 **/
gboolean
_thunar_vfs_io_local_link_file (const ThunarVfsPath *source_path,
                                ThunarVfsPath       *target_path,
                                ThunarVfsPath      **target_path_return,
                                GError             **error)
{
  ThunarVfsPath *path;
  GError        *err = NULL;
  guint          n;

  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_local (source_path), FALSE);
  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_local (target_path), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_vfs_return_val_if_fail (target_path_return != NULL, FALSE);

  /* check if source and target are the same */
  if (G_LIKELY (!thunar_vfs_path_equal (source_path, target_path)))
    {
      /* source and target aren't the same, just create the link */
      if (_thunar_vfs_io_local_xfer_link (source_path, target_path, error))
        {
          *target_path_return = thunar_vfs_path_ref (target_path);
          return TRUE;
        }
      return FALSE;
    }

  /* generate a new link name for the target path */
  for (n = 1;; ++n)
    {
      /* try to generate the next link path */
      path = _thunar_vfs_io_local_xfer_next_path (source_path, target_path->parent, n, THUNAR_VFS_IO_LOCAL_XFER_LINK, &err);
      if (G_UNLIKELY (path == NULL))
        goto error;

        {
        }

      /* try to symlink to the new file */
      if (_thunar_vfs_io_local_xfer_link (source_path, path, &err))
        {
          /* return the generated target path */
          *target_path_return = path;
          return TRUE;
        }

      /* release the generated path */
      thunar_vfs_path_unref (path);

      /* check if we cannot continue */
      if (err->domain != G_FILE_ERROR || err->code != G_FILE_ERROR_EXIST)
        {
error:    /* propagate the error */
          g_propagate_error (error, err);
          return FALSE;
        }

      /* and once again... */
      g_clear_error (&err);
    }

  /* we never get here */
  _thunar_vfs_assert_not_reached ();
  return FALSE;
}



/**
 * _thunar_vfs_io_local_move_file:
 * @source_path : the #ThunarVfsPath to the source file.
 * @target_path : the #ThunarVfsPath to the target file.
 * @error       : return location for errors or %NULL.
 *
 * Moves the file at the @source_path to the @target_path.
 *
 * Return value: %TRUE if the file at the @source_path was
 *               successfully moved to @target_path, %FALSE
 *               otherwise.
 **/
gboolean
_thunar_vfs_io_local_move_file (const ThunarVfsPath *source_path,
                                const ThunarVfsPath *target_path,
                                GError             **error)
{
  gboolean succeed;
  gchar   *source_absolute_path;
  gchar   *target_absolute_path;

  /* make sure that the target path does not already exist */
  target_absolute_path = thunar_vfs_path_dup_string (target_path);
  succeed = (g_access (target_absolute_path, F_OK) < 0);
  if (G_LIKELY (succeed))
    {
      /* try to rename the source file to the target path */
      source_absolute_path = thunar_vfs_path_dup_string (source_path);
      succeed = (g_rename (source_absolute_path, target_absolute_path) == 0);
      if (G_UNLIKELY (!succeed))
        {
          /* we cannot perform the rename */
          _thunar_vfs_set_g_error_from_errno3 (error);
        }
      g_free (source_absolute_path);
    }
  else
    {
      /* we don't want to override files w/o warning */
      _thunar_vfs_set_g_error_from_errno (error, EEXIST);
    }
  g_free (target_absolute_path);

  return succeed;
}



/**
 * _thunar_vfs_io_local_rename:
 * @info  : a #ThunarVfsInfo for a local file.
 * @name  : the new file name in UTF-8 encoding.
 * @error : return location for errors or %NULL.
 *
 * Tries to rename the file referred to by @info to the
 * new @name.
 *
 * The rename operation is smart in that it checks the
 * type of @info first, and if @info refers to a
 * <filename>.desktop</filename> file, the file name
 * won't be touched, but instead the <literal>Name</literal>
 * field of the <filename>.desktop</filename> will be
 * changed to @name. Else, if @info refers to a regular
 * file or directory, the file will be given a new
 * name.
 *
 * Return value: %TRUE on success, else %FALSE.
 **/
gboolean
_thunar_vfs_io_local_rename (ThunarVfsInfo *info,
                             const gchar   *name,
                             GError       **error)
{
  const gchar * const *locale;
  ThunarVfsMimeInfo   *mime_info;
  GKeyFile            *key_file;
  gsize                data_length;
  gchar               *data;
  gchar               *key;
  gchar                src_path[PATH_MAX + 1];
  gchar               *dir_name;
  gchar               *dst_name;
  gchar               *dst_path;
  FILE                *fp;

  _thunar_vfs_return_val_if_fail (*name != '\0' && strchr (name, G_DIR_SEPARATOR) == NULL, FALSE);
  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_local (info->path), FALSE);
  _thunar_vfs_return_val_if_fail (g_utf8_validate (name, -1, NULL), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* determine the source path */
  if (thunar_vfs_path_to_string (info->path, src_path, sizeof (src_path), error) < 0)
    return FALSE;

  /* check if we have a .desktop (and NOT a .directory) file here */
  if (G_UNLIKELY (info->mime_info == _thunar_vfs_mime_application_x_desktop && strcmp (thunar_vfs_path_get_name (info->path), ".directory") != 0))
    {
      /* try to open the .desktop file */
      key_file = g_key_file_new ();
      if (!g_key_file_load_from_file (key_file, src_path, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, error))
        {
error0:
          g_key_file_free (key_file);
          return FALSE;
        }

      /* check if the file is valid */
      if (G_UNLIKELY (!g_key_file_has_group (key_file, "Desktop Entry")))
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Invalid desktop file"));
          goto error0;
        }

      /* save the new name (localized if required) */
      for (locale = g_get_language_names (); *locale != NULL; ++locale)
        {
          key = g_strdup_printf ("Name[%s]", *locale);
          if (g_key_file_has_key (key_file, "Desktop Entry", key, NULL))
            {
              g_key_file_set_string (key_file, "Desktop Entry", key, name);
              g_free (key);
              break;
            }
          g_free (key);
        }

      /* fallback to unlocalized name */
      if (G_UNLIKELY (*locale == NULL))
        g_key_file_set_string (key_file, "Desktop Entry", "Name", name);

      /* serialize the key file to a buffer */
      data = g_key_file_to_data (key_file, &data_length, error);
      g_key_file_free (key_file);
      if (G_UNLIKELY (data == NULL))
        return FALSE;

      /* try to open the file for writing */
      fp = fopen (src_path, "w");
      if (G_UNLIKELY (fp == NULL))
        {
          _thunar_vfs_set_g_error_from_errno3 (error);
error1:
          g_free (data);
          return FALSE;
        }
      
      /* write the data back to the file */
      if (fwrite (data, data_length, 1, fp) != 1)
        {
          _thunar_vfs_set_g_error_from_errno3 (error);
          fclose (fp);
          goto error1;
        }

      /* close the file */
      fclose (fp);

      /* release the previous display name */
      if (G_LIKELY (info->display_name != thunar_vfs_path_get_name (info->path)))
        g_free (info->display_name);

      /* apply the new display name */
      info->display_name = g_strdup (name);

      /* clean up */
      g_free (data);
    }
  else
    {
      /* convert the destination file to local encoding */
      dst_name = g_filename_from_utf8 (name, -1, NULL, NULL, error);
      if (G_UNLIKELY (dst_name == NULL))
        return FALSE;

      /* determine the destination path */
      dir_name = g_path_get_dirname (src_path);
      dst_path = g_build_filename (dir_name, dst_name, NULL);
      g_free (dst_name);
      g_free (dir_name);

      /* verify that the rename target does not already exist */
      if (G_UNLIKELY (g_file_test (dst_path, G_FILE_TEST_EXISTS)))
        {
          /* tell the user that the file already exists */
          errno = EEXIST;
error2:
          _thunar_vfs_set_g_error_from_errno3 (error);
          g_free (dst_path);
          return FALSE;
        }

      /* perform the rename */
      if (G_UNLIKELY (g_rename (src_path, dst_path) < 0))
        goto error2;

      /* update the info's display name */
      if (info->display_name != thunar_vfs_path_get_name (info->path))
        g_free (info->display_name);
      info->display_name = g_strdup (name);

      /* check if this is a hidden file now */
      if (strlen (name) > 1 && (name[0] == '.' || name[strlen (name) - 1] == '~'))
        info->flags |= THUNAR_VFS_FILE_FLAGS_HIDDEN;
      else
        info->flags &= ~THUNAR_VFS_FILE_FLAGS_HIDDEN;

      /* update the info's path */
      thunar_vfs_path_unref (info->path);
      info->path = thunar_vfs_path_new (dst_path, NULL);

      /* if we have a regular file here, then we'll need to determine
       * the mime type again, as it may be based on the file name
       */
      if (G_LIKELY (info->type == THUNAR_VFS_FILE_TYPE_REGULAR))
        {
          mime_info = info->mime_info;
          info->mime_info = thunar_vfs_mime_database_get_info_for_file (_thunar_vfs_mime_database, dst_path, info->display_name);
          thunar_vfs_mime_info_unref (mime_info);
        }

      /* clean up */
      g_free (dst_path);
    }

  return TRUE;
}



#define __THUNAR_VFS_IO_LOCAL_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>

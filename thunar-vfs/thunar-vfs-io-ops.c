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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-io-local.h>
#include <thunar-vfs/thunar-vfs-io-ops.h>
#include <thunar-vfs/thunar-vfs-io-trash.h>
#include <thunar-vfs/thunar-vfs-monitor-private.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-thumb-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>

/* Use g_lstat(), g_mkdir() and g_remove() on win32 */
#if defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_lstat(path, statb) (lstat ((path), (statb)))
#define g_mkdir(path, mode) (mkdir ((path), (mode)))
#define g_remove(path) (remove ((path)))
#endif



static void tvio_set_g_error_with_paths (GError       **error,
                                         const gchar   *format,
                                         ThunarVfsPath *source_path,
                                         ThunarVfsPath *target_path,
                                         GError        *err);



static void
tvio_set_g_error_with_paths (GError       **error,
                             const gchar   *format,
                             ThunarVfsPath *source_path,
                             ThunarVfsPath *target_path,
                             GError        *err)
{
  gchar *source_display_name;
  gchar *target_display_name;
  gchar *s;

  /* determine a displayable variant of the source_path */
  s = _thunar_vfs_path_is_local (source_path) ? thunar_vfs_path_dup_string (source_path) : thunar_vfs_path_dup_uri (source_path);
  source_display_name = g_filename_display_name (s);
  g_free (s);

  /* determine a displayable variant of the target_path */
  s = _thunar_vfs_path_is_local (target_path) ? thunar_vfs_path_dup_string (target_path) : thunar_vfs_path_dup_uri (target_path);
  target_display_name = g_filename_display_name (s);
  g_free (s);

  /* generate a useful error message */
  s = g_strdup_printf (format, source_display_name, target_display_name);
  g_set_error (error, err->domain, err->code, "%s: %s", s, err->message);
  g_free (s);

  /* cleanup */
  g_free (target_display_name);
  g_free (source_display_name);
}



/**
 * _thunar_vfs_io_ops_get_file_size_and_type:
 * @path        : the #ThunarVfsPath to the file whose size and type
 *                to determine.
 * @size_return : return location for the file size or %NULL.
 * @type_return : return location for the file type or %NULL.
 * @error       : return location for errors or %NULL.
 *
 * Determines the size and type of the file at @path.
 *
 * Return value: %TRUE if the size and type was determined successfully,
 *               %FALSE otherwise.
 **/
gboolean
_thunar_vfs_io_ops_get_file_size_and_type (ThunarVfsPath       *path,
                                           ThunarVfsFileSize   *size_return,
                                           ThunarVfsFileType   *type_return,
                                           GError             **error)
{
  struct stat statb;
  gboolean    succeed = FALSE;
  gchar      *absolute_path;
  gchar      *uri;

  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* determine the absolute local path for the path object */
  absolute_path = _thunar_vfs_path_translate_dup_string (path, THUNAR_VFS_PATH_SCHEME_FILE, error);
  if (G_LIKELY (absolute_path != NULL))
    {
      /* determine the file info for the absolute path */
      succeed = (g_lstat (absolute_path, &statb) == 0);
      if (G_LIKELY (succeed))
        {
          /* return size and type if requested */
          if (G_LIKELY (size_return != NULL))
            *size_return = statb.st_size;
          if (G_LIKELY (type_return != NULL))
            *type_return = (statb.st_mode & S_IFMT) >> 12;
        }
      else
        {
          /* generate a useful error message */
          uri = thunar_vfs_path_dup_uri (path);
          _thunar_vfs_set_g_error_from_errno2 (error, errno, _("Failed to determine file info for \"%s\""), uri);
          g_free (uri);
        }

      /* cleanup */
      g_free (absolute_path);
    }

  return succeed;
}



/**
 * _thunar_vfs_io_ops_copy_file:
 * @source_path        : the #ThunarVfsPath to the source file.
 * @target_path        : the #ThunarVfsPath to the target location.
 * @target_path_return : the final #ThunarVfsPath of the target location, which may be
 *                       different than the @target_path.
 * @callback           : the progress callback, invoked whenever a new chunk of data is copied.
 * @callback_data      : additional data to pass to @callback.
 * @error              : return location for errors or %NULL.
 *
 * Copies the file at the @source_path to the location specified by the
 * @target_path. The final target location will be returned in @target_path_return.
 *
 * As a special case, if @callback returns %FALSE, the operation will be cancelled
 * and an @error with %G_FILE_ERROR and %G_FILE_ERROR_INTR will be returned.
 *
 * Return value: %TRUE if the file was successfully copied, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_io_ops_copy_file (ThunarVfsPath                 *source_path,
                              ThunarVfsPath                 *target_path,
                              ThunarVfsPath                **target_path_return,
                              ThunarVfsIOOpsProgressCallback callback,
                              gpointer                       callback_data,
                              GError                       **error)
{
  gboolean succeed;
  GError  *err = NULL;

  _thunar_vfs_return_val_if_fail (!thunar_vfs_path_is_root (source_path), FALSE);
  _thunar_vfs_return_val_if_fail (!thunar_vfs_path_is_root (target_path), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_vfs_return_val_if_fail (callback != NULL, FALSE);

  /* check if either source or target is trash */
  if (_thunar_vfs_path_is_trash (source_path) || _thunar_vfs_path_is_trash (target_path))
    {
      /* copy to or from the trash may result in a new target path */
      succeed = _thunar_vfs_io_trash_copy_file (source_path, target_path, &target_path, callback, callback_data, &err);
    }
  else if (_thunar_vfs_path_is_local (source_path) && _thunar_vfs_path_is_local (target_path))
    {
      /* copying files in the file system */
      succeed = _thunar_vfs_io_local_copy_file (source_path, target_path, &target_path, callback, callback_data, &err);
    }
  else
    {
      _thunar_vfs_set_g_error_not_supported (error);
      return FALSE;
    }

  /* check if we succeed */
  if (G_LIKELY (succeed))
    {
      /* schedule a "created" event for the target path */
      thunar_vfs_monitor_feed (_thunar_vfs_monitor, THUNAR_VFS_MONITOR_EVENT_CREATED, target_path);

      /* return the new target path */
      if (G_LIKELY (target_path_return != NULL))
        *target_path_return = target_path;
      else
        thunar_vfs_path_unref (target_path);
    }
  else
    {
      /* generate a useful error message */
      tvio_set_g_error_with_paths (error, _("Failed to copy \"%s\" to \"%s\""), source_path, target_path, err);
      g_error_free (err);
    }

  return succeed;
}



/**
 * _thunar_vfs_io_ops_link_file:
 * @source_path        : the #ThunarVfsPath to the source file.
 * @target_path        : the #ThunarVfsPath to the target location.
 * @target_path_return : return location for the final target path or %NULL.
 * @error              : return location for errors or %NULL.
 *
 * Creates a symbolic link at @target_path, which points to @source_path. The
 * real target path may be different than the suggested @target_path, and will
 * be returned in @target_path_return if not %NULL.
 *
 * The caller is responsible to free the #ThunarVfsPath object returned in 
 * @target_path_return using thunar_vfs_path_unref() when no longer needed
 * if %TRUE is returned.
 *
 * Return value: %TRUE if the link was successfully created, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_io_ops_link_file (ThunarVfsPath  *source_path,
                              ThunarVfsPath  *target_path,
                              ThunarVfsPath **target_path_return,
                              GError        **error)
{
  gboolean succeed;
  GError  *err = NULL;

  _thunar_vfs_return_val_if_fail (!thunar_vfs_path_is_root (source_path), FALSE);
  _thunar_vfs_return_val_if_fail (!thunar_vfs_path_is_root (target_path), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* check if either source and target are local */
  if (_thunar_vfs_path_is_local (source_path) && _thunar_vfs_path_is_local (target_path))
    {
      /* copying files in the file system */
      succeed = _thunar_vfs_io_local_link_file (source_path, target_path, &target_path, &err);
    }
  else
    {
      /* impossible to perform the link operation */
      _thunar_vfs_set_g_error_not_supported (&err);
      succeed = FALSE;
    }

  /* check if we succeed */
  if (G_LIKELY (succeed))
    {
      /* schedule a "created" event for the target path */
      thunar_vfs_monitor_feed (_thunar_vfs_monitor, THUNAR_VFS_MONITOR_EVENT_CREATED, target_path);

      /* return the new target path */
      if (G_LIKELY (target_path_return != NULL))
        *target_path_return = target_path;
      else
        thunar_vfs_path_unref (target_path);
    }
  else
    {
      /* generate a useful error message */
      tvio_set_g_error_with_paths (error, _("Failed to link \"%s\" to \"%s\""), source_path, target_path, err);
      g_error_free (err);
    }

  return succeed;
}



/**
 * _thunar_vfs_io_ops_move_file:
 * @source_path        : the #ThunarVfsPath to the source file.
 * @target_path        : the #ThunarVfsPath to the target location.
 * @target_path_return : the final #ThunarVfsPath of the target location, which
 *                       may be different than the @target_path.
 * @error              : return location for errors or %NULL.
 *
 * Moves the file at the @source_path to the location specified by the
 * @target_path. The final target location is returned in @target_path_return.
 *
 * Return value: %TRUE if the file was successfully moved, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_io_ops_move_file (ThunarVfsPath      *source_path,
                              ThunarVfsPath      *target_path,
                              ThunarVfsPath     **target_path_return,
                              GError            **error)
{
  gboolean succeed;
  GError  *err = NULL;

  _thunar_vfs_return_val_if_fail (!thunar_vfs_path_is_root (source_path), FALSE);
  _thunar_vfs_return_val_if_fail (!thunar_vfs_path_is_root (target_path), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* check if either source or target is trash */
  if (_thunar_vfs_path_is_trash (source_path) || _thunar_vfs_path_is_trash (target_path))
    {
      /* move to or from the trash may result in a new target path */
      succeed = _thunar_vfs_io_trash_move_file (source_path, target_path, &target_path, &err);
    }
  else if (_thunar_vfs_path_is_local (source_path) && _thunar_vfs_path_is_local (target_path))
    {
      /* moving files in the file system */
      succeed = _thunar_vfs_io_local_move_file (source_path, target_path, &err);

      /* local move does not alter the target path */
      if (G_LIKELY (succeed))
        thunar_vfs_path_ref (target_path);
    }
  else
    {
      _thunar_vfs_set_g_error_not_supported (error);
      return FALSE;
    }

  /* check if we succeed */
  if (G_LIKELY (succeed))
    {
      /* drop the thumbnails for the source path */
      _thunar_vfs_thumbnail_remove_for_path (source_path);

      /* schedule a "created" event for the target path */
      thunar_vfs_monitor_feed (_thunar_vfs_monitor, THUNAR_VFS_MONITOR_EVENT_CREATED, target_path);

      /* schedule a "deleted" event for the source path */
      thunar_vfs_monitor_feed (_thunar_vfs_monitor, THUNAR_VFS_MONITOR_EVENT_DELETED, source_path);

      /* return the new target path */
      if (G_LIKELY (target_path_return != NULL))
        *target_path_return = target_path;
      else
        thunar_vfs_path_unref (target_path);
    }
  else
    {
      /* generate a useful error message */
      tvio_set_g_error_with_paths (error, _("Failed to move \"%s\" to \"%s\""), source_path, target_path, err);
      g_error_free (err);
    }

  return succeed;
}



/**
 * _thunar_vfs_io_ops_mkdir:
 * @path  : the #ThunarVfsPath to the directory to create.
 * @mode  : the new #ThunarVfsFileMode for the directory.
 * @flags : see #ThunarVfsIOOpsFlags.
 * @error : return location for errors or %NULL.
 *
 * Creates a new directory at the specified @path with the given @mode.
 *
 * Return value: %TRUE if the directory was created successfully or the
 *               directory exists and %THUNAR_VFS_IO_OPS_IGNORE_EEXIST
 *               was specified in @flags, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_io_ops_mkdir (ThunarVfsPath      *path,
                          ThunarVfsFileMode   mode,
                          ThunarVfsIOOpsFlags flags,
                          GError            **error)
{
  gboolean succeed;
  gchar   *absolute_path;
  gchar   *display_name;
  gint     sverrno;

  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* translate the path object to an absolute path */
  absolute_path = _thunar_vfs_path_translate_dup_string (path, THUNAR_VFS_PATH_SCHEME_FILE, error);
  if (G_UNLIKELY (absolute_path == NULL))
    return FALSE;

  /* try to create the directory with the given mode */
  succeed = (g_mkdir (absolute_path, mode) == 0 || (errno == EEXIST && (flags & THUNAR_VFS_IO_OPS_IGNORE_EEXIST) != 0));
  if (G_UNLIKELY (!succeed))
    {
      /* save the errno value, may be modified */
      sverrno = errno;

      /* generate a useful error message */
      display_name = g_filename_display_name (absolute_path);
      _thunar_vfs_set_g_error_from_errno2 (error, sverrno, _("Failed to create directory \"%s\""), display_name);
      g_free (display_name);
    }
  else if (G_LIKELY (errno != EEXIST))
    {
      /* schedule a "created" event for the directory path */
      thunar_vfs_monitor_feed (_thunar_vfs_monitor, THUNAR_VFS_MONITOR_EVENT_CREATED, path);
    }

  /* cleanup */
  g_free (absolute_path);

  return succeed;
}



/**
 * _thunar_vfs_io_ops_remove:
 * @path  : the #ThunarVfsPath to the file to remove.
 * @flags : see #ThunarVfsIOOpsFlags.
 * @error : return location for errors or %NULL.
 *
 * Removes the file or folder at the specified @path.
 *
 * Return value: %TRUE if the file was successfully
 *               removed, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_io_ops_remove (ThunarVfsPath      *path,
                           ThunarVfsIOOpsFlags flags,
                           GError            **error)
{
  gboolean succeed;
  GError  *err = NULL;
  gchar   *message;
  gchar   *display_name;
  gchar   *absolute_path;

  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* remove using the appropriate backend */
  switch (thunar_vfs_path_get_scheme (path))
    {
    case THUNAR_VFS_PATH_SCHEME_FILE:
      absolute_path = thunar_vfs_path_dup_string (path);
      succeed = (g_remove (absolute_path) == 0);
      if (G_UNLIKELY (!succeed))
        _thunar_vfs_set_g_error_from_errno3 (&err);
      g_free (absolute_path);
      break;

    case THUNAR_VFS_PATH_SCHEME_TRASH:
      succeed = _thunar_vfs_io_trash_remove (path, &err);
      break;

    default:
      _thunar_vfs_set_g_error_not_supported (error);
      return FALSE;
    }

  /* drop the thumbnails if the removal succeed */
  if (G_LIKELY (succeed))
    {
      /* drop the thumbnails for the removed file */
      _thunar_vfs_thumbnail_remove_for_path (path);

      /* tell the monitor about the removed file */
      thunar_vfs_monitor_feed (_thunar_vfs_monitor, THUNAR_VFS_MONITOR_EVENT_DELETED, path);
    }
  else if ((flags & THUNAR_VFS_IO_OPS_IGNORE_ENOENT) != 0 && err->domain == G_FILE_ERROR && err->code == G_FILE_ERROR_NOENT)
    {
      /* we should ignore ENOENT errors */
      g_error_free (err);
      succeed = TRUE;
    }
  else
    {
      /* generate a useful error message */
      display_name = _thunar_vfs_path_dup_display_name (path);
      message = g_strdup_printf (_("Failed to remove \"%s\""), display_name);
      g_set_error (error, err->domain, err->code, "%s: %s", message, err->message);
      g_free (display_name);
      g_error_free (err);
      g_free (message);
    }

  return succeed;
}



#define __THUNAR_VFS_IO_OPS_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>

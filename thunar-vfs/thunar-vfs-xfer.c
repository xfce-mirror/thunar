/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
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
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-monitor.h>
#include <thunar-vfs/thunar-vfs-xfer.h>
#include <thunar-vfs/thunar-vfs-alias.h>

#if GLIB_CHECK_VERSION(2,6,0)
#include <glib/gstdio.h>
#else
#define g_lstat(path, buffer) (lstat ((path), (buffer)))
#define g_mkdir(path, mode) (mkdir ((path), (mode)))
#endif

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif



/* modes of xfer operation */
typedef enum
{
  THUNAR_VFS_XFER_COPY,
  THUNAR_VFS_XFER_LINK,
} ThunarVfsXferMode;



static gboolean         tvxc_mounted_readonly           (const gchar          *path);
static void             tvxc_set_error_from_errno       (GError              **error,
                                                         const gchar          *message,
                                                         const gchar          *path);
static ThunarVfsPath   *thunar_vfs_xfer_next_path       (const ThunarVfsPath  *source_path,
                                                         ThunarVfsPath        *target_directory_path,
                                                         guint                 n,
                                                         ThunarVfsXferMode     mode,
                                                         GError              **error);
static inline gboolean  thunar_vfs_xfer_copy_directory  (const gchar          *source_absolute_path,
                                                         const gchar          *target_absolute_path,
                                                         const struct stat    *source_statb,
                                                         ThunarVfsXferCallback callback,
                                                         gpointer              user_data,
                                                         gboolean              merge_directories,
                                                         GError              **error);
static inline gboolean  thunar_vfs_xfer_copy_fifo       (const gchar          *source_absolute_path,
                                                         const gchar          *target_absolute_path,
                                                         const struct stat    *source_statb,
                                                         ThunarVfsXferCallback callback,
                                                         gpointer              user_data,
                                                         GError              **error);
static inline gboolean  thunar_vfs_xfer_copy_regular    (const gchar          *source_absolute_path,
                                                         const gchar          *target_absolute_path,
                                                         const struct stat    *source_statb,
                                                         ThunarVfsXferCallback callback,
                                                         gpointer              user_data,
                                                         GError              **error);
static inline gboolean  thunar_vfs_xfer_copy_symlink    (const gchar          *source_absolute_path,
                                                         const gchar          *target_absolute_path,
                                                         const struct stat    *source_statb,
                                                         ThunarVfsXferCallback callback,
                                                         gpointer              user_data,
                                                         GError              **error);
static gboolean         thunar_vfs_xfer_copy_internal   (const ThunarVfsPath  *source_path,
                                                         ThunarVfsPath        *target_path,
                                                         ThunarVfsXferCallback callback,
                                                         gpointer              user_data,
                                                         gboolean              merge_directories,
                                                         GError              **error);
static inline gboolean  thunar_vfs_xfer_link_internal   (const ThunarVfsPath  *source_path,
                                                         ThunarVfsPath        *target_path,
                                                         GError              **error);



/* reference to the global ThunarVfsMonitor instance */
static ThunarVfsMonitor *thunar_vfs_xfer_monitor = NULL;



static gboolean
tvxc_mounted_readonly (const gchar *path)
{
#if defined(HAVE_STATVFS)
  struct statvfs statvfsb;
  return (statvfs (path, &statvfsb) == 0 && (statvfsb.f_flag & ST_RDONLY) != 0);
#elif defined(HAVE_STATFS)
  struct statfs statfsb;
  return (statfs (path, &statfsb) == 0 && (statfsb.f_flags & MNT_RDONLY) != 0);
#else
  return FALSE;
#endif
}



static void
tvxc_set_error_from_errno (GError     **error,
                           const gchar *message,
                           const gchar *path)
{
  gchar *display_name;
  gchar *format;
  gint   sverrno;

  /* save the errno value, as it may be modified */
  sverrno = errno;

  /* determine the display name for the path */
  display_name = g_filename_display_name (path);

  /* setup the error */
  format = g_strconcat (message, ": %s", NULL);
  g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (sverrno),
               format, display_name, g_strerror (sverrno));
  g_free (format);

  /* release the display name */
  g_free (display_name);
}



/* generates the next duplicate/link name */
static ThunarVfsPath*
thunar_vfs_xfer_next_path (const ThunarVfsPath *source_path,
                           ThunarVfsPath       *target_directory_path,
                           guint                n,
                           ThunarVfsXferMode    mode,
                           GError             **error)
{
  static const gchar * const NAMES[3][2] =
  {
    {
      N_ ("copy of %s"),
      N_ ("link to %s"),
    },
    {
      N_ ("another copy of %s"),
      N_ ("another link to %s"),
    },
    {
      N_ ("third copy of %s"),
      N_ ("third link to %s"),
    },
  };

  ThunarVfsPath *target_path;
  const gchar   *source_name;
  gchar         *source_display_name;
  gchar         *target_display_name;
  gchar         *target_name;
  gchar         *swap_name;
  gchar         *tmp_name;
  guint          m;

  g_return_val_if_fail (n > 0, NULL);

  /* try to determine the display name for the source file (in UTF-8) */
  source_name = thunar_vfs_path_get_name (source_path);
  source_display_name = g_filename_to_utf8 (source_name, -1, NULL, NULL, error);
  if (G_UNLIKELY (source_display_name == NULL))
    return NULL;

  /* check if the source display name matches one of the NAMES (when copying) */
  if (G_LIKELY (mode == THUNAR_VFS_XFER_COPY))
    {
      /* allocate memory for the new name */
      tmp_name = g_strdup (source_display_name);

      /* try the NAMES */
      for (m = 0; m < 3; ++m)
        if (sscanf (source_display_name, gettext (NAMES[m][THUNAR_VFS_XFER_COPY]), tmp_name) == 1)
          {
            /* swap tmp and source display name */
            swap_name = source_display_name;
            source_display_name = tmp_name;
            tmp_name = swap_name;
            break;
          }

      /* if we had no match on the NAMES, try the "%uth copy of %s" pattern */
      if (G_LIKELY (m == 3) && sscanf (source_display_name, _("%uth copy of %s"), &m, tmp_name) == 2)
        {
          /* swap tmp and source display name */
          swap_name = source_display_name;
          source_display_name = tmp_name;
          tmp_name = swap_name;
        }

      /* cleanup */
      g_free (tmp_name);
    }

  /* determine the target display name */
  if (G_LIKELY (n <= 3))
    target_display_name = g_strdup_printf (gettext (NAMES[n - 1][mode]), source_display_name);
  else if (mode == THUNAR_VFS_XFER_COPY)
    target_display_name = g_strdup_printf (ngettext ("%uth copy of %s", "%uth copy of %s", n), n, source_display_name);
  else
    target_display_name = g_strdup_printf (ngettext ("%uth link to %s", "%uth link to %s", n), n, source_display_name);

  /* we don't need the source display name anymore */
  g_free (source_display_name);

  /* try to determine the real target name from the display name */
  target_name = g_filename_from_utf8 (target_display_name, -1, NULL, NULL, error);

  /* generate the target path */
  target_path = (target_name != NULL) ? thunar_vfs_path_relative (target_directory_path, target_name) : NULL;
  g_free (target_display_name);
  g_free (target_name);
  return target_path;
}



static inline gboolean
thunar_vfs_xfer_copy_directory (const gchar          *source_absolute_path,
                                const gchar          *target_absolute_path,
                                const struct stat    *source_statb,
                                ThunarVfsXferCallback callback,
                                gpointer              user_data,
                                gboolean              merge_directories,
                                GError              **error)
{
  mode_t mode;

  /* default to the source dir permissions */
  mode = (source_statb->st_mode & ~S_IFMT);

  /* if the source is located on a rdonly medium or we are not the owner of
   * the source directory, we automatically chmod +rw the destination file.
   */
  if (tvxc_mounted_readonly (source_absolute_path) || source_statb->st_uid != getuid ())
    mode |= (THUNAR_VFS_FILE_MODE_USR_READ | THUNAR_VFS_FILE_MODE_USR_WRITE);

  /* check if the directory exists */
  if (g_file_test (target_absolute_path, G_FILE_TEST_IS_DIR))
    {
      /* return an error if we should not merge */
      if (G_UNLIKELY (!merge_directories))
        {
          errno = EEXIST;
          goto error;
        }
    }
  else if (g_mkdir (target_absolute_path, mode) < 0)
    {
error:
      /* setup the error return */
      tvxc_set_error_from_errno (error, _("Failed to create directory \"%s\""), target_absolute_path);
      return FALSE;
    }

  /* invoke the callback */
  (*callback) (source_statb->st_size, source_statb->st_size, user_data);

  return TRUE;
}



static inline gboolean
thunar_vfs_xfer_copy_fifo (const gchar          *source_absolute_path,
                           const gchar          *target_absolute_path,
                           const struct stat    *source_statb,
                           ThunarVfsXferCallback callback,
                           gpointer              user_data,
                           GError              **error)
{
  /* try to create the named fifo */
  if (mkfifo (target_absolute_path, source_statb->st_mode) < 0)
    {
      /* TRANSLATORS: FIFO is an acronym for First In, First Out. You can replace the word with `pipe'. */
      tvxc_set_error_from_errno (error, _("Failed to create named fifo \"%s\""), target_absolute_path);
      return FALSE;
    }

  /* invoke the callback */
  (*callback) (source_statb->st_size, source_statb->st_size, user_data);

  return TRUE;
}



static inline gboolean
thunar_vfs_xfer_copy_regular (const gchar          *source_absolute_path,
                              const gchar          *target_absolute_path,
                              const struct stat    *source_statb,
                              ThunarVfsXferCallback callback,
                              gpointer              user_data,
                              GError              **error)
{
  gboolean succeed = FALSE;
  mode_t   mode;
  gsize    done;
  gsize    bufsize;
  gchar   *buffer;
  gint     source_fd;
  gint     target_fd;
  gint     n, m, l;

  /* try to open the source file for reading */
  source_fd = open (source_absolute_path, O_RDONLY);
  if (G_UNLIKELY (source_fd < 0))
    {
      tvxc_set_error_from_errno (error, _("Failed to open \"%s\" for reading"), source_absolute_path);
      goto end0;
    }

  /* default to the permissions of the source file */
  mode = source_statb->st_mode;

  /* if the source is located on a rdonly medium or we are not the owner
   * of the source file, we automatically chmod +rw the destination file.
   */
  if (tvxc_mounted_readonly (source_absolute_path) || source_statb->st_uid != getuid ())
    mode |= (THUNAR_VFS_FILE_MODE_USR_READ | THUNAR_VFS_FILE_MODE_USR_WRITE);

  /* try to open the target file for writing */
  target_fd = open (target_absolute_path, O_CREAT | O_EXCL | O_NOFOLLOW | O_WRONLY, mode);
  if (G_UNLIKELY (target_fd < 0))
    {
      /* translate EISDIR, EMLINK and ETXTBSY to EEXIST */
      if (G_UNLIKELY (errno == EISDIR || errno == EMLINK || errno == ETXTBSY))
        errno = EEXIST;

      tvxc_set_error_from_errno (error, _("Failed to open \"%s\" for writing"), target_absolute_path);
      goto end1;
    }

  /* allocate the transfer buffer */
  bufsize = 8 * source_statb->st_blksize;
  buffer = g_new (gchar, bufsize);

  /* copy the data from the source file to the target file */
  for (done = 0; done < source_statb->st_size; )
    {
      /* read a chunk from the source file */
      n = read (source_fd, buffer, bufsize);
      if (G_UNLIKELY (n < 0))
        {
          /* just try again on EAGAIN/EINTR */
          if (G_UNLIKELY (errno == EAGAIN || errno == EINTR))
            continue;

          tvxc_set_error_from_errno (error, _("Failed to read data from \"%s\""), source_absolute_path);
          goto end2;
        }
      else if (n == 0)
        break;

      /* write the data to the target file */
      for (m = 0; m < n; )
        {
          l = write (target_fd, buffer + m, n - m);
          if (G_UNLIKELY (l < 0))
            {
              /* just try again on EAGAIN/EINTR */
              if (G_UNLIKELY (errno == EAGAIN || errno == EINTR))
                continue;

              tvxc_set_error_from_errno (error, _("Failed to write data to \"%s\""), target_absolute_path);
              goto end2;
            }
          else
            {
              /* advance the offset */
              m += l;
            }
        }

      /* advance the byte offset */
      done += n;

      /* invoke the callback (FALSE means cancel) */
      if (!((*callback) (source_statb->st_size, done, user_data)))
        {
          /* unlink the not yet completely written target file */
          if (done < source_statb->st_size && g_unlink (target_absolute_path) < 0)
            {
              tvxc_set_error_from_errno (error, _("Failed to remove \"%s\""), target_absolute_path);
              goto end2;
            }

          /* tell the caller that the job was cancelled */
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INTR, _("Operation canceled"));
          goto end2;
        }
    }

  /* invoke the callback with the fully written data */
  (*callback) (done, done, user_data);

  /* we did it */
  succeed = TRUE;

  /* release the transfer buffer */
end2:
  g_free (buffer);

  /* close the file descriptors */
  close (target_fd);
end1:
  close (source_fd);
end0:
  return succeed;
}



static inline gboolean
thunar_vfs_xfer_copy_symlink (const gchar          *source_absolute_path,
                              const gchar          *target_absolute_path,
                              const struct stat    *source_statb,
                              ThunarVfsXferCallback callback,
                              gpointer              user_data,
                              GError              **error)
{
  gchar link_target[THUNAR_VFS_PATH_MAXSTRLEN * 2 + 1];
  gint  link_target_len;

  /* try to read the link target */
  link_target_len = readlink (source_absolute_path, link_target, sizeof (link_target) - 1);
  if (G_UNLIKELY (link_target_len < 0))
    {
      tvxc_set_error_from_errno (error, _("Failed to read link target from \"%s\""), source_absolute_path);
      return FALSE;
    }
  link_target[link_target_len] = '\0';

  /* try to create the symbolic link */
  if (symlink (link_target, target_absolute_path) < 0)
    {
      tvxc_set_error_from_errno (error, _("Failed to create symbolic link \"%s\""), target_absolute_path);
      return FALSE;
    }

#ifdef HAVE_LCHMOD
  /* apply the file mode which was found for the source */
  if (lchmod (target_absolute_path, source_statb->st_mode) < 0)
    {
      tvxc_set_error_from_errno (error, _("Failed to change mode of \"%s\""), target_absolute_path);
      return FALSE;
    }
#endif

  /* invoke the callback */
  (*callback) (source_statb->st_size, source_statb->st_size, user_data);

  return TRUE;
}



static gboolean
thunar_vfs_xfer_copy_internal (const ThunarVfsPath   *source_path,
                               ThunarVfsPath         *target_path,
                               ThunarVfsXferCallback  callback,
                               gpointer               user_data,
                               gboolean               merge_directories,
                               GError               **error)
{
  struct stat source_statb;
  gboolean    succeed;
  gchar      *display_name;
  gchar       source_absolute_path[THUNAR_VFS_PATH_MAXSTRLEN];
  gchar       target_absolute_path[THUNAR_VFS_PATH_MAXSTRLEN];

  /* determine the absolute path to the source file */
  if (thunar_vfs_path_to_string (source_path, source_absolute_path, sizeof (source_absolute_path), error) < 0)
    return FALSE;

  /* stat the source path */
  if (lstat (source_absolute_path, &source_statb) < 0)
    {
      tvxc_set_error_from_errno (error, _("Failed to determine file info for \"%s\""), source_absolute_path);
      return FALSE;
    }

  /* determine the absolute path to the target file */
  if (thunar_vfs_path_to_string (target_path, target_absolute_path, sizeof (target_absolute_path), error) < 0)
    return FALSE;

  /* perform the appropriate operation */
  switch ((source_statb.st_mode & S_IFMT) >> 12)
    {
    case THUNAR_VFS_FILE_TYPE_DIRECTORY:
      succeed = thunar_vfs_xfer_copy_directory (source_absolute_path, target_absolute_path, &source_statb, callback, user_data, merge_directories, error);
      break;

    case THUNAR_VFS_FILE_TYPE_FIFO:
      succeed = thunar_vfs_xfer_copy_fifo (source_absolute_path, target_absolute_path, &source_statb, callback, user_data, error);
      break;

    case THUNAR_VFS_FILE_TYPE_REGULAR:
      succeed = thunar_vfs_xfer_copy_regular (source_absolute_path, target_absolute_path, &source_statb, callback, user_data, error);
      break;

    case THUNAR_VFS_FILE_TYPE_SYMLINK:
      succeed = thunar_vfs_xfer_copy_symlink (source_absolute_path, target_absolute_path, &source_statb, callback, user_data, error);
      break;

    default:
      display_name = g_filename_display_name (source_absolute_path);
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Failed to copy special file \"%s\""), display_name);
      g_free (display_name);
      return FALSE;
    }

  /* schedule a "created" event if the operation was successfull */
  if (G_LIKELY (succeed))
    thunar_vfs_monitor_feed (thunar_vfs_xfer_monitor, THUNAR_VFS_MONITOR_EVENT_CREATED, target_path);

  /* and that's it */
  return succeed;
}



static inline gboolean
thunar_vfs_xfer_link_internal (const ThunarVfsPath *source_path,
                               ThunarVfsPath       *target_path,
                               GError             **error)
{
  struct stat source_statb;
  gchar       source_absolute_path[THUNAR_VFS_PATH_MAXSTRLEN];
  gchar       target_absolute_path[THUNAR_VFS_PATH_MAXSTRLEN];

  /* determine the absolute path to the source file */
  if (thunar_vfs_path_to_string (source_path, source_absolute_path, sizeof (source_absolute_path), error) < 0)
    return FALSE;

  /* stat the source path */
  if (lstat (source_absolute_path, &source_statb) < 0)
    {
      tvxc_set_error_from_errno (error, _("Failed to determine file info for \"%s\""), source_absolute_path);
      return FALSE;
    }

  /* determine the absolute path to the target file */
  if (thunar_vfs_path_to_string (target_path, target_absolute_path, sizeof (target_absolute_path), error) < 0)
    return FALSE;

  /* try to create the symlink */
  if (symlink (source_absolute_path, target_absolute_path) < 0)
    {
      tvxc_set_error_from_errno (error, _("Failed to create symbolic link \"%s\""), target_absolute_path);
      return FALSE;
    }

  /* feed a "created" event for the target path */
  thunar_vfs_monitor_feed (thunar_vfs_xfer_monitor, THUNAR_VFS_MONITOR_EVENT_CREATED, target_path);

  return TRUE;
}



/**
 * thunar_vfs_xfer_copy:
 * @source_path        : the #ThunarVfsPath to the source file.
 * @target_path        : the #ThunarVfsPath to the target file.
 * @target_path_return : return location for the final target path or %NULL.
 * @callback           : progress callback function.
 * @user_data          : additional user data to pass to @callback.
 * @error              : return location for errors or %NULL.
 *
 * If @target_path_return is not %NULL, the final target path, which
 * may be different than @target_path, is placed in the location
 * pointed to by @target_path_return. The caller is responsible to
 * free the object using thunar_vfs_path_unref(). @target_path_return
 * will only be set if %TRUE is returned.
 *
 * As a special case, if @callback returns %FALSE (which means the
 * operation should be cancelled), this method returns %FALSE and
 * @error is set to #G_FILE_ERROR_INTR.
 *
 * Return value: %FALSE if @error is set, else %TRUE.
 **/
gboolean
thunar_vfs_xfer_copy (const ThunarVfsPath   *source_path,
                      ThunarVfsPath         *target_path,
                      ThunarVfsPath        **target_path_return,
                      ThunarVfsXferCallback  callback,
                      gpointer               user_data,
                      GError               **error)
{
  ThunarVfsPath *target_directory_path;
  ThunarVfsPath *path;
  gboolean       succeed;
  GError        *err = NULL;
  guint          n;

  g_return_val_if_fail (source_path != NULL, FALSE);
  g_return_val_if_fail (target_path != NULL, FALSE);
  g_return_val_if_fail (callback != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* check if source and target are the same */
  if (G_LIKELY (!thunar_vfs_path_equal (source_path, target_path)))
    {
      succeed = thunar_vfs_xfer_copy_internal (source_path, target_path, callback, user_data, TRUE, error);
      if (G_LIKELY (succeed && target_path_return != NULL))
        *target_path_return = thunar_vfs_path_ref (target_path);
      return succeed;
    }

  /* generate a duplicate name for the target path */
  target_directory_path = thunar_vfs_path_get_parent (target_path);
  for (n = 1;; ++n)
    {
      /* try to generate the next duplicate path */
      path = thunar_vfs_xfer_next_path (source_path, target_directory_path, n, THUNAR_VFS_XFER_COPY, &err);
      if (G_UNLIKELY (path == NULL))
        {
          g_propagate_error (error, err);
          return FALSE;
        }

      /* try to copy to the duplicate file */
      succeed = thunar_vfs_xfer_copy_internal (source_path, path, callback, user_data, FALSE, &err);

      /* check if we succeed or cannot continue */
      if (G_LIKELY (succeed || err->domain != G_FILE_ERROR || err->code != G_FILE_ERROR_EXIST))
        {
          if (G_LIKELY (succeed && target_path_return != NULL))
            *target_path_return = path;
          else
            thunar_vfs_path_unref (path);

          if (G_UNLIKELY (error == NULL))
            g_clear_error (&err);
          else
            *error = err;
          return succeed;
        }

      /* and once again... */
      thunar_vfs_path_unref (path);
      g_clear_error (&err);
    }

  /* we never get here */
  g_assert_not_reached ();
  return FALSE;
}



/**
 * thunar_vfs_xfer_link:
 * @source_path        : the #ThunarVfsPath to the source file.
 * @target_path        : the #ThunarVfsPath to the target file.
 * @target_path_return : return location for the final target path or %NULL.
 * @error              : return location for errors or %NULL.
 *
 * If @target_path_return is not %NULL, the final target path, which
 * may be different than @target_path, is placed in the location
 * pointed to by @target_path_return. The caller is responsible to
 * free the object using thunar_vfs_path_unref(). @target_path_return
 * will only be set if %TRUE is returned.
 *
 * Return value: %FALSE if @error is set, else %TRUE.
 **/
gboolean
thunar_vfs_xfer_link (const ThunarVfsPath  *source_path,
                      ThunarVfsPath        *target_path,
                      ThunarVfsPath       **target_path_return,
                      GError              **error)
{
  ThunarVfsPath *target_directory_path;
  ThunarVfsPath *path;
  gboolean       succeed;
  GError        *err = NULL;
  guint          n;

  g_return_val_if_fail (source_path != NULL, FALSE);
  g_return_val_if_fail (target_path != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* check if source and target are the same */
  if (G_LIKELY (!thunar_vfs_path_equal (source_path, target_path)))
    {
      succeed = thunar_vfs_xfer_link_internal (source_path, target_path, error);
      if (G_LIKELY (succeed && target_path_return != NULL))
        *target_path_return = thunar_vfs_path_ref (target_path);
      return succeed;
    }

  /* generate a new link name for the target path */
  target_directory_path = thunar_vfs_path_get_parent (target_path);
  for (n = 1;; ++n)
    {
      /* try to generate the next link path */
      path = thunar_vfs_xfer_next_path (source_path, target_directory_path, n, THUNAR_VFS_XFER_LINK, &err);
      if (G_UNLIKELY (path == NULL))
        {
          g_propagate_error (error, err);
          return FALSE;
        }

      /* try to symlink to the new file */
      succeed = thunar_vfs_xfer_link_internal (source_path, path, &err);

      /* check if we succeed or cannot continue */
      if (G_LIKELY (succeed || err->domain != G_FILE_ERROR || err->code != G_FILE_ERROR_EXIST))
        {
          if (G_LIKELY (succeed && target_path_return != NULL))
            *target_path_return = path;
          else
            thunar_vfs_path_unref (path);

          if (G_UNLIKELY (error == NULL))
            g_clear_error (&err);
          else
            *error = err;
          return succeed;
        }

      /* and once again... */
      thunar_vfs_path_unref (path);
      g_clear_error (&err);
    }

  /* we never get here */
  g_assert_not_reached ();
  return FALSE;
}



/**
 * _thunar_vfs_xfer_init:
 * Initializes the Thunar-VFS Transfer module.
 **/
void
_thunar_vfs_xfer_init (void)
{
  thunar_vfs_xfer_monitor = thunar_vfs_monitor_get_default ();
}



/**
 * _thunar_vfs_xfer_shutdown:
 * Shuts down the Thunar-VFS Transfer module.
 **/
void
_thunar_vfs_xfer_shutdown (void)
{
  g_object_unref (G_OBJECT (thunar_vfs_xfer_monitor));
  thunar_vfs_xfer_monitor = NULL;
}



#define __THUNAR_VFS_XFER_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>

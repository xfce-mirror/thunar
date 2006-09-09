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
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
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
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
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

#include <thunar-vfs/thunar-vfs-io-local-xfer.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>

/* use g_lstat(), g_mkdir(), g_open() and g_unlink() on win32 */
#if defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_lstat(path, buffer) (lstat ((path), (buffer)))
#define g_mkdir(path, mode) (mkdir ((path), (mode)))
#define g_open(path, flags, mode) (open ((path), (flags), (mode)))
#define g_unlink(path) (unlink ((path)))
#endif

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif



static gboolean     tvilx_mounted_readonly    (gint                           fd);
static void         tvilx_set_error_with_path (GError                       **error,
                                               const gchar                   *message,
                                               const gchar                   *path);
static inline void  tvilx_copy_regular        (const gchar                   *source_absolute_path,
                                               const gchar                   *target_absolute_path,
                                               const struct stat             *source_statb,
                                               ThunarVfsIOOpsProgressCallback callback,
                                               gpointer                       callback_data,
                                               GError                       **error);



static gboolean
tvilx_mounted_readonly (gint fd)
{
#if defined(HAVE_STATVFS1)
  struct statvfs statvfsb;
  return (fstatvfs1 (fd, &statvfsb, ST_NOWAIT) == 0 && (statvfsb.f_flag & ST_RDONLY) != 0);
#elif defined(HAVE_STATVFS)
  struct statvfs statvfsb;
  return (fstatvfs (fd, &statvfsb) == 0 && (statvfsb.f_flag & ST_RDONLY) != 0);
#elif defined(HAVE_STATFS)
  struct statfs statfsb;
  return (fstatfs (fd, &statfsb) == 0 && (statfsb.f_flags & MNT_RDONLY) != 0);
#else
  return FALSE;
#endif
}



static void
tvilx_set_error_with_path (GError     **error,
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
  format = g_strconcat (message, " (%s)", NULL);
  g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (sverrno), format, display_name, g_strerror (sverrno));
  g_free (format);

  /* release the display name */
  g_free (display_name);
}



static inline void
tvilx_copy_regular (const gchar                   *source_absolute_path,
                    const gchar                   *target_absolute_path,
                    const struct stat             *source_statb,
                    ThunarVfsIOOpsProgressCallback callback,
                    gpointer                       callback_data,
                    GError                       **error)
{
#ifdef HAVE_FUTIMES
  struct timeval times[2];
#endif
  mode_t         mode;
  gchar         *display_name;
  gchar         *buffer;
  gsize          bufsize;
  gsize          completed;
  gint           source_fd;
  gint           target_fd;
  gint           n, m, l;

  /* try to open the source file for reading */
  source_fd = g_open (source_absolute_path, O_RDONLY, 0000);
  if (G_UNLIKELY (source_fd < 0))
    {
      tvilx_set_error_with_path (error, _("Failed to open \"%s\" for reading"), source_absolute_path);
      return;
    }

  /* default to the permissions of the source file */
  mode = source_statb->st_mode;

  /* if the source is located on a rdonly medium or we are not the owner
   * of the source file, we automatically chmod +rw the destination file.
   */
  if (tvilx_mounted_readonly (source_fd) || source_statb->st_uid != getuid ())
    mode |= (THUNAR_VFS_FILE_MODE_USR_READ | THUNAR_VFS_FILE_MODE_USR_WRITE);

  /* try to open the target file for writing */
  target_fd = g_open (target_absolute_path, O_CREAT | O_EXCL | O_NOFOLLOW | O_WRONLY, mode);
  if (G_UNLIKELY (target_fd < 0))
    {
      /* translate EISDIR and EMLINK to EEXIST */
      if (G_UNLIKELY (errno == EISDIR || errno == EMLINK))
        errno = EEXIST;

      /* EEXIST gets a better error message */
      if (G_LIKELY (errno == EEXIST))
        {
          display_name = g_filename_display_name (target_absolute_path);
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), _("The file \"%s\" already exists"), display_name);
          g_free (display_name);
        }
      else
        {
          /* use the generic error message */
          tvilx_set_error_with_path (error, _("Failed to open \"%s\" for writing"), target_absolute_path);
        }

      goto end1;
    }

#ifdef HAVE_MMAP
  /* try mmap() for files not larger than 8MB */
  buffer = (source_statb->st_size <= 8 * 1024 * 1024)
         ? mmap (NULL, source_statb->st_size, PROT_READ, MAP_SHARED, source_fd, 0)
         : MAP_FAILED;
  if (G_LIKELY (buffer != (gchar *) MAP_FAILED))
    {
#ifdef HAVE_POSIX_MADVISE
      /* tell the system that the data will be read sequentially */
      posix_madvise (buffer, source_statb->st_size, POSIX_MADV_SEQUENTIAL);
#endif

      /* write the data to the target file */
      for (m = 0, n = source_statb->st_size; m < n; )
        {
          l = write (target_fd, buffer + m, n - m);
          if (G_UNLIKELY (l < 0))
            {
              /* just try again on EAGAIN/EINTR */
              if (G_LIKELY (errno != EAGAIN && errno != EINTR))
                {
                  tvilx_set_error_with_path (error, _("Failed to write data to \"%s\""), target_absolute_path);
                  break;
                }
            }
          else
            {
              /* advance the offset */
              m += l;
            }

          /* invoke the callback (FALSE means cancel) */
          if (!((*callback) (m, callback_data)))
            {
              /* unlink the (not yet completely written) target file */
              if (g_unlink (target_absolute_path) < 0)
                {
                  tvilx_set_error_with_path (error, _("Failed to remove \"%s\""), target_absolute_path);
                  break;
                }

              /* tell the caller that the job was cancelled */
              _thunar_vfs_set_g_error_from_errno (error, EINTR);
              break;
            }
        }

      /* need to unmap prior to close */
      munmap (buffer, source_statb->st_size);
    }
  else
#endif /* !HAVE_MMAP */
    {
      /* allocate the transfer buffer */
      bufsize = 8 * source_statb->st_blksize;
      buffer = g_new (gchar, bufsize);

      /* copy the data from the source file to the target file */
      for (completed = 0; completed < source_statb->st_size; )
        {
          /* read a chunk from the source file */
          n = read (source_fd, buffer, bufsize);
          if (G_UNLIKELY (n < 0))
            {
              /* just try again on EAGAIN/EINTR */
              if (G_LIKELY (errno != EAGAIN && errno != EINTR))
                {
                  tvilx_set_error_with_path (error, _("Failed to read data from \"%s\""), source_absolute_path);
                  goto end2;
                }
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

                  tvilx_set_error_with_path (error, _("Failed to write data to \"%s\""), target_absolute_path);
                  goto end2;
                }
              else
                {
                  /* advance the offset */
                  m += l;
                }
            }

          /* invoke the callback (FALSE means cancel) */
          if (!((*callback) (n, callback_data)))
            {
              /* unlink the (not yet completely written) target file */
              if (g_unlink (target_absolute_path) < 0)
                {
                  tvilx_set_error_with_path (error, _("Failed to remove \"%s\""), target_absolute_path);
                  goto end2;
                }

              /* tell the caller that the job was cancelled */
              _thunar_vfs_set_g_error_from_errno (error, EINTR);
              goto end2;
            }
        }

end2: /* release the transfer buffer */
      g_free (buffer);
    }

  /* set the access/modification time of the target to that of
   * the source: http://bugzilla.xfce.org/show_bug.cgi?id=2244
   */
#ifdef HAVE_FUTIMES
  /* set access and modifications using the source_statb */
  times[0].tv_sec = source_statb->st_atime; times[0].tv_usec = 0;
  times[1].tv_sec = source_statb->st_mtime; times[1].tv_usec = 0;

  /* apply the new times to the file (ignoring errors here) */
  futimes (target_fd, times);
#endif

  /* close the file descriptors */
  close (target_fd);
end1:
  close (source_fd);
}



/**
 * _thunar_vfs_io_local_xfer_next_path:
 * @source_path           : the #ThunarVfsPath of the source file.
 * @target_directory_path : the #ThunarVfsPath to the target directory.
 * @n                     : the current step.
 * @mode                  : whether to generate duplicate or link name.
 * @error                 : return location for errors or %NULL.
 *
 * This method generates the next duplicate/link name for the @source_path
 * in @target_directory_path.
 *
 * The caller is responsible to free the returned #ThunarVfsPath using
 * thunar_vfs_path_unref() when no longer needed.
 *
 * Return value: the @n<!---->th unique #ThunarVfsPath for @source_path in
 *               @target_directory_path according to @mode.
 **/
ThunarVfsPath*
_thunar_vfs_io_local_xfer_next_path (const ThunarVfsPath     *source_path,
                                     ThunarVfsPath           *target_directory_path,
                                     guint                    n,
                                     ThunarVfsIOLocalXferMode mode,
                                     GError                 **error)
{
  static const gchar const NAMES[3][2][19] =
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

  _thunar_vfs_return_val_if_fail (n > 0, NULL);

  /* try to determine the display name for the source file (in UTF-8) */
  source_name = thunar_vfs_path_get_name (source_path);
  source_display_name = g_filename_to_utf8 (source_name, -1, NULL, NULL, error);
  if (G_UNLIKELY (source_display_name == NULL))
    return NULL;

  /* check if the source display name matches one of the NAMES (when copying) */
  if (G_LIKELY (mode == THUNAR_VFS_IO_LOCAL_XFER_COPY))
    {
      /* allocate memory for the new name */
      tmp_name = g_strdup (source_display_name);

      /* try the NAMES */
      for (m = 0; m < 3; ++m)
        if (sscanf (source_display_name, gettext (NAMES[m][THUNAR_VFS_IO_LOCAL_XFER_COPY]), tmp_name) == 1)
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
  else if (mode == THUNAR_VFS_IO_LOCAL_XFER_COPY)
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



/**
 * _thunar_vfs_io_local_xfer_copy:
 * @source_path       : the #ThunarVfsPath for the source file.
 * @target_path       : the #ThunarVfsPath for the target location.
 * @callback          : the progress callback function.
 * @callback_data     : data to pass to @callback.
 * @merge_directories : %TRUE to merge directories, which means that EEXIST is not
 *                      returned if a directory should be copied and the directory
 *                      int the target location already exists.
 * @error             : return location for errors or %NULL.
 *
 * Copies the file from @source_path to @target_path.
 *
 * Return value: %TRUE if the file was successfully copied, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_io_local_xfer_copy (const ThunarVfsPath           *source_path,
                                ThunarVfsPath                 *target_path,
                                ThunarVfsIOOpsProgressCallback callback,
                                gpointer                       callback_data,
                                gboolean                       merge_directories,
                                GError                       **error)
{
  struct stat source_statb;
  GError     *err = NULL;
  gchar       source_absolute_path[THUNAR_VFS_PATH_MAXSTRLEN];
  gchar       target_absolute_path[THUNAR_VFS_PATH_MAXSTRLEN];
#ifdef HAVE_SYMLINK
  gchar      *link_target;
#endif

  /* determine the absolute source and target path */
  if (G_UNLIKELY (thunar_vfs_path_to_string (source_path, source_absolute_path, sizeof (source_absolute_path), error) < 0
               || thunar_vfs_path_to_string (target_path, target_absolute_path, sizeof (target_absolute_path), error) < 0))
    {
      /* should not happen, but hey... */
      return FALSE;
    }

  /* stat the source path */
  if (g_lstat (source_absolute_path, &source_statb) < 0)
    {
      /* unable to stat source file, impossible to copy then */
      tvilx_set_error_with_path (&err, _("Failed to determine file info for \"%s\""), source_absolute_path);
    }
  else
    {
      /* perform the appropriate operation */
      switch ((source_statb.st_mode & S_IFMT) >> 12)
        {
        case THUNAR_VFS_FILE_TYPE_DIRECTORY:
          /* check if the directory exists, or we should not merge */
          if (!g_file_test (target_absolute_path, G_FILE_TEST_IS_DIR) || !merge_directories)
            {
              /* default to the source dir permissions, but make sure that we can write to newly created directories */
              mode_t target_mode = (source_statb.st_mode & ~S_IFMT) | THUNAR_VFS_FILE_MODE_USR_READ | THUNAR_VFS_FILE_MODE_USR_WRITE;

              /* try to create the directory */
              if (g_mkdir (target_absolute_path, target_mode) < 0)
                tvilx_set_error_with_path (&err, _("Failed to create directory \"%s\""), target_absolute_path);
            }
          break;

#ifdef HAVE_MKFIFO
        case THUNAR_VFS_FILE_TYPE_FIFO:
          if (mkfifo (target_absolute_path, source_statb.st_mode) < 0)
            {
              /* TRANSLATORS: FIFO is an acronym for First In, First Out. You can replace the word with `pipe'. */
              tvilx_set_error_with_path (&err, _("Failed to create named fifo \"%s\""), target_absolute_path);
            }
          break;
#endif

        case THUNAR_VFS_FILE_TYPE_REGULAR:
          /* copying regular files requires some more work */
          tvilx_copy_regular (source_absolute_path, target_absolute_path, &source_statb, callback, callback_data, &err);
          break;

#ifdef HAVE_SYMLINK
        case THUNAR_VFS_FILE_TYPE_SYMLINK:
          /* try to read the link target */
          link_target = g_file_read_link (source_absolute_path, &err);
          if (G_LIKELY (link_target != NULL))
            {
              /* try to create the symbolic link */
              if (symlink (link_target, target_absolute_path) < 0)
                tvilx_set_error_with_path (&err, _("Failed to create symbolic link \"%s\""), target_absolute_path);

              /* apply the file mode which was found for the source */
#ifdef HAVE_LCHMOD
              else if (lchmod (target_absolute_path, source_statb.st_mode) < 0)
                tvilx_set_error_with_path (&err, _("Failed to change permissions of \"%s\""), target_absolute_path);
#endif
            }
          g_free (link_target);
          break;
#endif

        default:
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Special files cannot be copied"));
          break;
        }

      /* update the progress for all but regular files if we succeed */
      if (err == NULL && !S_ISREG (source_statb.st_mode))
        (*callback) (source_statb.st_size, callback_data);
    }

  /* check if we failed */
  if (G_UNLIKELY (err != NULL))
    {
      /* propagate the error */
      g_propagate_error (error, err);
      return FALSE;
    }

  return TRUE;
}



/**
 * _thunar_vfs_io_local_xfer_link:
 * @source_path : the #ThunarVfsPath to the source file.
 * @target_path : the #ThunarVfsPath to the target location.
 * @error       : return location for errors or %NULL.
 *
 * Creates a new symbolic link in @target_path pointing to the
 * file at the @source_path.
 *
 * Return value: %TRUE if the symlink creation succeed, %FALSE
 *               otherwise.
 **/
gboolean
_thunar_vfs_io_local_xfer_link (const ThunarVfsPath *source_path,
                                ThunarVfsPath       *target_path,
                                GError             **error)
{
#ifdef HAVE_SYMLINK
  struct stat source_statb;
  GError     *err = NULL;
  gchar       source_absolute_path[THUNAR_VFS_PATH_MAXSTRLEN];
  gchar       target_absolute_path[THUNAR_VFS_PATH_MAXSTRLEN];

  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_local (source_path), FALSE);
  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_local (target_path), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* determine the absolute source and target path */
  if (G_UNLIKELY (thunar_vfs_path_to_string (source_path, source_absolute_path, sizeof (source_absolute_path), error) < 0
               || thunar_vfs_path_to_string (target_path, target_absolute_path, sizeof (target_absolute_path), error) < 0))
    {
      /* should not happen, but hey... */
      return FALSE;
    }

  /* stat the source path */
  if (g_lstat (source_absolute_path, &source_statb) < 0)
    {
      /* the file does not exist, don't try to create a symlink then */
      tvilx_set_error_with_path (&err, _("Failed to determine file info for \"%s\""), source_absolute_path);
    }
  else
    {
      /* try to create the symlink */
      if (err == NULL && symlink (source_absolute_path, target_absolute_path) < 0)
        tvilx_set_error_with_path (error, _("Failed to create symbolic link \"%s\""), target_absolute_path);
    }

  /* check if we failed */
  if (G_UNLIKELY (err != NULL))
    {
      /* propagate the error */
      g_propagate_error (error, err);
      return FALSE;
    }

  return TRUE;
#else
  g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Symbolic links are not supported"));
  return FALSE;
#endif
}



#define __THUNAR_VFS_IO_LOCAL_XFER_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>

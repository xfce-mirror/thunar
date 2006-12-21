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

#include <thunar-vfs/thunar-vfs.h>
#include <thunar-vfs/thunar-vfs-deep-count-job.h>
#include <thunar-vfs/thunar-vfs-io-jobs.h>
#include <thunar-vfs/thunar-vfs-io-trash.h>
#include <thunar-vfs/thunar-vfs-job-private.h>
#include <thunar-vfs/thunar-vfs-mime-database-private.h>
#include <thunar-vfs/thunar-vfs-monitor-private.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-simple-job.h>
#include <thunar-vfs/thunar-vfs-transfer-job.h>
#include <thunar-vfs/thunar-vfs-alias.h>



static gint thunar_vfs_ref_count = 0;



/**
 * thunar_vfs_init:
 *
 * Initializes the ThunarVFS library.
 **/
void
thunar_vfs_init (void)
{
  if (g_atomic_int_exchange_and_add (&thunar_vfs_ref_count, 1) == 0)
    {
      /* initialize the GThread system */
      if (!g_thread_supported ())
        g_thread_init (NULL);

      /* ensure any strings get translated properly */
      xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

      /* initialize the path module */
      _thunar_vfs_path_init ();

      /* allocate the shared monitor instance */
      _thunar_vfs_monitor = g_object_new (THUNAR_VFS_TYPE_MONITOR, NULL);

      /* allocate the shared mime database instance */
      _thunar_vfs_mime_database = g_object_new (THUNAR_VFS_TYPE_MIME_DATABASE, NULL);

      /* pre-determine the most important mime types */
      _thunar_vfs_mime_inode_directory = thunar_vfs_mime_database_get_info (_thunar_vfs_mime_database, "inode/directory");
      _thunar_vfs_mime_application_x_desktop = thunar_vfs_mime_database_get_info (_thunar_vfs_mime_database, "application/x-desktop");
      _thunar_vfs_mime_application_x_executable = thunar_vfs_mime_database_get_info (_thunar_vfs_mime_database, "application/x-executable");
      _thunar_vfs_mime_application_x_shellscript = thunar_vfs_mime_database_get_info (_thunar_vfs_mime_database, "application/x-shellscript");
      _thunar_vfs_mime_application_octet_stream = thunar_vfs_mime_database_get_info (_thunar_vfs_mime_database, "application/octet-stream");

      /* initialize the trash subsystem */
      _thunar_vfs_io_trash_init ();

      /* initialize the jobs framework */
      _thunar_vfs_job_init ();
    }
}



/**
 * thunar_vfs_shutdown:
 *
 * Shuts down the ThunarVFS library.
 **/
void
thunar_vfs_shutdown (void)
{
  if (g_atomic_int_dec_and_test (&thunar_vfs_ref_count))
    {
      /* shutdown the jobs framework */
      _thunar_vfs_job_shutdown ();

      /* shutdown the trash subsystem */
      _thunar_vfs_io_trash_shutdown ();

      /* release the mime type references */
      thunar_vfs_mime_info_unref (_thunar_vfs_mime_application_octet_stream);
      thunar_vfs_mime_info_unref (_thunar_vfs_mime_application_x_shellscript);
      thunar_vfs_mime_info_unref (_thunar_vfs_mime_application_x_executable);
      thunar_vfs_mime_info_unref (_thunar_vfs_mime_application_x_desktop);
      thunar_vfs_mime_info_unref (_thunar_vfs_mime_inode_directory);

      /* release the reference on the mime database */
      g_object_unref (G_OBJECT (_thunar_vfs_mime_database));
      _thunar_vfs_mime_database = NULL;

      /* release the reference on the monitor */
      g_object_unref (G_OBJECT (_thunar_vfs_monitor));
      _thunar_vfs_monitor = NULL;

      /* shutdown the path module */
      _thunar_vfs_path_shutdown ();
    }
}



/**
 * thunar_vfs_listdir:
 * @path  : the #ThunarVfsPath for the folder that should be listed.
 * @error : return location for errors or %NULL.
 *
 * Generates a #ThunarVfsJob, which lists the contents of the folder
 * at the specified @path. If the job could not be launched for
 * some reason, %NULL will be returned and @error will be set to
 * point to a #GError describing the cause. Otherwise the newly
 * allocated #ThunarVfsJob will be returned and the caller is
 * responsible to call g_object_unref().
 *
 * Note, that the returned job is launched right away, so you
 * don't need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_listdir (ThunarVfsPath *path,
                    GError       **error)
{
  /* allocate and launch the listdir job */
  return thunar_vfs_simple_job_launch (_thunar_vfs_io_jobs_listdir, 1,
                                       THUNAR_VFS_TYPE_PATH, path);
}



/**
 * thunar_vfs_create_file:
 * @path  : the #ThunarVfsPath of the file to create.
 * @error : return location for errors or %NULL.
 *
 * Allocates a new #ThunarVfsJob, which creates a new empty
 * file at @path.
 *
 * The caller is responsible to free the returned job using
 * g_object_unref() when no longer needed.
 *
 * Note that the returned job is launched right away, so you
 * don't need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_create_file (ThunarVfsPath *path,
                        GError       **error)
{
  GList path_list;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* fake a path list */
  path_list.data = path;
  path_list.next = NULL;
  path_list.prev = NULL;

  /* allocate the job */
  return thunar_vfs_create_files (&path_list, error);
}



/**
 * thunar_vfs_create_files:
 * @path_list : a list of #ThunarVfsPath<!---->s for files to create.
 * @error     : return location for errors or %NULL.
 *
 * Allocates a new #ThunarVfsJob which creates new empty files for all
 * #ThunarVfsPath<!---->s in @path_list.
 *
 * The caller is responsible to free the returned job using
 * g_object_unref() when no longer needed.
 *
 * Note that the returned job is launched right away, so you
 * don't need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_create_files (GList   *path_list,
                         GError **error)
{
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* verify that we have only local paths here */
  if (!_thunar_vfs_check_only_local (path_list, error))
    return NULL;

  /* allocate and launch the create job */
  return thunar_vfs_simple_job_launch (_thunar_vfs_io_jobs_create, 1,
                                       THUNAR_VFS_TYPE_PATH_LIST, path_list);
}



/**
 * thunar_vfs_copy_file:
 * @source_path : the source #ThunarVfsPath.
 * @target_path : the target #ThunarVfsPath.
 * @error       : return location for errors or %NULL.
 *
 * Allocates a new #ThunarVfsTransferJob, which copies the file
 * from @source_path to @target_path. That said, the file or directory
 * located at @source_path will be placed at @target_path, NOT INTO
 * @target_path.
 *
 * The caller is responsible to free the returned job using
 * g_object_unref() when no longer needed.
 *
 * Note, that the returned job is launched right away, so you don't
 * need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsTransferJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_copy_file (ThunarVfsPath *source_path,
                      ThunarVfsPath *target_path,
                      GError       **error)
{
  GList source_path_list;
  GList target_path_list;

  g_return_val_if_fail (source_path != NULL, NULL);
  g_return_val_if_fail (target_path != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* fake a source path list */
  source_path_list.data = source_path;
  source_path_list.next = NULL;
  source_path_list.prev = NULL;

  /* fake a target path list */
  target_path_list.data = target_path;
  target_path_list.next = NULL;
  target_path_list.prev = NULL;

  /* allocate the job */
  return thunar_vfs_copy_files (&source_path_list, &target_path_list, error);
}



/**
 * thunar_vfs_copy_files:
 * @source_path_list : the list of #ThunarVfsPath<!---->s that should be copied.
 * @target_path_list : the list of #ThunarVfsPath<!---->s for the targets.
 * @error            : return location for errors or %NULL.
 *
 * Similar to thunar_vfs_copy_file(), but takes a bunch of files. The
 * @source_path_list and @target_path_list must be of the same size.
 *
 * Note, that the returned job is launched right away, so you don't
 * need to call thunar_vfs_job_launch() on it. The caller is responsible
 * to free the returned object using g_object_unref() when no longer
 * needed.
 *
 * Return value: the newly allocated #ThunarVfsTransferJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_copy_files (GList   *source_path_list,
                       GList   *target_path_list,
                       GError **error)
{
  ThunarVfsJob *job;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate/launch the job */
  job = thunar_vfs_transfer_job_new (source_path_list, target_path_list, FALSE, error);
  if (G_LIKELY (job != NULL))
    thunar_vfs_job_launch (job);

  return job;
}



/**
 * thunar_vfs_link_file:
 * @source_path : the source #ThunarVfsPath.
 * @target_path : the target #ThunarVfsPath.
 * @error       : return location for errors or %NULL.
 *
 * Allocates a new #ThunarVfsJob, which creates a symbolic
 * link from @source_path to @target_path.
 *
 * If @source_path and @target_path refer to the same file,
 * a new unique target filename will be choosen automatically.
 *
 * The caller is responsible to free the returned job using
 * g_object_unref() when no longer needed.
 *
 * Note, that the returned job is launched right away, so you don't
 * need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_link_file (ThunarVfsPath *source_path,
                      ThunarVfsPath *target_path,
                      GError       **error)
{
  GList source_path_list;
  GList target_path_list;

  g_return_val_if_fail (!thunar_vfs_path_is_root (source_path), NULL);
  g_return_val_if_fail (!thunar_vfs_path_is_root (target_path), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* fake a source path list */
  source_path_list.data = source_path;
  source_path_list.next = NULL;
  source_path_list.prev = NULL;

  /* fake a target path list */
  target_path_list.data = target_path;
  target_path_list.next = NULL;
  target_path_list.prev = NULL;

  return thunar_vfs_link_files (&source_path_list, &target_path_list, error);
}



/**
 * thunar_vfs_link_files:
 * @source_path_list : list of #ThunarVfsPath<!---->s to the source files.
 * @target_path_list : list of #ThunarVfsPath<!---->s to the target files.
 * @error            : return location for errors or %NULL.
 *
 * Like thunar_vfs_link_file(), but works on path lists, rather than a single
 * path. The length of the @source_path_list and @target_path_list must match,
 * otherwise the behaviour is undefined, but its likely to crash the application.
 *
 * Right now links can only be created from local files to local files (with
 * path scheme %THUNAR_VFS_PATH_SCHEME_FILE).
 *
 * The caller is responsible to free the returned job using
 * g_object_unref() when no longer needed.
 *
 * Note, that the returned job is launched right away, so you don't
 * need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_link_files (GList   *source_path_list,
                       GList   *target_path_list,
                       GError **error)
{
  g_return_val_if_fail (g_list_length (source_path_list) == g_list_length (target_path_list), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate and launch the job */
  return thunar_vfs_simple_job_launch (_thunar_vfs_io_jobs_link, 2,
                                       THUNAR_VFS_TYPE_PATH_LIST, source_path_list,
                                       THUNAR_VFS_TYPE_PATH_LIST, target_path_list);
}



/**
 * thunar_vfs_move_file:
 * @source_path : the source #ThunarVfsPath.
 * @target_path : the target #ThunarVfsPath.
 * @error       : return location for errors or %NULL.
 *
 * Allocates a new #ThunarVfsTransferJob, which moves the file
 * from @source_path to @target_path. That said, the file or directory
 * located at @source_path will be placed at @target_path, NOT INTO
 * @target_path.
 *
 * The caller is responsible to free the returned job using
 * g_object_unref() when no longer needed.
 *
 * Note, that the returned job is launched right away, so you don't
 * need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsTransferJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_move_file (ThunarVfsPath *source_path,
                      ThunarVfsPath *target_path,
                      GError       **error)
{
  GList source_path_list;
  GList target_path_list;

  g_return_val_if_fail (source_path != NULL, NULL);
  g_return_val_if_fail (target_path != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* fake a source path list */
  source_path_list.data = source_path;
  source_path_list.next = NULL;
  source_path_list.prev = NULL;

  /* fake a target path list */
  target_path_list.data = target_path;
  target_path_list.next = NULL;
  target_path_list.prev = NULL;

  /* allocate and launch the job */
  return thunar_vfs_move_files (&source_path_list, &target_path_list, error);
}



/**
 * thunar_vfs_move_files:
 * @source_path_list : the list of #ThunarVfsPath<!---->s that should be moved.
 * @target_path_list : the list of #ThunarVfsPath<!---->s to the targets.
 * @error            : return location for errors or %NULL.
 *
 * Similar to thunar_vfs_move_file(), but takes a bunch of files. The
 * @source_path_list and @target_path_list must be of the same size.
 *
 * Note, that the returned job is launched right away, so you don't
 * need to call thunar_vfs_job_launch() on it. The caller is responsible
 * to free the returned object using g_object_unref() when no longer
 * needed.
 *
 * Return value: the newly allocated #ThunarVfsTransferJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_move_files (GList   *source_path_list,
                       GList   *target_path_list,
                       GError **error)
{
  ThunarVfsJob *job;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate/launch the job */
  job = thunar_vfs_transfer_job_new (source_path_list, target_path_list, TRUE, error);
  if (G_LIKELY (job != NULL))
    thunar_vfs_job_launch (job);

  return job;
}



/**
 * thunar_vfs_unlink_file:
 * @path  : a #ThunarVfsPath, that should be unlinked.
 * @error : return location for errors or %NULL.
 *
 * Simple wrapper to thunar_vfs_unlink_files(), which takes
 * only a single path.
 *
 * Note, that the returned job is launched right away, so you
 * don't need to call thunar_vfs_job_launch() on it. The caller
 * is responsible to free the returned object using g_object_unref()
 * when no longer needed.
 *
 * Return value: the newly allocated #ThunarVfsJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_unlink_file (ThunarVfsPath *path,
                        GError       **error)
{
  GList path_list;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* fake a path list */
  path_list.data = path;
  path_list.next = NULL;
  path_list.prev = NULL;

  /* allocate and launch the job */
  return thunar_vfs_unlink_files (&path_list, error);
}



/**
 * thunar_vfs_unlink_files:
 * @path_list : a list of #ThunarVfsPath<!---->s, that should be unlinked.
 * @error     : return location for errors or %NULL.
 *
 * Allocates a new #ThunarVfsJob which recursively unlinks all files
 * referenced by the @path_list. If the job cannot be launched for
 * some reason, %NULL will be returned and @error will be set to point to
 * a #GError describing the cause. Else, the newly allocated #ThunarVfsJob
 * will be returned, and the caller is responsible to free it using
 * g_object_unref() when no longer needed.
 *
 * Note, that the returned job is launched right away, so you
 * don't need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_unlink_files (GList   *path_list,
                         GError **error)
{
  return thunar_vfs_simple_job_launch (_thunar_vfs_io_jobs_unlink, 1,
                                       THUNAR_VFS_TYPE_PATH_LIST, path_list);
}



/**
 * thunar_vfs_make_directory:
 * @path  : the #ThunarVfsPath to the directory to create.
 * @error : return location for errors or %NULL.
 *
 * Simple wrapper for thunar_vfs_make_directories().
 *
 * Return value: the newly allocated #ThunarVfsJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_make_directory (ThunarVfsPath *path,
                           GError       **error)
{
  GList path_list;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* fake a path list */
  path_list.data = path;
  path_list.next = NULL;
  path_list.prev = NULL;

  /* allocate and launch the job */
  return thunar_vfs_make_directories (&path_list, error);
}



/**
 * thunar_vfs_make_directories:
 * @path_list : a list of #ThunarVfsPath<!---->s that contain the paths
 *              to the directories which should be created.
 * @error     : return location for errors or %NULL.
 *
 * Allocates a new #ThunarVfsJob to create new directories at all
 * #ThunarVfsPath<!---->s specified in @path_list. Returns %NULL if
 * the job could not be launched for some reason, and @error will be
 * set to point to a #GError describing the cause. Otherwise the
 * job will be returned and the caller is responsible to free the
 * returned object using g_object_unref() when no longer needed.
 *
 * Note, that the returned job is launched right away, so you don't
 * need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_make_directories (GList   *path_list,
                             GError **error)
{
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* verify that we have only local paths here */
  if (!_thunar_vfs_check_only_local (path_list, error))
    return NULL;

  /* allocate and launch the mkdir job */
  return thunar_vfs_simple_job_launch (_thunar_vfs_io_jobs_mkdir, 1,
                                       THUNAR_VFS_TYPE_PATH_LIST, path_list);
}



/**
 * thunar_vfs_change_mode:
 * @path      : the base #ThunarVfsPath.
 * @dir_mask  : the mask for the @dir_mode.
 * @dir_mode  : the new mode for directories.
 * @file_mask : the mask for the @file_mode.
 * @file_mode : the new mode for files.
 * @recursive : whether to change permissions recursively.
 * @error     : return location for errors or %NULL.
 *
 * The caller is responsible to free the returned job using
 * g_object_unref() when no longer needed.
 *
 * Note, that the returned job is launched right away, so you don't
 * need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_change_mode (ThunarVfsPath    *path,
                        ThunarVfsFileMode dir_mask,
                        ThunarVfsFileMode dir_mode,
                        ThunarVfsFileMode file_mask,
                        ThunarVfsFileMode file_mode,
                        gboolean          recursive,
                        GError          **error)
{
  GList path_list;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* verify that we have only local paths here */
  if (G_UNLIKELY (!_thunar_vfs_path_is_local (path)))
    {
      _thunar_vfs_set_g_error_not_supported (error);
      return NULL;
    }

  /* fake a path list */
  path_list.data = path;
  path_list.next = NULL;
  path_list.prev = NULL;

  /* allocate and launch the new job */
  return thunar_vfs_simple_job_launch (_thunar_vfs_io_jobs_chmod, 6,
                                       THUNAR_VFS_TYPE_PATH_LIST, &path_list,
                                       THUNAR_VFS_TYPE_VFS_FILE_MODE, dir_mask,
                                       THUNAR_VFS_TYPE_VFS_FILE_MODE, dir_mode,
                                       THUNAR_VFS_TYPE_VFS_FILE_MODE, file_mask,
                                       THUNAR_VFS_TYPE_VFS_FILE_MODE, file_mode,
                                       G_TYPE_BOOLEAN, recursive);
}



/**
 * thunar_vfs_change_group:
 * @path      : the base #ThunarVfsPath.
 * @gid       : the new group id.
 * @recursive : whether to change groups recursively.
 * @error     : return location for errors or %NULL.
 *
 * The caller is responsible to free the returned job using
 * g_object_unref() when no longer needed.
 *
 * Note, that the returned job is launched right away, so you don't
 * need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_change_group (ThunarVfsPath   *path,
                         ThunarVfsGroupId gid,
                         gboolean         recursive,
                         GError         **error)
{
  GList path_list;

  g_return_val_if_fail (gid >= 0, NULL);
  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* verify that we have only local paths here */
  if (G_UNLIKELY (!_thunar_vfs_path_is_local (path)))
    {
      _thunar_vfs_set_g_error_not_supported (error);
      return NULL;
    }

  /* fake a path list */
  path_list.data = path;
  path_list.next = NULL;
  path_list.prev = NULL;

  /* allocate and launch the new job */
  return thunar_vfs_simple_job_launch (_thunar_vfs_io_jobs_chown, 4,
                                       THUNAR_VFS_TYPE_PATH_LIST, &path_list,
                                       G_TYPE_INT, (gint) -1,
                                       G_TYPE_INT, (gint) gid,
                                       G_TYPE_BOOLEAN, recursive);
}



/**
 * thunar_vfs_change_owner:
 * @path      : the base #ThunarVfsPath.
 * @uid       : the new user id.
 * @recursive : whether to change groups recursively.
 * @error     : return location for errors or %NULL.
 *
 * The caller is responsible to free the returned job using
 * g_object_unref() when no longer needed.
 *
 * Note, that the returned job is launched right away, so you don't
 * need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsChownJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_change_owner (ThunarVfsPath  *path,
                         ThunarVfsUserId uid,
                         gboolean        recursive,
                         GError        **error)
{
  GList path_list;

  g_return_val_if_fail (uid >= 0, NULL);
  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* verify that we have only local paths here */
  if (G_UNLIKELY (!_thunar_vfs_path_is_local (path)))
    {
      _thunar_vfs_set_g_error_not_supported (error);
      return NULL;
    }

  /* fake a path list */
  path_list.data = path;
  path_list.next = NULL;
  path_list.prev = NULL;

  /* allocate and launch the new job */
  return thunar_vfs_simple_job_launch (_thunar_vfs_io_jobs_chown, 4,
                                       THUNAR_VFS_TYPE_PATH_LIST, &path_list,
                                       G_TYPE_INT, (gint) uid,
                                       G_TYPE_INT, (gint) -1,
                                       G_TYPE_BOOLEAN, recursive);
}



/**
 * thunar_vfs_deep_count:
 * @path  : the base #ThunarVfsPath.
 * @flags : the #ThunarVfsDeepCountFlags which control the behaviour
 *          of the returned job.
 * @error : return location for errors or %NULL.
 *
 * Starts a #ThunarVfsJob, which will count the number of items
 * in the directory specified by @path and also determine the
 * total size. If @path is not a directory, then the size of the
 * item at @path will be determined.
 *
 * The caller is responsible to free the returned job using
 * g_object_unref() when no longer needed.
 *
 * Note, that the returned job is launched right away, so you don't
 * need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsDeepCountJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_deep_count (ThunarVfsPath          *path,
                       ThunarVfsDeepCountFlags flags,
                       GError                **error)
{
  ThunarVfsJob *job;
  
  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate and launch the new job */
  job = thunar_vfs_deep_count_job_new (path, flags);
  if (G_LIKELY (job != NULL))
    thunar_vfs_job_launch (job);

  return job;
}



#define __THUNAR_VFS_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>

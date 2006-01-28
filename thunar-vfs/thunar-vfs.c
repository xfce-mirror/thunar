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
#include <thunar-vfs/thunar-vfs-chmod-job.h>
#include <thunar-vfs/thunar-vfs-chown-job.h>
#include <thunar-vfs/thunar-vfs-creat-job.h>
#include <thunar-vfs/thunar-vfs-link-job.h>
#include <thunar-vfs/thunar-vfs-listdir-job.h>
#include <thunar-vfs/thunar-vfs-mkdir-job.h>
#include <thunar-vfs/thunar-vfs-transfer-job.h>
#include <thunar-vfs/thunar-vfs-unlink-job.h>
#include <thunar-vfs/thunar-vfs-xfer.h>
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
      /* initialize the path module */
      _thunar_vfs_path_init ();

      /* initialize the xfer module */
      _thunar_vfs_xfer_init ();

      /* initialize the info module */
      _thunar_vfs_info_init ();

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

      /* release the info module */
      _thunar_vfs_info_shutdown ();

      /* shutdown the xfer module */
      _thunar_vfs_xfer_shutdown ();

      /* shutdown the path module */
      _thunar_vfs_path_shutdown ();
    }
}



/**
 * thunar_vfs_listdir:
 * @path  : the #ThunarVfsPath for the folder that should be listed.
 * @error : return location for errors or %NULL.
 *
 * Generates a #ThunarVfsListdirJob, which can be used to list the
 * contents of a directory (as specified by @path). If the creation
 * of the job failes for some reason, %NULL will be returned and
 * @error will be set to point to a #GError describing the cause.
 * Else the newly allocated #ThunarVfsListdirJob will be returned
 * and the caller is responsible to call g_object_unref().
 *
 * Note, that the returned job is launched right away, so you
 * don't need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsListdirJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_listdir (ThunarVfsPath *path,
                    GError       **error)
{
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate the job */
  return thunar_vfs_job_launch (thunar_vfs_listdir_job_new (path));
}



/**
 * thunar_vfs_create_file:
 * @path  : the #ThunarVfsPath of the file to create.
 * @error : return location for errors or %NULL.
 *
 * Allocates a new #ThunarVfsCreateJob, which creates a new 
 * file at @path.
 *
 * The caller is responsible to free the returned job using
 * g_object_unref() when no longer needed.
 *
 * Note that the returned job is launched right away, so you
 * don't need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsCreateJob or
 *               %NULL if an error occurs while creating the job.
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
 * Allocates a new #ThunarVfsCreateJob, which creates new 
 * files for all #ThunarVfsPath<!---->s in @path_list.
 *
 * The caller is responsible to free the returned job using
 * g_object_unref() when no longer needed.
 *
 * Note that the returned job is launched right away, so you
 * don't need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsCreateJob or
 *               %NULL if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_create_files (GList   *path_list,
                         GError **error)
{
  ThunarVfsJob *job;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate/launch the job */
  job = thunar_vfs_creat_job_new (path_list, error);
  if (G_LIKELY (job != NULL))
    thunar_vfs_job_launch (job);

  return job;
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
 * Allocates a new #ThunarVfsLinkJob, which creates a symbolic
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
 * Return value: the newly allocated #ThunarVfsLinkJob or %NULL
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
 * Like thunar_vfs_link_file(), but works on path lists, rather than
 * a single path.
 *
 * The caller is responsible to free the returned job using
 * g_object_unref() when no longer needed.
 *
 * Note, that the returned job is launched right away, so you don't
 * need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsLinkJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_link_files (GList   *source_path_list,
                       GList   *target_path_list,
                       GError **error)
{
  ThunarVfsJob *job;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate/launch the job */
  job = thunar_vfs_link_job_new (source_path_list, target_path_list, error);
  if (G_LIKELY (job != NULL))
    thunar_vfs_job_launch (job);

  return job;
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
 * thunar_vfs_move_into:
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
 * Return value: the newly allocated #ThunarVfsUnlinkJob or %NULL
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
 * Generates a #ThunarVfsInteractiveJob, which can be used to unlink
 * all files referenced by the @path_list. If the creation of the job
 * failes for some reason, %NULL will be returned and @error will
 * be set to point to a #GError describing the cause. Else, the
 * newly allocated #ThunarVfsUnlinkJob will be returned, and the
 * caller is responsible to call g_object_unref().
 *
 * Note, that the returned job is launched right away, so you
 * don't need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsUnlinkJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_unlink_files (GList   *path_list,
                         GError **error)
{
  ThunarVfsJob *job;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* try to allocate the job */
  job = thunar_vfs_unlink_job_new (path_list, error);
  if (G_LIKELY (job != NULL))
    thunar_vfs_job_launch (job);

  return job;
}



/**
 * thunar_vfs_make_directory:
 * @path  : the #ThunarVfsPath to the directory to create.
 * @error : return location for errors or %NULL.
 *
 * Generates a #ThunarVfsMkdirJob, which can be used to
 * asynchronously create a new directory at the given @path. If
 * the creation of the job fails for some reason, %NULL will be
 * returned and @error will be set to point to a #GError
 * describing the cause of the problem. Else the newly allocated
 * #ThunarVfsMkdirJob will be returned, and the caller is responsible
 * to call g_object_unref().
 *
 * Note, that the returned job is launched right away, so you
 * don't need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsMkdirJob or %NULL
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
 * @path_list : a list of #ThunarVfsPath<!--->s that contain the paths
 *              to the directories which should be created.
 * @error     : return location for errors or %NULL.
 *
 * Similar to thunar_vfs_make_directory(), but allows the creation
 * of multiple directories using a single #ThunarVfsJob.
 *
 * The caller is responsible to free the returned job using
 * g_object_unref() when no longer needed.
 *
 * Note, that the returned job is launched right away, so you don't
 * need to call thunar_vfs_job_launch() on it.
 *
 * Return value: the newly allocated #ThunarVfsMkdirJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_make_directories (GList   *path_list,
                             GError **error)
{
  ThunarVfsJob *job;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate and launch the new job */
  job = thunar_vfs_mkdir_job_new (path_list, error);
  if (G_LIKELY (job != NULL))
    thunar_vfs_job_launch (job);

  return job;
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
 * Return value: the newly allocated #ThunarVfsChmodJob or %NULL
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
  ThunarVfsJob *job;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate and launch the new job */
  job = thunar_vfs_chmod_job_new (path, dir_mask, dir_mode, file_mask, file_mode, recursive, error);
  if (G_LIKELY (job != NULL))
    thunar_vfs_job_launch (job);

  return job;
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
 * Return value: the newly allocated #ThunarVfsChownJob or %NULL
 *               if an error occurs while creating the job.
 **/
ThunarVfsJob*
thunar_vfs_change_group (ThunarVfsPath   *path,
                         ThunarVfsGroupId gid,
                         gboolean         recursive,
                         GError         **error)
{
  ThunarVfsJob *job;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail ((gint) gid >= 0, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate and launch the new job */
  job = thunar_vfs_chown_job_new (path, -1, gid, recursive, error);
  if (G_LIKELY (job != NULL))
    thunar_vfs_job_launch (job);

  return job;
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
  ThunarVfsJob *job;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail ((gint) uid >= 0, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* allocate and launch the new job */
  job = thunar_vfs_chown_job_new (path, uid, -1, recursive, error);
  if (G_LIKELY (job != NULL))
    thunar_vfs_job_launch (job);

  return job;
}



#define __THUNAR_VFS_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>

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
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-enum-types.h>
#include <thunar-vfs/thunar-vfs-info.h>
#include <thunar-vfs/thunar-vfs-io-jobs.h>
#include <thunar-vfs/thunar-vfs-io-local.h>
#include <thunar-vfs/thunar-vfs-io-ops.h>
#include <thunar-vfs/thunar-vfs-io-scandir.h>
#include <thunar-vfs/thunar-vfs-io-trash.h>
#include <thunar-vfs/thunar-vfs-monitor-private.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>

/* use g_chmod(), g_open() and g_stat() on win32 */
#if defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_chmod(path, mode) (chmod ((path), (mode)))
#define g_open(path, flags, mode) (open ((path), (flags), (mode)))
#define g_stat(path, statb) (stat ((path), (statb)))
#endif



static GList *tvij_collect_nofollow (ThunarVfsJob *job,
                                     GList        *base_path_list,
                                     GError      **error) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;



static GList*
tvij_collect_nofollow (ThunarVfsJob *job,
                       GList        *base_path_list,
                       GError      **error)
{
  GError *err = NULL;
  GList  *child_path_list;
  GList  *path_list = NULL;
  GList  *lp;

  /* tell the user that we're preparing to unlink the files */
  _thunar_vfs_job_info_message (job, _("Preparing..."));

  /* recursively collect the paths */
  for (lp = base_path_list; err == NULL && lp != NULL && !thunar_vfs_job_cancelled (job); lp = lp->next)
    {
      /* try to scan the path as directory */
      child_path_list = _thunar_vfs_io_scandir (lp->data, &job->cancelled, THUNAR_VFS_IO_SCANDIR_RECURSIVE, &err);
      if (G_UNLIKELY (err != NULL))
        {
          /* we can safely ignore ENOENT/ENOTDIR errors here */
          if (err->domain == G_FILE_ERROR && (err->code == G_FILE_ERROR_NOENT || err->code == G_FILE_ERROR_NOTDIR))
            {
              /* reset the error */
              g_clear_error (&err);
            }
        }

      /* prepend the new paths to the existing list */
      path_list = thunar_vfs_path_list_prepend (path_list, lp->data);
      path_list = g_list_concat (child_path_list, path_list);
    }

  /* check if we failed */
  if (G_UNLIKELY (err != NULL))
    {
      /* release the collected paths */
      thunar_vfs_path_list_free (path_list);

      /* propagate the error */
      g_propagate_error (error, err);
      return NULL;
    }

  return path_list;
}



/**
 * _thunar_vfs_io_jobs_chmod:
 * @job            : a #ThunarVfsJob.
 * @param_values   : exactly six #GValue with the #GList of #ThunarVfsPath<!---->s
 *                   for the files whose mode to changed, the directory mode mask,
 *                   the directory mode, the file mask, the file mode and a boolean
 *                   flag telling whether to recursively process directories.
 * @n_param_values : the number of #GValue<!---->s in @param_values, must be exactly
 *                   six for this job.
 * @error          : return location for errors or %NULL.
 *
 * See thunar_vfs_change_mode() for details.
 *
 * The #ThunarVfsPath<!---->s in the first item of @param_values may only include local
 * paths with scheme %THUNAR_VFS_PATH_SCHEME_FILE.
 *
 * Return value: %TRUE if the changing of permissions succeed, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_io_jobs_chmod (ThunarVfsJob *job,
                           const GValue *param_values,
                           guint         n_param_values,
                           GError      **error)
{
  ThunarVfsFileMode file_mask = g_value_get_flags (&param_values[3]);
  ThunarVfsFileMode file_mode = g_value_get_flags (&param_values[4]);
  ThunarVfsFileMode dir_mask = g_value_get_flags (&param_values[1]);
  ThunarVfsFileMode dir_mode = g_value_get_flags (&param_values[2]);
  ThunarVfsFileMode mask;
  ThunarVfsFileMode mode;
  struct stat       statb;
  gboolean          recursive = g_value_get_boolean (&param_values[5]);
  GError           *err = NULL;
  GList            *path_list = g_value_get_boxed (&param_values[0]);
  GList            *lp;
  gchar            *absolute_path;
  gchar            *display_name;
  gint              sverrno;

  _thunar_vfs_return_val_if_fail (G_VALUE_HOLDS (&param_values[1], THUNAR_VFS_TYPE_VFS_FILE_MODE), FALSE);
  _thunar_vfs_return_val_if_fail (G_VALUE_HOLDS (&param_values[2], THUNAR_VFS_TYPE_VFS_FILE_MODE), FALSE);
  _thunar_vfs_return_val_if_fail (G_VALUE_HOLDS (&param_values[3], THUNAR_VFS_TYPE_VFS_FILE_MODE), FALSE);
  _thunar_vfs_return_val_if_fail (G_VALUE_HOLDS (&param_values[4], THUNAR_VFS_TYPE_VFS_FILE_MODE), FALSE);
  _thunar_vfs_return_val_if_fail (G_VALUE_HOLDS (&param_values[0], THUNAR_VFS_TYPE_PATH_LIST), FALSE);
  _thunar_vfs_return_val_if_fail (G_VALUE_HOLDS (&param_values[5], G_TYPE_BOOLEAN), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_vfs_return_val_if_fail (THUNAR_VFS_IS_JOB (job), FALSE);
  _thunar_vfs_return_val_if_fail (n_param_values == 6, FALSE);

#ifdef G_ENABLE_DEBUG
  /* verify that we have only local paths */
  for (lp = path_list; lp != NULL; lp = lp->next)
    _thunar_vfs_assert (_thunar_vfs_path_is_local (lp->data));
#endif

  /* collect the paths for the chmod operation */
  path_list = recursive ? tvij_collect_nofollow (job, path_list, &err) : thunar_vfs_path_list_copy (path_list);
  if (G_UNLIKELY (err != NULL))
    {
      /* propagate the error */
      g_propagate_error (error, err);
      return FALSE;
    }

  /* we know the total list of paths to process now */
  _thunar_vfs_job_total_paths (job, path_list);

  /* change the permissions of all paths */
  for (lp = path_list; lp != NULL && !thunar_vfs_job_cancelled (job); lp = lp->next)
    {
      /* reset the saved errno */
      sverrno = 0;

      /* update the progress information */
      _thunar_vfs_job_process_path (job, lp);

      /* try to stat the file */
      absolute_path = thunar_vfs_path_dup_string (lp->data);
      if (g_stat (absolute_path, &statb) == 0)
        {
          /* different actions depending on the type of the file */
          mask = S_ISDIR (statb.st_mode) ? dir_mask : file_mask;
          mode = S_ISDIR (statb.st_mode) ? dir_mode : file_mode;

          /* determine the new mode */
          mode = ((statb.st_mode & ~mask) | mode) & 07777;

          /* try to apply the new mode */
          if (g_chmod (absolute_path, mode) < 0)
            goto sverror;

          /* feed a change notification event */
          thunar_vfs_monitor_feed (_thunar_vfs_monitor, THUNAR_VFS_MONITOR_EVENT_CHANGED, lp->data);
        }
      else
        {
sverror:
          /* save the errno */
          sverrno = errno;
        }
      g_free (absolute_path);

      /* check if we failed (ignoring ENOENT) */
      if (G_UNLIKELY (sverrno != 0 && sverrno != ENOENT))
        {
          /* generate an error */
          display_name = _thunar_vfs_path_dup_display_name (lp->data);
          _thunar_vfs_set_g_error_from_errno2 (&err, sverrno, _("Failed to change permissions of \"%s\""), display_name);
          g_free (display_name);

          /* ask the user whether to skip this one */
          _thunar_vfs_job_ask_skip (job, "%s", err->message);

          /* reset the error */
          g_clear_error (&err);
        }
    }

  /* release the path list */
  thunar_vfs_path_list_free (path_list);

  return TRUE;
}



/**
 * _thunar_vfs_io_jobs_chown:
 * @job            : a #ThunarVfsJob.
 * @param_values   : exactly four #GValue with the #GList of #ThunarVfsPath<!---->s
 *                   for the files whose ownership to changed, the new user id or %-1,
 *                   the new group id or %-1, and a boolean flag telling whether to
 *                   recursively process directories.
 * @n_param_values : the number of #GValue<!---->s in @param_values, must be exactly
 *                   four for this job.
 * @error          : return location for errors or %NULL.
 *
 * See thunar_vfs_change_group() and thunar_vfs_change_owner() for details.
 *
 * The #ThunarVfsPath<!---->s in the first item of @param_values may only include local
 * paths with scheme %THUNAR_VFS_PATH_SCHEME_FILE.
 *
 * Return value: %TRUE if the changing of ownership succeed, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_io_jobs_chown (ThunarVfsJob *job,
                           const GValue *param_values,
                           guint         n_param_values,
                           GError      **error)
{
  struct stat statb;
  gboolean    recursive = g_value_get_boolean (&param_values[3]);
  GError     *err = NULL;
  GList      *path_list = g_value_get_boxed (&param_values[0]);
  GList      *lp;
  gchar      *absolute_path;
  gchar      *display_name;
  gint        sverrno;
  gint        uid = g_value_get_int (&param_values[1]);
  gint        gid = g_value_get_int (&param_values[2]);
  gint        nuid;
  gint        ngid;

  _thunar_vfs_return_val_if_fail (G_VALUE_HOLDS (&param_values[0], THUNAR_VFS_TYPE_PATH_LIST), FALSE);
  _thunar_vfs_return_val_if_fail (G_VALUE_HOLDS (&param_values[3], G_TYPE_BOOLEAN), FALSE);
  _thunar_vfs_return_val_if_fail (G_VALUE_HOLDS (&param_values[1], G_TYPE_INT), FALSE);
  _thunar_vfs_return_val_if_fail (G_VALUE_HOLDS (&param_values[2], G_TYPE_INT), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_vfs_return_val_if_fail (THUNAR_VFS_IS_JOB (job), FALSE);
  _thunar_vfs_return_val_if_fail (uid >= 0 || gid >= 0, FALSE);
  _thunar_vfs_return_val_if_fail (n_param_values == 4, FALSE);

#ifdef G_ENABLE_DEBUG
  /* verify that we have only local paths */
  for (lp = path_list; lp != NULL; lp = lp->next)
    _thunar_vfs_assert (_thunar_vfs_path_is_local (lp->data));
#endif

  /* collect the paths for the chown operation */
  path_list = recursive ? tvij_collect_nofollow (job, path_list, &err) : thunar_vfs_path_list_copy (path_list);
  if (G_UNLIKELY (err != NULL))
    {
      /* propagate the error */
      g_propagate_error (error, err);
      return FALSE;
    }

  /* we know the total list of paths to process now */
  _thunar_vfs_job_total_paths (job, path_list);

  /* change the ownership of all paths */
  for (lp = path_list; lp != NULL && !thunar_vfs_job_cancelled (job); lp = lp->next)
    {
      /* reset the saved errno */
      sverrno = 0;

      /* update the progress information */
      _thunar_vfs_job_process_path (job, lp);

      /* try to stat the file */
      absolute_path = thunar_vfs_path_dup_string (lp->data);
      if (g_stat (absolute_path, &statb) == 0)
        {
          /* determine the new uid/gid */
          nuid = (uid < 0) ? statb.st_uid : uid;
          ngid = (gid < 0) ? statb.st_gid : gid;

          /* try to apply the new ownership */
          if (chown (absolute_path, nuid, ngid) < 0)
            goto sverror;

          /* feed a change notification event */
          thunar_vfs_monitor_feed (_thunar_vfs_monitor, THUNAR_VFS_MONITOR_EVENT_CHANGED, lp->data);
        }
      else
        {
sverror:
          /* save the errno */
          sverrno = errno;
        }
      g_free (absolute_path);

      /* check if we failed (ignoring ENOENT) */
      if (G_UNLIKELY (sverrno != 0 && sverrno != ENOENT))
        {
          /* generate an error */
          display_name = _thunar_vfs_path_dup_display_name (lp->data);
          _thunar_vfs_set_g_error_from_errno2 (&err, sverrno, G_LIKELY (uid >= 0)
                                               ? _("Failed to change file owner of \"%s\"")
                                               : _("Failed to change file group of \"%s\""),
                                               display_name);
          g_free (display_name);

          /* ask the user whether to skip this one */
          _thunar_vfs_job_ask_skip (job, "%s", err->message);

          /* reset the error */
          g_clear_error (&err);
        }
    }

  /* release the path list */
  thunar_vfs_path_list_free (path_list);

  return TRUE;
}



/**
 * _thunar_vfs_io_jobs_create:
 * @job            : a #ThunarVfsJob.
 * @param_values   : exactly one #GValue with the #GList of #ThunarVfsPath<!---->s
 *                   for which to create new files.
 * @n_param_values : the number of #GValue<!---->s in @param_values, must be exactly
 *                   one for this job.
 * @error          : return location for errors or %NULL.
 *
 * Creates empty files for all #ThunarVfsPath<!---->s specified in @param_values.
 *
 * The #ThunarVfsPath<!---->s in @param_values may only include local paths with scheme
 * %THUNAR_VFS_PATH_SCHEME_FILE.
 *
 * Return value: %TRUE if the creation of the files succeed, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_io_jobs_create (ThunarVfsJob *job,
                            const GValue *param_values,
                            guint         n_param_values,
                            GError      **error)
{
  ThunarVfsJobResponse overwrite;
  GError              *err = NULL;
  gchar               *absolute_path;
  gchar               *display_name;
  gchar               *message;
  GList               *path_list = g_value_get_boxed (&param_values[0]);
  GList               *lp;
  gint                 fd;

  _thunar_vfs_return_val_if_fail (G_VALUE_HOLDS (&param_values[0], THUNAR_VFS_TYPE_PATH_LIST), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_vfs_return_val_if_fail (THUNAR_VFS_IS_JOB (job), FALSE);
  _thunar_vfs_return_val_if_fail (n_param_values == 1, FALSE);

#ifdef G_ENABLE_DEBUG
  /* verify that we have only local paths */
  for (lp = path_list; lp != NULL; lp = lp->next)
    _thunar_vfs_assert (_thunar_vfs_path_is_local (lp->data));
#endif

  /* we know the total paths to be processed */
  _thunar_vfs_job_total_paths (job, path_list);

  /* process all paths */
  for (lp = path_list; err == NULL && lp != NULL && !thunar_vfs_job_cancelled (job); lp = lp->next)
    {
      /* update the progress information */
      _thunar_vfs_job_process_path (job, lp);

      /* determine the absolute path to the path object */
      absolute_path = thunar_vfs_path_dup_string (lp->data);

again:
      /* try to create the file at the given path */
      fd = g_open (absolute_path, O_CREAT | O_EXCL | O_WRONLY, 0644);
      if (G_UNLIKELY (fd < 0))
        {
          /* check if the file already exists */
          if (G_UNLIKELY (errno == EEXIST))
            {
              /* ask the user whether to override this path */
              display_name = _thunar_vfs_path_dup_display_name (lp->data);
              overwrite = _thunar_vfs_job_ask_overwrite (job, _("The file \"%s\" already exists"), display_name);
              g_free (display_name);

              /* check if we should overwrite */
              if (G_UNLIKELY (overwrite == THUNAR_VFS_JOB_RESPONSE_YES))
                {
                  /* try to remove the file (fail if not possible) */
                  if (_thunar_vfs_io_ops_remove (lp->data, THUNAR_VFS_IO_OPS_IGNORE_ENOENT, &err))
                    {
                      /* try again */
                      goto again;
                    }
                }
            }
          else
            {
              /* ask the user whether to skip this path (cancels the job if not) */
              display_name = _thunar_vfs_path_dup_display_name (lp->data);
              message = g_strdup_printf (_("Failed to create empty file \"%s\""), display_name);
              _thunar_vfs_job_ask_skip (job, "%s: %s", message, g_strerror (errno));
              g_free (display_name);
              g_free (message);
            }
        }
      else
        {
          /* feed a "created" event for the new file */
          thunar_vfs_monitor_feed (_thunar_vfs_monitor, THUNAR_VFS_MONITOR_EVENT_CREATED, lp->data);

          /* close the file */
          close (fd);
        }

      /* cleanup */
      g_free (absolute_path);
    }

  /* check if we failed */
  if (G_UNLIKELY (err != NULL))
    {
      /* propagate the error */
      g_propagate_error (error, err);
      return FALSE;
    }

  /* emit the "new-files" signal with the given path list */
  _thunar_vfs_job_new_files (job, path_list);

  return TRUE;
}



/**
 * _thunar_vfs_io_jobs_link:
 * @job            : a #ThunarVfsJob.
 * @param_values   : exactly two #GValue<!---->s with the #GList of #ThunarVfsPath<!---->s
 *                   for the source paths and the target paths.
 * @n_param_values : the number of #GValue<!---->s in @param_values, must be exactly
 *                   two for this job.
 * @error          : return location for errors or %NULL.
 *
 * Creates symbolic links from all #ThunarVfsPath<!---->s in the first parameter of @param_values
 * to the corresponding #ThunarVfsPath<!---->s in the second parameter of @param_values.
 *
 * The #ThunarVfsPath<!---->s in both @param_values may only include local paths with scheme
 * %THUNAR_VFS_PATH_SCHEME_FILE.
 *
 * Return value: %TRUE if the link were created successfully, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_io_jobs_link (ThunarVfsJob *job,
                          const GValue *param_values,
                          guint         n_param_values,
                          GError      **error)
{
  ThunarVfsJobResponse overwrite;
  ThunarVfsPath       *target_path;
  GError              *err = NULL;
  GList               *source_path_list = g_value_get_boxed (&param_values[0]);
  GList               *target_path_list = g_value_get_boxed (&param_values[1]);
  GList               *sp;
  GList               *tp;

  _thunar_vfs_return_val_if_fail (g_list_length (source_path_list) == g_list_length (target_path_list), FALSE);
  _thunar_vfs_return_val_if_fail (G_VALUE_HOLDS (&param_values[0], THUNAR_VFS_TYPE_PATH_LIST), FALSE);
  _thunar_vfs_return_val_if_fail (G_VALUE_HOLDS (&param_values[1], THUNAR_VFS_TYPE_PATH_LIST), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_vfs_return_val_if_fail (THUNAR_VFS_IS_JOB (job), FALSE);
  _thunar_vfs_return_val_if_fail (n_param_values == 2, FALSE);

  /* we know the total list of paths to process */
  _thunar_vfs_job_total_paths (job, source_path_list);

  /* process all files */
  for (sp = source_path_list, tp = target_path_list; err == NULL && sp != NULL; sp = sp->next, tp = tp->next)
    {
      /* check if the job was cancelled by the user */
      if (G_UNLIKELY (thunar_vfs_job_cancelled (job)))
        break;

      /* update the progress information */
      _thunar_vfs_job_process_path (job, sp);

again:
      /* try to perform the symlink operation */
      if (_thunar_vfs_io_ops_link_file (sp->data, tp->data, &target_path, &err))
        {
          /* replace the path on the target path list */
          thunar_vfs_path_unref (tp->data);
          tp->data = target_path;
        }
      else if (G_LIKELY (err->domain == G_FILE_ERROR && err->code == G_FILE_ERROR_EXIST))
        {
          /* ask the user whether we should remove the target first */
          overwrite = _thunar_vfs_job_ask_overwrite (job, "%s", err->message);

          /* release the error */
          g_clear_error (&err);

          /* check if we should overwrite the existing file */
          if (G_LIKELY (overwrite == THUNAR_VFS_JOB_RESPONSE_YES))
            {
              /* try to remove the target file (fail if not possible) */
              if (_thunar_vfs_io_ops_remove (tp->data, THUNAR_VFS_IO_OPS_IGNORE_ENOENT, &err))
                {
                  /* try again... */
                  goto again;
                }
            }
        }
    }

  /* check if we failed */
  if (G_UNLIKELY (err != NULL))
    {
      /* propagate the error */
      g_propagate_error (error, err);
      return FALSE;
    }

  /* emit the "new-files" signal for the target paths */
  _thunar_vfs_job_new_files (job, target_path_list);

  return TRUE;
}



/**
 * _thunar_vfs_io_jobs_listdir:
 * @job            : a #ThunarVfsJob.
 * @param_values   : exactly one #GValue with the #ThunarVfsPath of the folder whose
 *                   contents to list.
 * @n_param_values : the number of #GValue<!---->s in @param_values, must be exactly
 *                   one for this job.
 * @error          : return location for errors or %NULL.
 *
 * Lists the contents of the folder at the #ThunarVfsPath in @param_values.
 *
 * Return value: %TRUE if the folder contents were successfully listed, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_io_jobs_listdir (ThunarVfsJob *job,
                             const GValue *param_values,
                             guint         n_param_values,
                             GError      **error)
{
  ThunarVfsPath *path = g_value_get_boxed (&param_values[0]);
  GError        *err = NULL;
  GList         *info_list;

  _thunar_vfs_return_val_if_fail (G_VALUE_HOLDS (&param_values[0], THUNAR_VFS_TYPE_PATH), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_vfs_return_val_if_fail (THUNAR_VFS_IS_JOB (job), FALSE);
  _thunar_vfs_return_val_if_fail (n_param_values == 1, FALSE);

  /* determine the type of directory to scan */
  switch (thunar_vfs_path_get_scheme (path))
    {
    case THUNAR_VFS_PATH_SCHEME_FILE:
      info_list = _thunar_vfs_io_local_listdir (path, &err);
      break;

    case THUNAR_VFS_PATH_SCHEME_TRASH:
      info_list = _thunar_vfs_io_trash_listdir (path, &err);
      break;

    default:
      _thunar_vfs_set_g_error_not_supported (error);
      return FALSE;
    }

  /* check if we have any files to report */
  if (G_LIKELY (info_list != NULL))
    {
      /* emit the "infos-ready" signal with the given list */
      if (!_thunar_vfs_job_infos_ready (job, info_list))
        {
          /* if none of the handlers took over ownership, we still
           * own it and we are thereby responsible to release it.
           */
          thunar_vfs_info_list_free (info_list);
        }
    }
  else if (err != NULL)
    {
      /* propagate the error to the caller */
      g_propagate_error (error, err);
      return FALSE;
    }

  return TRUE;
}



/**
 * _thunar_vfs_io_jobs_mkdir:
 * @job            : a #ThunarVfsJob.
 * @param_values   : exactly one #GValue with the #GList of #ThunarVfsPath<!---->s
 *                   for which to create new directories.
 * @n_param_values : the number of #GValue<!---->s in @param_values, must be exactly
 *                   one for this job.
 * @error          : return location for errors or %NULL.
 *
 * Creates new directories for all #ThunarVfsPath<!---->s specified in @param_values.
 *
 * The #ThunarVfsPath<!---->s in @param_values may only include local paths with scheme
 * %THUNAR_VFS_PATH_SCHEME_FILE.
 *
 * Return value: %TRUE if the creation of the folders succeed, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_io_jobs_mkdir (ThunarVfsJob *job,
                           const GValue *param_values,
                           guint         n_param_values,
                           GError      **error)
{
  GList *path_list = g_value_get_boxed (&param_values[0]);
  GList *lp;

  _thunar_vfs_return_val_if_fail (G_VALUE_HOLDS (&param_values[0], THUNAR_VFS_TYPE_PATH_LIST), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_vfs_return_val_if_fail (THUNAR_VFS_IS_JOB (job), FALSE);
  _thunar_vfs_return_val_if_fail (n_param_values == 1, FALSE);

#ifdef G_ENABLE_DEBUG
  /* verify that we have only local paths */
  for (lp = path_list; lp != NULL; lp = lp->next)
    _thunar_vfs_assert (_thunar_vfs_path_is_local (lp->data));
#endif

  /* we know the total list of paths to process */
  _thunar_vfs_job_total_paths (job, path_list);

  /* process all directories */
  for (lp = path_list; lp != NULL && !thunar_vfs_job_cancelled (job); lp = lp->next)
    {
      /* update the progress information */
      _thunar_vfs_job_process_path (job, lp);

      /* try to create the target directory */
      if (!_thunar_vfs_io_ops_mkdir (lp->data, 0755, THUNAR_VFS_IO_OPS_NONE, error))
        return FALSE;
    }

  /* emit the "new-files" signal */
  _thunar_vfs_job_new_files (job, path_list);
  return TRUE;
}



/**
 * _thunar_vfs_io_jobs_unlink:
 * @job            : a #ThunarVfsJob.
 * @param_values   : exactly one #GValue with the #GList of #ThunarVfsPath<!---->s
 *                   for the files and folders which should be unlinked recursively.
 * @n_param_values : the number of #GValue<!---->s in @param_values, must be exactly
 *                   one for this job.
 * @error          : return location for errors or %NULL.
 *
 * Recursively unlinks all #ThunarVfsPath<!---->s specified in @param_values.
 *
 * Return value: %TRUE if the unlink operation succeed, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_io_jobs_unlink (ThunarVfsJob *job,
                            const GValue *param_values,
                            guint         n_param_values,
                            GError      **error)
{
  GError *err = NULL;
  GList  *path_list;
  GList  *lp;

  _thunar_vfs_return_val_if_fail (G_VALUE_HOLDS (&param_values[0], THUNAR_VFS_TYPE_PATH_LIST), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_vfs_return_val_if_fail (THUNAR_VFS_IS_JOB (job), FALSE);
  _thunar_vfs_return_val_if_fail (n_param_values == 1, FALSE);

  /* collect the paths for the removal */
  path_list = tvij_collect_nofollow (job, g_value_get_boxed (&param_values[0]), &err);
  if (G_UNLIKELY (err != NULL))
    {
      /* propagate the error */
      g_propagate_error (error, err);
      return FALSE;
    }

  /* we know the total list of paths to process */
  _thunar_vfs_job_total_paths (job, path_list);

  /* perform the actual removal of the paths */
  for (lp = path_list; lp != NULL && !thunar_vfs_job_cancelled (job); lp = lp->next)
    {
      /* update the progress information */
      _thunar_vfs_job_process_path (job, lp);

      /* skip the root folders, which cannot be deleted anyway */
      if (G_LIKELY (!thunar_vfs_path_is_root (lp->data)))
        {
          /* remove the file for the current path */
          if (!_thunar_vfs_io_ops_remove (lp->data, THUNAR_VFS_IO_OPS_IGNORE_ENOENT, &err))
            {
              /* ask the user whether to skip the file */
              _thunar_vfs_job_ask_skip (job, "%s", err->message);

              /* clear the error */
              g_clear_error (&err);
            }
        }
    }

  /* release the path list */
  thunar_vfs_path_list_free (path_list);

  return TRUE;
}



#define __THUNAR_VFS_IO_JOBS_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>

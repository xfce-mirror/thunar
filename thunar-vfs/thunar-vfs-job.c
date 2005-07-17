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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar-vfs/thunar-vfs-job.h>



static void     thunar_vfs_job_register_type (GType        *type);
static void     thunar_vfs_job_init          (ThunarVfsJob *job);
static void     thunar_vfs_job_execute       (gpointer      data,
                                              gpointer      user_data);
static gboolean thunar_vfs_job_idle          (gpointer      user_data);



static GThreadPool *job_pool = NULL;



GType
thunar_vfs_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;
  static GOnce once = G_ONCE_INIT;

  /* thread-safe type registration */
  g_once (&once, (GThreadFunc) thunar_vfs_job_register_type, &type);

  return type;
}



static void
thunar_vfs_job_register_type (GType *type)
{
  static const GTypeFundamentalInfo finfo =
  {
    G_TYPE_FLAG_CLASSED | G_TYPE_FLAG_INSTANTIATABLE | G_TYPE_FLAG_DERIVABLE
  };

  static const GTypeInfo info =
  {
    sizeof (ThunarVfsJobClass),
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    sizeof (ThunarVfsJob),
    0,
    (GInstanceInitFunc) thunar_vfs_job_init,
    NULL,
  };

  *type = g_type_register_fundamental (g_type_fundamental_next (), "ThunarVfsJob", &info, &finfo, G_TYPE_FLAG_ABSTRACT);
}



static void
thunar_vfs_job_init (ThunarVfsJob *job)
{
  job->ref_count = 1;
  job->cond = g_cond_new ();
  job->mutex = g_mutex_new ();
}



static void
thunar_vfs_job_execute (gpointer data,
                        gpointer user_data)
{
  ThunarVfsJob *job = THUNAR_VFS_JOB (data);

  /* perform the real work */
  (*THUNAR_VFS_JOB_GET_CLASS (job)->execute) (job);

  /* cleanup */
  thunar_vfs_job_unref (job);
}



static gboolean
thunar_vfs_job_idle (gpointer user_data)
{
  ThunarVfsJob *job = THUNAR_VFS_JOB (user_data);

  g_return_val_if_fail (THUNAR_VFS_IS_JOB (job), FALSE);

  g_mutex_lock (job->mutex);

  /* invoke the callback if the job wasn't cancelled */
  if (G_LIKELY (!thunar_vfs_job_cancelled (job)))
    THUNAR_VFS_JOB_GET_CLASS (job)->callback (job);

  g_cond_signal (job->cond);
  g_mutex_unlock (job->mutex);

  /* remove the idle source */
  return FALSE;
}



/**
 * thunar_vfs_job_ref:
 * @job : a #ThunarVfsJob.
 *
 * Increments the reference count on @job by
 * 1 and returns a pointer to @job.
 *
 * Return value: a pointer to @job.
 **/
ThunarVfsJob*
thunar_vfs_job_ref (ThunarVfsJob *job)
{
  g_return_val_if_fail (THUNAR_VFS_IS_JOB (job), NULL);
  g_return_val_if_fail (job->ref_count > 0, NULL);

  g_atomic_int_inc (&job->ref_count);

  return job;
}



/**
 * thunar_vfs_job_unref:
 * @job : a #ThunarVfsJob.
 *
 * Decrements the reference count on @job by
 * 1. If the reference count drops to zero,
 * the resources allocated to @job will be
 * freed.
 **/
void
thunar_vfs_job_unref (ThunarVfsJob *job)
{
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (job->ref_count > 0);

  if (g_atomic_int_dec_and_test (&job->ref_count))
    {
      /* call the finalize method (if any) */
      if (THUNAR_VFS_JOB_GET_CLASS (job)->finalize != NULL)
        (*THUNAR_VFS_JOB_GET_CLASS (job)->finalize) (job);

      if (G_UNLIKELY (job->error != NULL))
        g_error_free (job->error);

      g_mutex_free (job->mutex);
      g_cond_free (job->cond);

      g_type_free_instance ((GTypeInstance *) job);
    }
}



/**
 * thunar_vfs_job_cancel:
 * @job : a #ThunarVfsJob.
 *
 * Attempts to cancel the operation currently
 * performed by @job. No single callback will
 * be invoked for a cancelled job, you need
 * to take that into account.
 **/
void
thunar_vfs_job_cancel (ThunarVfsJob *job)
{
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (job->ref_count > 0);

  job->cancelled = TRUE;
}



/**
 * thunar_vfs_job_cancelled:
 * @job : a #ThunarVfsJob.
 *
 * Checks whether @job was previously cancelled
 * by a call to #thunar_vfs_job_cancel().
 *
 * Return value: %TRUE if @job is cancelled.
 **/
gboolean
thunar_vfs_job_cancelled (const ThunarVfsJob *job)
{
  g_return_val_if_fail (THUNAR_VFS_IS_JOB (job), FALSE);
  g_return_val_if_fail (job->ref_count > 0, FALSE);

  return job->cancelled;
}



/**
 * thunar_vfs_job_failed:
 * @job : a #ThunarVfsJob.
 *
 * Checks whether the execution of @job failed with
 * an error. You can use #thunar_vfs_job_get_error()
 * to query the error cause.
 *
 * Return value: %TRUE if @job failed.
 **/
gboolean
thunar_vfs_job_failed (const ThunarVfsJob *job)
{
  g_return_val_if_fail (THUNAR_VFS_IS_JOB (job), FALSE);
  g_return_val_if_fail (job->ref_count > 0, FALSE);

  return (job->error != NULL);
}



/**
 * thunar_vfs_job_finished:
 * @job : a #ThunarVfsJob.
 *
 * Checks whether the execution of @job is finished,
 * either successfull or not. You can use
 * #thunar_vfs_job_failed() to determine whether the
 * execution terminated successfully.
 *
 * Return value: %TRUE if @job is done.
 **/
gboolean
thunar_vfs_job_finished (const ThunarVfsJob *job)
{
  g_return_val_if_fail (THUNAR_VFS_IS_JOB (job), FALSE);
  g_return_val_if_fail (job->ref_count > 0, FALSE);

  return job->finished;
}



/**
 * thunar_vfs_job_get_error:
 * @job : a #ThunarVfsJob.
 *
 * Returns the #GError that describes the exact cause
 * of a failure during the execution of @job, or %NULL
 * if @job is not finished or did not fail. Use
 * #thunar_vfs_job_failed() to see whether the @job
 * failed.
 *
 * The returned #GError instance is owned by @job, so
 * the caller must not free it.
 *
 * Return value: the #GError describing the failure
 *               cause for @job, or %NULL.
 **/
GError*
thunar_vfs_job_get_error (const ThunarVfsJob *job)
{
  g_return_val_if_fail (THUNAR_VFS_IS_JOB (job), NULL);
  g_return_val_if_fail (job->ref_count > 0, NULL);

  return job->error;
}



/**
 * thunar_vfs_job_finish:
 * @job : a #ThunarVfsJob.
 *
 * Marks the @job as finished (if not already finished).
 *
 * This method is part of the module API and must not
 * be called by application code.
 **/
void
thunar_vfs_job_finish (ThunarVfsJob *job)
{
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (job->ref_count > 0);

  job->finished = TRUE;
}



/**
 * thunar_vfs_job_schedule:
 * @job : a #ThunarVfsJob.
 *
 * This functions schedules @job to be run as soon
 * as possible, in a separate thread.
 *
 * Return value: a pointer to @job.
 **/
ThunarVfsJob*
thunar_vfs_job_schedule (ThunarVfsJob *job)
{
  g_return_val_if_fail (THUNAR_VFS_IS_JOB (job), NULL);
  g_return_val_if_fail (job->ref_count > 0, NULL);
  g_return_val_if_fail (job_pool != NULL, NULL);

  /* schedule the job on the thread pool */
  g_thread_pool_push (job_pool, thunar_vfs_job_ref (job), NULL);

  return job;
}



/**
 * thunar_vfs_job_callback:
 * @job : a #ThunarVfsJob.
 **/
void
thunar_vfs_job_callback (ThunarVfsJob *job)
{
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (job->ref_count > 0);

  if (G_LIKELY (!thunar_vfs_job_cancelled (job)))
    {
      g_mutex_lock (job->mutex);
      g_idle_add_full (G_PRIORITY_HIGH, thunar_vfs_job_idle, job, NULL);
      g_cond_wait (job->cond, job->mutex);
      g_mutex_unlock (job->mutex);
    }
}



/**
 * thunar_vfs_job_set_error:
 * @job     : a #ThunarVfsJob.
 * @domain  : the #GError domain.
 * @code    : the #GError code.
 * @message : an error message.
 *
 * Marks @job as failed and finished, and sets the failure
 * cause according to the given error parameters.
 *
 * This method is part of the module API and must not
 * be called by application code.
 **/
void
thunar_vfs_job_set_error (ThunarVfsJob *job,
                          GQuark        domain,
                          gint          code,
                          const gchar  *message)
{
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (job->ref_count > 0);
  g_return_if_fail (message != NULL);

  if (G_UNLIKELY (job->error != NULL))
    g_error_free (job->error);

  job->error = g_error_new_literal (domain, code, message);
  job->finished = TRUE;
}








#if 0
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <thunar-vfs/thunar-vfs-job.h>



typedef struct _ThunarVfsJobReadFolder ThunarVfsJobReadFolder;



static void thunar_vfs_job_callback (ThunarVfsJob *job);



struct _ThunarVfsJob
{
  volatile gint cancelled;

  void (*callback_func) (ThunarVfsJob *job);
  void (*execute_func) (ThunarVfsJob *job);
  void (*free_func) (ThunarVfsJob *job);
};

struct _ThunarVfsJobReadFolder
{
  ThunarVfsJob __parent__;

  ThunarVfsURI *uri;
  GSList       *infos;
  GError       *error;

  ThunarVfsJobReadFolderCallback callback;
  gpointer                       user_data;
};



static GThreadPool *job_pool = NULL;

#ifndef HAVE_READDIR_R
G_LOCK_DEFINE_STATIC (readdir);
#endif



static void
thunar_vfs_job_execute (gpointer data,
                        gpointer user_data)
{
  ThunarVfsJob *job = (ThunarVfsJob *) data;
  (*job->execute_func) (job);
  (*job->free_func) (job);
}



static void
thunar_vfs_job_schedule (ThunarVfsJob *job)
{
  g_thread_pool_push (job_pool, job, NULL);
}



static void
thunar_vfs_job_read_folder_free (ThunarVfsJob *job)
{
  ThunarVfsJobReadFolder *folder_job = (ThunarVfsJobReadFolder *) job;

  if (G_UNLIKELY (folder_job->error != NULL))
    g_error_free (folder_job->error);

  thunar_vfs_info_list_free (folder_job->infos);
  thunar_vfs_uri_unref (folder_job->uri);

  g_free (folder_job);
}



static void
thunar_vfs_job_read_folder_callback (ThunarVfsJob *job)
{
  ThunarVfsJobReadFolder *folder_job = (ThunarVfsJobReadFolder *) job;
  (*folder_job->callback) (job, folder_job->error, folder_job->infos, folder_job->user_data);
}



static void
thunar_vfs_job_read_folder_execute (ThunarVfsJob *job)
{
  ThunarVfsJobReadFolder *folder_job = (ThunarVfsJobReadFolder *) job;
  ThunarVfsInfo          *info;
  struct dirent           d_buffer;
  struct dirent          *d;
  GStringChunk           *names_chunk;
  ThunarVfsURI           *file_uri;
  const gchar            *folder_path;
  GSList                 *names;
  GSList                 *lp;
  DIR                    *dp;

  folder_path = thunar_vfs_uri_get_path (folder_job->uri);

  dp = opendir (folder_path);
  if (G_UNLIKELY (dp == NULL))
    {
      folder_job->error = g_error_new_literal (G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
    }
  else
    {
      names_chunk = g_string_chunk_new (4 * 1024);
      names = NULL;

      /* read the directory content */
      for (;;)
        {
#ifdef HAVE_READDIR_R
          if (G_UNLIKELY (readdir_r (dp, &d_buffer, &d) < 0))
            {
              folder_job->error = g_error_new_literal (G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
              break;
            }
#else
          G_LOCK (readdir);
          d = readdir (dp);
          if (G_LIKELY (d != NULL))
            {
              memcpy (&d_buffer, d, sizeof (*d));
              d = &d_buffer;
            }
          G_UNLOCK (readdir);
#endif

          if (G_UNLIKELY (d == NULL))
            {
              /* end of directory reached */
              break;
            }

          names = g_slist_prepend (names, g_string_chunk_insert (names_chunk, d->d_name));
        }

      closedir (dp);

      /* check if the job was neither cancelled nor was there an error */
      if (G_LIKELY (!job->cancelled && folder_job->error == NULL))
        {
          for (lp = names; lp != NULL; lp = lp->next)
            {
              file_uri = thunar_vfs_uri_relative (folder_job->uri, lp->data);
              info = thunar_vfs_info_new_for_uri (file_uri, NULL);
              if (G_LIKELY (info != NULL))
                folder_job->infos = g_slist_prepend (folder_job->infos, info);
              thunar_vfs_uri_unref (file_uri);
            }
        }

      /* free the names */
      g_string_chunk_free (names_chunk);
      g_slist_free (names);
    }

  thunar_vfs_job_callback (job);
}



/**
 * _thunar_vfs_job_init:
 *
 * Initializes the jobs module of the ThunarVFS
 * library.
 **/
void
_thunar_vfs_job_init (void)
{
  g_return_if_fail (job_pool == NULL);

  job_pool = g_thread_pool_new (thunar_vfs_job_execute, NULL, 8, FALSE, NULL);
}



/**
 **/
void
thunar_vfs_job_cancel (ThunarVfsJob *job)
{
  g_return_if_fail (job != NULL);
  g_return_if_fail (!job->cancelled);

  g_atomic_int_inc ((gint *) &job->cancelled);
}



/**
 **/
ThunarVfsJob*
thunar_vfs_job_read_folder (ThunarVfsURI                  *folder_uri,
                            ThunarVfsJobReadFolderCallback callback,
                            gpointer                       user_data)
{
  ThunarVfsJobReadFolder *folder_job;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (folder_uri), NULL);
  g_return_val_if_fail (callback != NULL, NULL);

  folder_job = g_new0 (ThunarVfsJobReadFolder, 1);
  folder_job->uri = thunar_vfs_uri_ref (folder_uri);
  folder_job->callback = callback;
  folder_job->user_data = user_data;

  THUNAR_VFS_JOB (folder_job)->callback_func = thunar_vfs_job_read_folder_callback;
  THUNAR_VFS_JOB (folder_job)->execute_func = thunar_vfs_job_read_folder_execute;
  THUNAR_VFS_JOB (folder_job)->free_func = thunar_vfs_job_read_folder_free;

  thunar_vfs_job_schedule (THUNAR_VFS_JOB (folder_job));

  return THUNAR_VFS_JOB (folder_job);
}
#endif



/**
 * _thunar_vfs_job_init:
 *
 * Initializes the jobs module of the ThunarVFS
 * library.
 **/
void
_thunar_vfs_job_init (void)
{
  g_return_if_fail (job_pool == NULL);

  job_pool = g_thread_pool_new (thunar_vfs_job_execute, NULL, 8, FALSE, NULL);
}



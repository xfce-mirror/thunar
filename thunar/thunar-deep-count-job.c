/* vi:set sw=2 sts=2 ts=2 et ai: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>.
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or (at 
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 * MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <thunar/thunar-deep-count-job.h>
#include <thunar/thunar-job.h>
#include <thunar/thunar-marshal.h>
#include <thunar/thunar-private.h>



/* Signal identifiers */
enum
{
  STATUS_UPDATE,
  LAST_SIGNAL,
};



static void     thunar_deep_count_job_finalize   (GObject                 *object);
static gboolean thunar_deep_count_job_execute    (ExoJob                  *job,
                                                  GError                 **error);



struct _ThunarDeepCountJobClass
{
  ThunarJobClass __parent__;

  /* signals */
  void (*status_update) (ThunarJob *job,
                         guint64    total_size,
                         guint      file_count,
                         guint      directory_count,
                         guint      unreadable_directory_count);
};

struct _ThunarDeepCountJob
{
  ThunarJob __parent__;

  GFile              *file;
  GFileQueryInfoFlags query_flags;

  /* the time of the last "status-update" emission */
  GTimeVal            last_time;

  /* status information */
  guint64             total_size;
  guint               file_count;
  guint               directory_count;
  guint               unreadable_directory_count;
};



static guint deep_count_signals[LAST_SIGNAL];



G_DEFINE_TYPE (ThunarDeepCountJob, thunar_deep_count_job, THUNAR_TYPE_JOB)



static void
thunar_deep_count_job_class_init (ThunarDeepCountJobClass *klass)
{
  ExoJobClass  *job_class;
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_deep_count_job_finalize; 

  job_class = EXO_JOB_CLASS (klass);
  job_class->execute = thunar_deep_count_job_execute;

  /**
   * ThunarDeepCountJob::status-update:
   * @job                        : a #ThunarJob.
   * @total_size                 : the total size in bytes.
   * @file_count                 : the number of files.
   * @directory_count            : the number of directories.
   * @unreadable_directory_count : the number of unreadable directories.
   *
   * Emitted by the @job to inform listeners about the number of files, 
   * directories and bytes counted so far.
   **/
  deep_count_signals[STATUS_UPDATE] =
    g_signal_new ("status-update",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS,
                  G_STRUCT_OFFSET (ThunarDeepCountJobClass, status_update),
                  NULL, NULL,
                  _thunar_marshal_VOID__UINT64_UINT_UINT_UINT,
                  G_TYPE_NONE, 4,
                  G_TYPE_UINT64,
                  G_TYPE_UINT,
                  G_TYPE_UINT,
                  G_TYPE_UINT);
}



static void
thunar_deep_count_job_init (ThunarDeepCountJob *job)
{
  job->file = NULL;
  job->query_flags = G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS;
  job->total_size = 0;
  job->file_count = 0;
  job->directory_count = 0;
  job->unreadable_directory_count = 0;
}



static void
thunar_deep_count_job_finalize (GObject *object)
{
  ThunarDeepCountJob *job = THUNAR_DEEP_COUNT_JOB (object);

  if (G_LIKELY (job->file != NULL))
    g_object_unref (job->file);

  (*G_OBJECT_CLASS (thunar_deep_count_job_parent_class)->finalize) (object);
}



static void
thunar_deep_count_job_status_update (ThunarDeepCountJob *job)
{
  _thunar_return_if_fail (THUNAR_IS_DEEP_COUNT_JOB (job));

  exo_job_emit (EXO_JOB (job), 
                deep_count_signals[STATUS_UPDATE],
                0,
                job->total_size,
                job->file_count,
                job->directory_count,
                job->unreadable_directory_count);
}



static gboolean
thunar_deep_count_job_process (ExoJob  *job,
                               GFile   *file,
                               GError **error)
{
  ThunarDeepCountJob *count_job = THUNAR_DEEP_COUNT_JOB (job);
  GFileEnumerator    *enumerator;
  GFileInfo          *child_info;
  GFileInfo          *info;
  gboolean            success = TRUE;
  GFile              *child;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* abort if job was already cancelled */
  if (exo_job_is_cancelled (job))
    return FALSE;

  /* query size and type of the current file */
  info = g_file_query_info (file, 
                            G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                            G_FILE_ATTRIBUTE_STANDARD_SIZE,
                            count_job->query_flags,
                            exo_job_get_cancellable (job),
                            error);

  /* abort on invalid info or cancellation */
  if (info == NULL || exo_job_is_cancelled (job))
    return FALSE;

  /* add size of the file to the total size */
  count_job->total_size += g_file_info_get_size (info);

  /* recurse if we have a directory */
  if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
    {
      /* try to read from the directory */
      enumerator = g_file_enumerate_children (file,
                                              G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                              G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                                              G_FILE_ATTRIBUTE_STANDARD_NAME,
                                              count_job->query_flags,
                                              exo_job_get_cancellable (job),
                                              error);

      if (!exo_job_is_cancelled (job))
        {
          if (enumerator == NULL)
            {
              /* directory was unreadable */
              count_job->unreadable_directory_count += 1;

              if (file == count_job->file)
                {
                  /* we only bail out if the job file is unreadable */
                  success = FALSE;
                }
              else
                {
                  /* ignore errors from files other than the job file */
                  g_error_free (*error);
                  *error = NULL;
                }
            }
          else
            {
              /* directory was readable */
              count_job->directory_count += 1;

              while (!exo_job_is_cancelled (job))
                {
                  /* query next child info */
                  child_info = g_file_enumerator_next_file (enumerator,
                                                            exo_job_get_cancellable (job),
                                                            error);

                  /* abort on invalid child info (iteration ends) or cancellation */
                  if (child_info == NULL || exo_job_is_cancelled (job))
                    break;

                  /* generate a GFile for the child */
                  child = g_file_resolve_relative_path (file, g_file_info_get_name (child_info));

                  /* recurse unless the job was cancelled before */
                  if (!exo_job_is_cancelled (job))
                    thunar_deep_count_job_process (job, child, error);

                  /* free resources */
                  g_object_unref (child);
                  g_object_unref (child_info);
                }

              /* destroy the enumerator */
              g_object_unref (enumerator);
            }
        }

      /* emit status update whenever we've finished a directory */
      thunar_deep_count_job_status_update (count_job);
    }
  else
    {
      /* we have a regular file or at least not a directory */
      count_job->file_count += 1;
    }

  /* destroy the file info */
  g_object_unref (info);

  /* emit final status update at the very end of the computation */
  if (file == count_job->file)
    thunar_deep_count_job_status_update (count_job);

  /* we've succeeded if there was no error when loading information
   * about the job file itself and the job was not cancelled */
  return !exo_job_is_cancelled (job) && success;
}



static gboolean
thunar_deep_count_job_execute (ExoJob  *job,
                               GError **error)
{
  gboolean success;
  GError  *err = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* don't start the job if it was already cancelled */
  if (exo_job_set_error_if_cancelled (job, error))
    return FALSE;

  /* count files, directories and compute size of the job file */
  success = thunar_deep_count_job_process (job, 
                                           THUNAR_DEEP_COUNT_JOB (job)->file, 
                                           &err);

  if (!success)
    {
      g_assert (err != NULL || exo_job_is_cancelled (job));

      /* set error if the job was cancelled. otherwise just propagate 
       * the results of the processing function */
      if (exo_job_set_error_if_cancelled (job, error))
        {
          if (err != NULL)
            g_error_free (err);
        }
      else
        {
          if (err != NULL)
            g_propagate_error (error, err);
        }
      
      return FALSE;
    }
  else
    return TRUE;
}



ThunarDeepCountJob *
thunar_deep_count_job_new (GFile              *file,
                           GFileQueryInfoFlags flags)
{
  ThunarDeepCountJob *job;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);

  job = g_object_new (THUNAR_TYPE_DEEP_COUNT_JOB, NULL);
  job->file = g_object_ref (file);
  job->query_flags = flags;

  return job;
}

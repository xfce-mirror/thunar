/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-io-scan-directory.h>
#include <thunar/thunar-io-jobs-util.h>
#include <thunar/thunar-job.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-thumbnail-cache.h>
#include <thunar/thunar-transfer-job.h>



/* seconds before we show the transfer rate + remaining time */
#define MINIMUM_TRANSFER_TIME (10 * G_USEC_PER_SEC) /* 10 seconds */



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILE_SIZE_BINARY,
  PROP_FREEZE_TRANSFER_ON_SAME_SOURCE_DEVICE,
  PROP_FREEZE_TRANSFER_ON_SAME_TARGET_DEVICE,
};



typedef struct _ThunarTransferNode ThunarTransferNode;



static void     thunar_transfer_job_get_property (GObject    *object,
                                                  guint       prop_id,
                                                  GValue     *value,
                                                  GParamSpec *pspec);

static void     thunar_transfer_job_set_property (GObject      *object,
                                                  guint         prop_id,
                                                  const GValue *value,
                                                  GParamSpec   *pspec);

static void     thunar_transfer_job_finalize     (GObject                *object);
static gboolean thunar_transfer_job_execute      (ExoJob                 *job,
                                                  GError                **error);
static void     thunar_transfer_node_free        (gpointer                data);



struct _ThunarTransferJobClass
{
  ThunarJobClass __parent__;
};

struct _ThunarTransferJob
{
  ThunarJob             __parent__;

  ThunarTransferJobType type;
  GList                *source_node_list;
  guint32               source_device_id;
  GList                *target_file_list;
  guint32               target_device_id;

  gint64                start_time;
  gint64                last_update_time;
  guint64               last_total_progress;

  guint64               total_size;
  guint64               total_progress;
  guint64               file_progress;
  guint64               transfer_rate;

  ThunarPreferences    *preferences;
  gboolean              file_size_binary;
  gboolean              freeze_on_same_source_device;
  gboolean              freeze_on_same_target_device;
};

struct _ThunarTransferNode
{
  ThunarTransferNode *next;
  ThunarTransferNode *children;
  GFile              *source_file;
  gboolean            replace_confirmed;
};



G_DEFINE_TYPE (ThunarTransferJob, thunar_transfer_job, THUNAR_TYPE_JOB)



static void
thunar_transfer_job_class_init (ThunarTransferJobClass *klass)
{
  GObjectClass *gobject_class;
  ExoJobClass  *exojob_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_transfer_job_finalize;
  gobject_class->get_property = thunar_transfer_job_get_property;
  gobject_class->set_property = thunar_transfer_job_set_property;

  exojob_class = EXO_JOB_CLASS (klass);
  exojob_class->execute = thunar_transfer_job_execute;

  /**
   * ThunarPropertiesDialog:file_size_binary:
   *
   * Whether the file size should be shown in binary or decimal.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE_SIZE_BINARY,
                                   g_param_spec_boolean ("file-size-binary",
                                                         "FileSizeBinary",
                                                         NULL,
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPropertiesDialog:freeze_on_same_source_device:
   *
   * Whether to freeze this job if another job is transfering
   * from the same source device.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FREEZE_TRANSFER_ON_SAME_SOURCE_DEVICE,
                                   g_param_spec_boolean ("freeze-transfer-on-same-source-device",
                                                         "FreezeTransferOnSameSourceDevice",
                                                         NULL,
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPropertiesDialog:freeze_on_same_target_device:
   *
   * Whether to freeze this job if another job is transfering
   * to the same target device.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FREEZE_TRANSFER_ON_SAME_TARGET_DEVICE,
                                   g_param_spec_boolean ("freeze-transfer-on-same-target-device",
                                                         "FreezeTransferOnSameTargetDevice",
                                                         NULL,
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));
}



static void
thunar_transfer_job_init (ThunarTransferJob *job)
{
  job->preferences = thunar_preferences_get ();
  exo_binding_new (G_OBJECT (job->preferences), "misc-file-size-binary",
                   G_OBJECT (job), "file-size-binary");
  exo_binding_new (G_OBJECT (job->preferences), "misc-freeze-transfer-on-same-source-device",
                   G_OBJECT (job), "freeze-transfer-on-same-source-device");
  exo_binding_new (G_OBJECT (job->preferences), "misc-freeze-transfer-on-same-target-device",
                   G_OBJECT (job), "freeze-transfer-on-same-target-device");

  job->type = 0;
  job->source_node_list = NULL;
  job->source_device_id = 0;
  job->target_file_list = NULL;
  job->target_device_id = 0;
  job->total_size = 0;
  job->total_progress = 0;
  job->file_progress = 0;
  job->last_update_time = 0;
  job->last_total_progress = 0;
  job->transfer_rate = 0;
  job->start_time = 0;
}



static void
thunar_transfer_job_finalize (GObject *object)
{
  ThunarTransferJob *job = THUNAR_TRANSFER_JOB (object);

  g_list_free_full (job->source_node_list, thunar_transfer_node_free);

  thunar_g_file_list_free (job->target_file_list);

  g_object_unref (job->preferences);

  (*G_OBJECT_CLASS (thunar_transfer_job_parent_class)->finalize) (object);
}



static void
thunar_transfer_job_get_property (GObject     *object,
                                  guint        prop_id,
                                  GValue      *value,
                                  GParamSpec  *pspec)
{
  ThunarTransferJob *job = THUNAR_TRANSFER_JOB (object);

  switch (prop_id)
    {
    case PROP_FILE_SIZE_BINARY:
      g_value_set_boolean (value, job->file_size_binary);
      break;
    case PROP_FREEZE_TRANSFER_ON_SAME_SOURCE_DEVICE:
      g_value_set_boolean (value, job->freeze_on_same_source_device);
      break;
    case PROP_FREEZE_TRANSFER_ON_SAME_TARGET_DEVICE:
      g_value_set_boolean (value, job->freeze_on_same_target_device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_transfer_job_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ThunarTransferJob *job = THUNAR_TRANSFER_JOB (object);

  switch (prop_id)
    {
    case PROP_FILE_SIZE_BINARY:
      job->file_size_binary = g_value_get_boolean (value);
      break;
    case PROP_FREEZE_TRANSFER_ON_SAME_SOURCE_DEVICE:
      job->freeze_on_same_source_device = g_value_get_boolean (value);
      break;
    case PROP_FREEZE_TRANSFER_ON_SAME_TARGET_DEVICE:
      job->freeze_on_same_target_device = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_transfer_job_check_pause (ThunarTransferJob *job)
{
  _thunar_return_if_fail (THUNAR_IS_TRANSFER_JOB (job));
  while (thunar_job_is_paused (THUNAR_JOB (job)) && !exo_job_is_cancelled (EXO_JOB (job)))
    {
      g_usleep (500 * 1000); /* 500ms pause */
    }
}



static void
thunar_transfer_job_progress (goffset  current_num_bytes,
                              goffset  total_num_bytes,
                              gpointer user_data)
{
  ThunarTransferJob *job = user_data;
  guint64            new_percentage;
  gint64             current_time;
  gint64             expired_time;
  guint64            transfer_rate;

  _thunar_return_if_fail (THUNAR_IS_TRANSFER_JOB (job));

  thunar_transfer_job_check_pause (job);

  if (G_LIKELY (job->total_size > 0))
    {
      /* update total progress */
      job->total_progress += (current_num_bytes - job->file_progress);

      /* update file progress */
      job->file_progress = current_num_bytes;

      /* compute the new percentage after the progress we've made */
      new_percentage = (job->total_progress * 100.0) / job->total_size;

      /* get current time */
      current_time = g_get_real_time ();
      expired_time = current_time - job->last_update_time;

      /* notify callers not more then every 500ms */
      if (expired_time > (500 * 1000))
        {
          /* calculate the transfer rate in the last expired time */
          transfer_rate = (job->total_progress - job->last_total_progress) / ((gfloat) expired_time / G_USEC_PER_SEC);

          /* take the average of the last 10 rates (5 sec), so the output is less jumpy */
          if (job->transfer_rate > 0)
            job->transfer_rate = ((job->transfer_rate * 10) + transfer_rate) / 11;
          else
            job->transfer_rate = transfer_rate;

          /* emit the percent signal */
          exo_job_percent (EXO_JOB (job), new_percentage);

          /* update internals */
          job->last_update_time = current_time;
          job->last_total_progress = job->total_progress;
        }
    }
}



static gboolean
thunar_transfer_job_collect_node (ThunarTransferJob  *job,
                                  ThunarTransferNode *node,
                                  GError            **error)
{
  ThunarTransferNode *child_node;
  GFileInfo          *info;
  GError             *err = NULL;
  GList              *file_list;
  GList              *lp;

  _thunar_return_val_if_fail (THUNAR_IS_TRANSFER_JOB (job), FALSE);
  _thunar_return_val_if_fail (node != NULL && G_IS_FILE (node->source_file), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return FALSE;

  info = g_file_query_info (node->source_file,
                            G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                            G_FILE_ATTRIBUTE_STANDARD_TYPE,
                            G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                            exo_job_get_cancellable (EXO_JOB (job)),
                            &err);

  if (G_UNLIKELY (info == NULL))
    return FALSE;

  job->total_size += g_file_info_get_size (info);

  /* check if we have a directory here */
  if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
    {
      /* scan the directory for immediate children */
      file_list = thunar_io_scan_directory (THUNAR_JOB (job), node->source_file,
                                            G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                            FALSE, FALSE, FALSE, &err);

      /* add children to the transfer node */
      for (lp = file_list; err == NULL && lp != NULL; lp = lp->next)
        {
          thunar_transfer_job_check_pause (job);

          /* allocate a new transfer node for the child */
          child_node = g_slice_new0 (ThunarTransferNode);
          child_node->source_file = g_object_ref (lp->data);
          child_node->replace_confirmed = node->replace_confirmed;

          /* hook the child node into the child list */
          child_node->next = node->children;
          node->children = child_node;

          /* collect the child node */
          thunar_transfer_job_collect_node (job, child_node, &err);
        }

      /* release the child files */
      thunar_g_file_list_free (file_list);
    }

  /* release file info */
  g_object_unref (info);

  if (G_UNLIKELY (err != NULL))
    {
      g_propagate_error (error, err);
      return FALSE;
    }

  return TRUE;
}



static gboolean
ttj_copy_file (ThunarTransferJob *job,
               GFile             *source_file,
               GFile             *target_file,
               GFileCopyFlags     copy_flags,
               gboolean           merge_directories,
               GError           **error)
{
  GFileType source_type;
  GFileType target_type;
  gboolean  target_exists;
  GError   *err = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_TRANSFER_JOB (job), FALSE);
  _thunar_return_val_if_fail (G_IS_FILE (source_file), FALSE);
  _thunar_return_val_if_fail (G_IS_FILE (target_file), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* reset the file progress */
  job->file_progress = 0;

  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return FALSE;
  thunar_transfer_job_check_pause (job);

  source_type = g_file_query_file_type (source_file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                        exo_job_get_cancellable (EXO_JOB (job)));

  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return FALSE;
  thunar_transfer_job_check_pause (job);

  target_type = g_file_query_file_type (target_file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                        exo_job_get_cancellable (EXO_JOB (job)));

  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return FALSE;
  thunar_transfer_job_check_pause (job);

  /* check if the target is a symlink and we are in overwrite mode */
  if (target_type == G_FILE_TYPE_SYMBOLIC_LINK && (copy_flags & G_FILE_COPY_OVERWRITE) != 0)
    {
      /* try to delete the symlink */
      if (!g_file_delete (target_file, exo_job_get_cancellable (EXO_JOB (job)), &err))
        {
          g_propagate_error (error, err);
          return FALSE;
        }
    }

  /* try to copy the file */
  g_file_copy (source_file, target_file, copy_flags,
               exo_job_get_cancellable (EXO_JOB (job)),
               thunar_transfer_job_progress, job, &err);

  /* check if there were errors */
  if (G_UNLIKELY (err != NULL && err->domain == G_IO_ERROR))
    {
      if (err->code == G_IO_ERROR_WOULD_MERGE
          || (err->code == G_IO_ERROR_EXISTS
              && source_type == G_FILE_TYPE_DIRECTORY
              && target_type == G_FILE_TYPE_DIRECTORY))
        {
          /* we tried to overwrite a directory with a directory. this normally results
           * in a merge. ignore the error if we actually *want* to merge */
          if (merge_directories)
            g_clear_error (&err);
        }
      else if (err->code == G_IO_ERROR_WOULD_RECURSE)
        {
          g_clear_error (&err);

          /* we tried to copy a directory and either
           *
           * - the target did not exist which means we simple have to
           *   create the target directory
           *
           * or
           *
           * - the target is not a directory and we tried to overwrite it in
           *   which case we have to delete it first and then create the target
           *   directory
           */

          /* check if the target file exists */
          target_exists = g_file_query_exists (target_file,
                                               exo_job_get_cancellable (EXO_JOB (job)));

          /* abort on cancellation, continue otherwise */
          if (!exo_job_set_error_if_cancelled (EXO_JOB (job), &err))
            {
              if (target_exists)
                {
                  /* the target still exists and thus is not a directory. try to remove it */
                  g_file_delete (target_file,
                                 exo_job_get_cancellable (EXO_JOB (job)),
                                 &err);
                }

              /* abort on error or cancellation, continue otherwise */
              if (err == NULL)
                {
                  /* now try to create the directory */
                  g_file_make_directory (target_file,
                                         exo_job_get_cancellable (EXO_JOB (job)),
                                         &err);
                }
            }
        }
    }

  if (G_UNLIKELY (err != NULL))
    {
      g_propagate_error (error, err);
      return FALSE;
    }
  else
    {
      return TRUE;
    }
}



/**
 * thunar_transfer_job_copy_file:
 * @job                : a #ThunarTransferJob.
 * @source_file        : the source #GFile to copy.
 * @target_file        : the destination #GFile to copy to.
 * @replace_confirmed  : whether the user has already confirmed that this file should replace an existing one
 * @error              : return location for errors or %NULL.
 *
 * Tries to copy @source_file to @target_file. The real destination is the
 * return value and may differ from @target_file (e.g. if you try to copy
 * the file "/foo/bar" into the same directory you'll end up with something
 * like "/foo/copy of bar" instead of "/foo/bar"). If an existing file would
 * be replaced, the user is asked to confirm this unless @replace_confirmed
 * is TRUE.
 *
 * The return value is guaranteed to be %NULL on errors and @error will
 * always be set in those cases. If the file is skipped, the return value
 * will be @source_file.
 *
 * Return value: the destination #GFile to which @source_file was copied
 *               or linked. The caller is reposible to release it with
 *               g_object_unref() if no longer needed. It points to
 *               @source_file if the file was skipped and will be %NULL
 *               on error or cancellation.
 **/
static GFile *
thunar_transfer_job_copy_file (ThunarTransferJob *job,
                               GFile             *source_file,
                               GFile             *target_file,
                               gboolean           replace_confirmed,
                               GError           **error)
{
  ThunarJobResponse response;
  GFileCopyFlags    copy_flags = G_FILE_COPY_NOFOLLOW_SYMLINKS;
  GError           *err = NULL;
  gint              n;

  _thunar_return_val_if_fail (THUNAR_IS_TRANSFER_JOB (job), NULL);
  _thunar_return_val_if_fail (G_IS_FILE (source_file), NULL);
  _thunar_return_val_if_fail (G_IS_FILE (target_file), NULL);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* abort on cancellation */
  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return NULL;

  /* various attempts to copy the file */
  while (err == NULL)
    {
      thunar_transfer_job_check_pause (job);
      if (G_LIKELY (!g_file_equal (source_file, target_file)))
        {
          /* try to copy the file from source_file to the target_file */
          if (ttj_copy_file (job, source_file, target_file, copy_flags, TRUE, &err))
            {
              /* return the real target file */
              return g_object_ref (target_file);
            }
        }
      else
        {
          for (n = 1; err == NULL; ++n)
            {
              GFile *duplicate_file = thunar_io_jobs_util_next_duplicate_file (THUNAR_JOB (job),
                                                                               source_file,
                                                                               TRUE, n,
                                                                               &err);

              if (err == NULL)
                {
                  /* try to copy the file from source file to the duplicate file */
                  if (ttj_copy_file (job, source_file, duplicate_file, copy_flags, TRUE, &err))
                    {
                      /* return the real target file */
                      return duplicate_file;
                    }

                  g_object_unref (duplicate_file);
                }

              if (err != NULL && err->domain == G_IO_ERROR && err->code == G_IO_ERROR_EXISTS)
                {
                  /* this duplicate already exists => clear the error to try the next alternative */
                  g_clear_error (&err);
                }
            }
        }

      /* check if we can recover from this error */
      if (err->domain == G_IO_ERROR && err->code == G_IO_ERROR_EXISTS)
        {
          /* reset the error */
          g_clear_error (&err);

          /* if necessary, ask the user whether to replace the target file */
          if(replace_confirmed)
            response = THUNAR_JOB_RESPONSE_YES;
          else
            response = thunar_job_ask_replace (THUNAR_JOB (job), source_file,
                                               target_file, &err);

          if (err != NULL)
            break;

          /* add overwrite flag and retry if we should overwrite */
          if (response == THUNAR_JOB_RESPONSE_YES)
            {
              copy_flags |= G_FILE_COPY_OVERWRITE;
              continue;
            }

          /* tell the caller we skipped the file if the user
           * doesn't want to retry/overwrite */
          if (response == THUNAR_JOB_RESPONSE_NO)
            return g_object_ref (source_file);
        }
    }

  _thunar_assert (err != NULL);

  g_propagate_error (error, err);
  return NULL;
}



static void
thunar_transfer_job_copy_node (ThunarTransferJob  *job,
                               ThunarTransferNode *node,
                               GFile              *target_file,
                               GFile              *target_parent_file,
                               GList             **target_file_list_return,
                               GError            **error)
{
  ThunarThumbnailCache *thumbnail_cache;
  ThunarApplication    *application;
  ThunarJobResponse     response;
  GFileInfo            *info;
  GError               *err = NULL;
  GFile                *real_target_file = NULL;
  gchar                *base_name;

  _thunar_return_if_fail (THUNAR_IS_TRANSFER_JOB (job));
  _thunar_return_if_fail (node != NULL && G_IS_FILE (node->source_file));
  _thunar_return_if_fail (target_file == NULL || node->next == NULL);
  _thunar_return_if_fail ((target_file == NULL && target_parent_file != NULL) || (target_file != NULL && target_parent_file == NULL));
  _thunar_return_if_fail (error == NULL || *error == NULL);

  /* The caller can either provide a target_file or a target_parent_file, but not both. The toplevel
   * transfer_nodes (for which next is NULL) should be called with target_file, to get proper behavior
   * wrt restoring files from the trash. Other transfer_nodes will be called with target_parent_file.
   */

  /* take a reference on the thumbnail cache */
  application = thunar_application_get ();
  thumbnail_cache = thunar_application_get_thumbnail_cache (application);
  g_object_unref (application);

  for (; err == NULL && node != NULL; node = node->next)
    {
      /* guess the target file for this node (unless already provided) */
      if (G_LIKELY (target_file == NULL))
        {
          base_name = g_file_get_basename (node->source_file);
          target_file = g_file_get_child (target_parent_file, base_name);
          g_free (base_name);
        }
      else
        target_file = g_object_ref (target_file);

      /* query file info */
      info = g_file_query_info (node->source_file,
                                G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                exo_job_get_cancellable (EXO_JOB (job)),
                                &err);

      /* abort on error or cancellation */
      if (info == NULL)
        {
          g_object_unref (target_file);
          break;
        }

      /* update progress information */
      exo_job_info_message (EXO_JOB (job), "%s", g_file_info_get_display_name (info));

retry_copy:
      thunar_transfer_job_check_pause (job);

      /* copy the item specified by this node (not recursively) */
      real_target_file = thunar_transfer_job_copy_file (job, node->source_file,
                                                        target_file, node->replace_confirmed, &err);
      if (G_LIKELY (real_target_file != NULL))
        {
          /* node->source_file == real_target_file means to skip the file */
          if (G_LIKELY (node->source_file != real_target_file))
            {
              /* notify the thumbnail cache of the copy operation */
              thunar_thumbnail_cache_copy_file (thumbnail_cache,
                                                node->source_file,
                                                real_target_file);

              /* check if we have children to copy */
              if (node->children != NULL)
                {
                  /* copy all children of this node */
                  thunar_transfer_job_copy_node (job, node->children, NULL, real_target_file, NULL, &err);

                  /* free resources allocted for the children */
                  thunar_transfer_node_free (node->children);
                  node->children = NULL;
                }

              /* check if the child copy failed */
              if (G_UNLIKELY (err != NULL))
                {
                  /* outa here, freeing the target paths */
                  g_object_unref (real_target_file);
                  g_object_unref (target_file);
                  break;
                }

              /* add the real target file to the return list */
              if (G_LIKELY (target_file_list_return != NULL))
                {
                  *target_file_list_return =
                    thunar_g_file_list_prepend (*target_file_list_return,
                                                real_target_file);
                }

retry_remove:
              thunar_transfer_job_check_pause (job);

              /* try to remove the source directory if we are on copy+remove fallback for move */
              if (job->type == THUNAR_TRANSFER_JOB_MOVE)
                {
                  if (g_file_delete (node->source_file,
                                     exo_job_get_cancellable (EXO_JOB (job)),
                                     &err))
                    {
                      /* notify the thumbnail cache of the delete operation */
                      thunar_thumbnail_cache_delete_file (thumbnail_cache,
                                                          node->source_file);
                    }
                  else
                    {
                      /* ask the user to retry */
                      response = thunar_job_ask_skip (THUNAR_JOB (job), "%s",
                                                      err->message);

                      /* reset the error */
                      g_clear_error (&err);

                      /* check whether to retry */
                      if (G_UNLIKELY (response == THUNAR_JOB_RESPONSE_RETRY))
                        goto retry_remove;
                    }
                }
            }

          g_object_unref (real_target_file);
        }
      else if (err != NULL)
        {
          /* we can only skip if there is space left on the device */
          if (err->domain != G_IO_ERROR || err->code != G_IO_ERROR_NO_SPACE)
            {
              /* ask the user to skip this node and all subnodes */
              response = thunar_job_ask_skip (THUNAR_JOB (job), "%s", err->message);

              /* reset the error */
              g_clear_error (&err);

              /* check whether to retry */
              if (G_UNLIKELY (response == THUNAR_JOB_RESPONSE_RETRY))
                goto retry_copy;
            }
        }

      /* release the guessed target file */
      g_object_unref (target_file);
      target_file = NULL;

      /* release file info */
      g_object_unref (info);
    }

  /* release the thumbnail cache */
  g_object_unref (thumbnail_cache);

  /* propagate error if we failed or the job was cancelled */
  if (G_UNLIKELY (err != NULL))
    g_propagate_error (error, err);
}



static gboolean
thunar_transfer_job_verify_destination (ThunarTransferJob  *transfer_job,
                                        GError            **error)
{
  GFileInfo         *filesystem_info;
  guint64            free_space;
  GFile             *dest;
  GFileInfo         *dest_info;
  gchar             *dest_name = NULL;
  gchar             *base_name;
  gboolean           succeed = TRUE;
  gchar             *size_string;

  _thunar_return_val_if_fail (THUNAR_IS_TRANSFER_JOB (transfer_job), FALSE);

  /* no target file list */
  if (transfer_job->target_file_list == NULL)
    return TRUE;

  /* total size is nul, should be fine */
  if (transfer_job->total_size == 0)
    return TRUE;

  /* for all actions in thunar use the same target directory so
   * although not all files are checked, this should work nicely */
  dest = g_file_get_parent (G_FILE (transfer_job->target_file_list->data));

  /* query information about the filesystem */
  filesystem_info = g_file_query_filesystem_info (dest, THUNARX_FILESYSTEM_INFO_NAMESPACE,
                                                  exo_job_get_cancellable (EXO_JOB (transfer_job)),
                                                  NULL);

  /* unable to query the info, this could happen on some backends */
  if (filesystem_info == NULL)
    {
      g_object_unref (G_OBJECT (dest));
      return TRUE;
    }

  /* some info about the file */
  dest_info = g_file_query_info (dest, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, 0,
                                 exo_job_get_cancellable (EXO_JOB (transfer_job)),
                                 NULL);
  if (dest_info != NULL)
    {
      dest_name = g_strdup (g_file_info_get_display_name (dest_info));
      g_object_unref (G_OBJECT (dest_info));
    }

  if (dest_name == NULL)
    {
      base_name = g_file_get_basename (dest);
      dest_name = g_filename_display_name (base_name);
      g_free (base_name);
    }

  if (g_file_info_has_attribute (filesystem_info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE))
    {
      free_space = g_file_info_get_attribute_uint64 (filesystem_info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
      if (transfer_job->total_size > free_space)
        {
          size_string = g_format_size_full (transfer_job->total_size - free_space,
                                            transfer_job->file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
          succeed = thunar_job_ask_no_size (THUNAR_JOB (transfer_job),
                                             _("Error while copying to \"%s\": %s more space is "
                                               "required to copy to the destination"),
                                            dest_name, size_string);
          g_free (size_string);
        }
    }

  /* We used to check G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE here,
   * but it's not completely reliable, especially on remote file systems.
   * The downside is that operations on read-only files fail with a generic
   * error message.
   * More details: https://bugzilla.xfce.org/show_bug.cgi?id=15367#c16 */

  g_object_unref (filesystem_info);
  g_object_unref (G_OBJECT (dest));
  g_free (dest_name);

  return succeed;
}



static GList *
thunar_transfer_job_filter_running_jobs (GList *jobs, ThunarJob *own_job)
{
  ThunarJob *job;
  GList     *run_jobs = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_TRANSFER_JOB (own_job), NULL);

  for (; jobs != NULL; jobs = jobs->next)
    {
      job = jobs->data;
      if (job == own_job)
        continue;
      if (!exo_job_is_cancelled (EXO_JOB (job)) && !thunar_job_is_paused (job) && !thunar_job_is_frozen (job))
        {
          run_jobs = g_list_append (run_jobs, job);
        }
    }

  return run_jobs;
}



static gboolean
thunar_transfer_job_device_id_in_job_source_list (guint32 device_id, GList *jobs)
{
  ThunarTransferJob *job;

  for (; jobs != NULL; jobs = jobs->next)
    {
      if (THUNAR_IS_TRANSFER_JOB (jobs->data))
        {
          job = THUNAR_TRANSFER_JOB (jobs->data);
          if (job->source_device_id != 0 && device_id == job->source_device_id)
            return TRUE;
        }
    }
  return FALSE;
}



static gboolean
thunar_transfer_job_device_id_in_job_target_list (guint32 device_id, GList *jobs)
{
  ThunarTransferJob *job;

  for (; device_id != 0 && jobs != NULL; jobs = jobs->next)
    {
      if (THUNAR_IS_TRANSFER_JOB (jobs->data))
        {
          job = THUNAR_TRANSFER_JOB (jobs->data);
          if (job->target_device_id != 0 && device_id == job->target_device_id)
            return TRUE;
        }
    }
  return FALSE;
}



/**
 * thunar_transfer_job_verify_devices:
 * @job : a #ThunarTransferJob.
 *
 * Based on thunar setting, will block until all running jobs
 * doing IO on the source files or target files devices are completed.
 * The blocking could be forced by the user in the UI.
 *
 **/
static void
thunar_transfer_job_verify_devices (ThunarTransferJob *transfer_job)
{
  ThunarTransferNode *node;
  GFile              *file;
  GFileInfo          *file_info;
  guint32             src_device_id = 0;
  guint32             tgt_device_id = 0;

  _thunar_return_if_fail (THUNAR_IS_TRANSFER_JOB (transfer_job));

  /* no source node list nor target file list */
  if (transfer_job->source_node_list == NULL || transfer_job->target_file_list == NULL)
    return;

  /* first source file */
  node = transfer_job->source_node_list->data;
  file = node->source_file;
  /* query device id */
  file_info = g_file_query_info (file,
                                 G_FILE_ATTRIBUTE_UNIX_DEVICE,
                                 G_FILE_QUERY_INFO_NONE,
                                 exo_job_get_cancellable (EXO_JOB (transfer_job)),
                                 NULL);
  if (file_info != NULL)
    {
      src_device_id = g_file_info_get_attribute_uint32 (file_info, G_FILE_ATTRIBUTE_UNIX_DEVICE);
      transfer_job->source_device_id = src_device_id;
      g_object_unref (file_info);
    }

  /* first target file */
  file = G_FILE (transfer_job->target_file_list->data);
  /* query device id */
  file_info = g_file_query_info (file,
                                 G_FILE_ATTRIBUTE_UNIX_DEVICE,
                                 G_FILE_QUERY_INFO_NONE,
                                 exo_job_get_cancellable (EXO_JOB (transfer_job)),
                                 NULL);
  if (file_info != NULL)
    {
      tgt_device_id = g_file_info_get_attribute_uint32 (file_info, G_FILE_ATTRIBUTE_UNIX_DEVICE);
      transfer_job->target_device_id = tgt_device_id;
      g_object_unref (file_info);
    }

  if (transfer_job->freeze_on_same_source_device || transfer_job->freeze_on_same_target_device)
    {
      GList    *jobs = thunar_job_ask_jobs (THUNAR_JOB (transfer_job));
      GList    *other_jobs = thunar_transfer_job_filter_running_jobs (jobs, THUNAR_JOB (transfer_job));
      gboolean  been_frozen = FALSE;
      while (
        !exo_job_is_cancelled (EXO_JOB (transfer_job)) &&
        (
          (transfer_job->freeze_on_same_source_device && thunar_transfer_job_device_id_in_job_source_list (src_device_id, other_jobs)) ||
          (transfer_job->freeze_on_same_target_device && thunar_transfer_job_device_id_in_job_target_list (tgt_device_id, other_jobs))
        )
      )
        {
          if (!thunar_job_is_frozen (THUNAR_JOB (transfer_job)))
            {
              if (been_frozen)
                {
                  /* cannot re-freeze. It means that the user force to unfreeze */
                  break;
                }
              else
                {
                  /* first time here. The job needs to change to frozen state */
                  been_frozen = TRUE;
                  thunar_job_freeze (THUNAR_JOB (transfer_job));
                  thunar_job_emit_frozen_signal (THUNAR_JOB (transfer_job));
                }
            }
          g_usleep(500 * 1000);
          g_list_free (g_steal_pointer (&jobs));
          g_list_free (g_steal_pointer (&other_jobs));
          jobs = thunar_job_ask_jobs (THUNAR_JOB (transfer_job));
          other_jobs = thunar_transfer_job_filter_running_jobs (jobs, THUNAR_JOB (transfer_job));
        }
      g_list_free (g_steal_pointer (&jobs));
      g_list_free (g_steal_pointer (&other_jobs));
      if (thunar_job_is_frozen (THUNAR_JOB (transfer_job)))
        {
          thunar_job_unfreeze (THUNAR_JOB (transfer_job));
          thunar_job_emit_unfrozen_signal (THUNAR_JOB (transfer_job));
        }
    }
}



static gboolean
thunar_transfer_job_execute (ExoJob  *job,
                             GError **error)
{
  ThunarThumbnailCache *thumbnail_cache;
  ThunarTransferNode   *node;
  ThunarApplication    *application;
  ThunarJobResponse     response;
  ThunarTransferJob    *transfer_job = THUNAR_TRANSFER_JOB (job);
  GFileInfo            *info;
  GFileCopyFlags        flags;
  gboolean              parent_exists;
  gboolean              move_successful;
  GError               *err = NULL;
  GList                *new_files_list = NULL;
  GList                *snext;
  GList                *sp;
  GList                *tnext;
  GList                *tp;
  GFile                *target_parent;
  gchar                *base_name;
  gchar                *parent_display_name;

  _thunar_return_val_if_fail (THUNAR_IS_TRANSFER_JOB (job), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (exo_job_set_error_if_cancelled (job, error))
    return FALSE;

  exo_job_info_message (job, _("Collecting files..."));

  /* take a reference on the thumbnail cache */
  application = thunar_application_get ();
  thumbnail_cache = thunar_application_get_thumbnail_cache (application);
  g_object_unref (application);

  for (sp = transfer_job->source_node_list, tp = transfer_job->target_file_list;
       sp != NULL && tp != NULL && err == NULL;
       sp = snext, tp = tnext)
    {
      thunar_transfer_job_check_pause (transfer_job);

      /* determine the next list items */
      snext = sp->next;
      tnext = tp->next;

      /* determine the current source transfer node */
      node = sp->data;

      info = g_file_query_info (node->source_file,
                                G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                exo_job_get_cancellable (job),
                                &err);

      if (G_UNLIKELY (info == NULL))
        break;

      flags = G_FILE_COPY_NOFOLLOW_SYMLINKS | G_FILE_COPY_NO_FALLBACK_FOR_MOVE | G_FILE_COPY_ALL_METADATA;

      /* check if we are moving a file out of the trash */
      if (transfer_job->type == THUNAR_TRANSFER_JOB_MOVE
          && thunar_g_file_is_trashed (node->source_file))
        {
          /* Using this flag when moving filled folders out of trash leaves a copy in trash and pops up a warning */
          flags &= ~G_FILE_COPY_NO_FALLBACK_FOR_MOVE;

          /* update progress information */
          exo_job_info_message (job, _("Trying to restore \"%s\""),
                                g_file_info_get_display_name (info));

          /* determine the parent file */
          target_parent = g_file_get_parent (tp->data);

          /* check if the parent exists */
          if (target_parent != NULL)
            parent_exists = g_file_query_exists (target_parent, exo_job_get_cancellable (job));
          else
            parent_exists = FALSE;

          /* abort on cancellation */
          if (exo_job_set_error_if_cancelled (job, &err))
            {
              g_object_unref (target_parent);
              g_object_unref (info);
              break;
            }

          if (target_parent != NULL && !parent_exists)
            {
              /* determine the display name of the parent */
              base_name = g_file_get_basename (target_parent);
              parent_display_name = g_filename_display_name (base_name);
              g_free (base_name);

              /* ask the user whether he wants to create the parent folder because its gone */
              response = thunar_job_ask_create (THUNAR_JOB (job),
                                                _("The folder \"%s\" does not exist anymore but is "
                                                  "required to restore the file \"%s\" from the "
                                                  "trash"),
                                                parent_display_name,
                                                g_file_info_get_display_name (info));

              /* abort if cancelled */
              if (G_UNLIKELY (response == THUNAR_JOB_RESPONSE_CANCEL))
                {
                  g_object_unref (target_parent);
                  g_free (parent_display_name);
                  g_object_unref (info);
                  break;
                }

              /* try to create the parent directory */
              if (!g_file_make_directory_with_parents (target_parent,
                                                       exo_job_get_cancellable (job),
                                                       &err))
                {
                  if (!exo_job_is_cancelled (job))
                    {
                      g_clear_error (&err);

                      /* overwrite the internal GIO error with something more user-friendly */
                      g_set_error (&err, G_IO_ERROR, G_IO_ERROR_FAILED,
                                   _("Failed to restore the folder \"%s\""),
                                   parent_display_name);
                    }

                  g_object_unref (target_parent);
                  g_free (parent_display_name);
                  g_object_unref (info);
                  break;
                }

              /* clean up */
              g_free (parent_display_name);
            }

          if (target_parent != NULL)
            g_object_unref (target_parent);
        }

      if (transfer_job->type == THUNAR_TRANSFER_JOB_MOVE)
        {
          /* update progress information */
          exo_job_info_message (job, _("Trying to move \"%s\""),
                                g_file_info_get_display_name (info));

          /* try moving without overwriting */
          move_successful = g_file_move (node->source_file, tp->data,
                                         flags,
                                         exo_job_get_cancellable (job),
                                         NULL, NULL, &err);

          /* if the file already exists, ask the user if they want to overwrite it */
          if (!move_successful && err->code == G_IO_ERROR_EXISTS)
            {
              g_clear_error (&err);
              response = thunar_job_ask_replace (THUNAR_JOB (job), node->source_file, tp->data, NULL);

              /* if the user chose to overwrite then try to do so */
              if (response == THUNAR_JOB_RESPONSE_YES)
                {
                  node->replace_confirmed = TRUE;
                  move_successful = g_file_move (node->source_file, tp->data,
                                                 flags | G_FILE_COPY_OVERWRITE,
                                                 exo_job_get_cancellable (job),
                                                 NULL, NULL, &err);
                }

              /* if the user chose to cancel then abort all remaining file moves */
              if (response == THUNAR_JOB_RESPONSE_CANCEL)
                {
                  /* release all the remaining source and target files, and free the lists */
                  g_list_free_full (transfer_job->source_node_list, thunar_transfer_node_free);
                  transfer_job->source_node_list = NULL;
                  g_list_free_full (transfer_job->target_file_list, g_object_unref);
                  transfer_job->target_file_list= NULL;
                  g_object_unref (info);
                  break;
                }

              /* if the user chose not to replace the file, so that response == THUNAR_JOB_RESPONSE_NO,
               * then err will be NULL but move_successfull will be FALSE, so that the source and target
               * files will be released and the matching list items will be dropped below
               */
            }

          if (err == NULL)
            {
              if (move_successful)
                {
                  /* notify the thumbnail cache of the move operation */
                  thunar_thumbnail_cache_move_file (thumbnail_cache,
                                                    node->source_file,
                                                    tp->data);

                  /* add the target file to the new files list */
                  new_files_list = thunar_g_file_list_prepend (new_files_list, tp->data);
                }

              /* release source and target files */
              thunar_transfer_node_free (node);
              g_object_unref (tp->data);

              /* drop the matching list items */
              transfer_job->source_node_list = g_list_delete_link (transfer_job->source_node_list, sp);
              transfer_job->target_file_list = g_list_delete_link (transfer_job->target_file_list, tp);
            }
          /* prepare for the fallback copy and delete if appropriate */
          else if (!exo_job_is_cancelled (job) &&
                   ((err->code == G_IO_ERROR_NOT_SUPPORTED) ||
                    (err->code == G_IO_ERROR_WOULD_MERGE) || (err->code == G_IO_ERROR_WOULD_RECURSE)) )
            {
              g_clear_error (&err);

              /* update progress information */
              exo_job_info_message (job, _("Could not move \"%s\" directly. "
                                           "Collecting files for copying..."),
                                    g_file_info_get_display_name (info));

              /* if this call fails to collect the node, err will be non-NULL and the loop will exit */
              thunar_transfer_job_collect_node (transfer_job, node, &err);
            }

        }

      else if (transfer_job->type == THUNAR_TRANSFER_JOB_COPY)
        {
          if (!thunar_transfer_job_collect_node (THUNAR_TRANSFER_JOB (job), node, &err))
            break;
        }

      g_object_unref (info);
    }

  /* release the thumbnail cache */
  g_object_unref (thumbnail_cache);

  /* continue if there were no errors yet */
  if (G_LIKELY (err == NULL))
    {
      /* check destination */
      if (!thunar_transfer_job_verify_destination (transfer_job, &err))
        {
          if (err != NULL)
            {
              g_propagate_error (error, err);
              return FALSE;
            }
          else
            {
              /* pretend nothing happened */
              return TRUE;
            }
        }

      thunar_transfer_job_verify_devices (transfer_job);

      /* transfer starts now */
      transfer_job->start_time = g_get_real_time ();

      /* perform the copy recursively for all source transfer nodes */
      for (sp = transfer_job->source_node_list, tp = transfer_job->target_file_list;
           sp != NULL && tp != NULL && err == NULL;
           sp = sp->next, tp = tp->next)
        {
          thunar_transfer_job_copy_node (transfer_job, sp->data, tp->data, NULL,
                                         &new_files_list, &err);
        }
    }

  /* check if we failed */
  if (G_UNLIKELY (err != NULL))
    {
      g_propagate_error (error, err);
      return FALSE;
    }
  else
    {
      thunar_job_new_files (THUNAR_JOB (job), new_files_list);
      thunar_g_file_list_free (new_files_list);
      return TRUE;
    }
}



static void
thunar_transfer_node_free (gpointer data)
{
  ThunarTransferNode *node = data;
  ThunarTransferNode *next;

  /* free all nodes in a row */
  while (node != NULL)
    {
      /* free all children of this node */
      thunar_transfer_node_free (node->children);

      /* determine the next node */
      next = node->next;

      /* drop the source file of this node */
      g_object_unref (node->source_file);

      /* release the resources of this node */
      g_slice_free (ThunarTransferNode, node);

      /* continue with the next node */
      node = next;
    }
}



ThunarJob *
thunar_transfer_job_new (GList                *source_node_list,
                         GList                *target_file_list,
                         ThunarTransferJobType type)
{
  ThunarTransferNode *node;
  ThunarTransferJob  *job;
  GList              *sp;
  GList              *tp;

  _thunar_return_val_if_fail (source_node_list != NULL, NULL);
  _thunar_return_val_if_fail (target_file_list != NULL, NULL);
  _thunar_return_val_if_fail (g_list_length (source_node_list) == g_list_length (target_file_list), NULL);

  job = g_object_new (THUNAR_TYPE_TRANSFER_JOB, NULL);
  job->type = type;

  /* add a transfer node for each source path and a matching target parent path */
  for (sp = source_node_list, tp = target_file_list;
       sp != NULL;
       sp = sp->next, tp = tp->next)
    {
      /* make sure we don't transfer root directories. this should be prevented in the GUI */
      if (G_UNLIKELY (thunar_g_file_is_root (sp->data) || thunar_g_file_is_root (tp->data)))
        continue;

      /* only process non-equal pairs unless we're copying */
      if (G_LIKELY (type != THUNAR_TRANSFER_JOB_MOVE || !g_file_equal (sp->data, tp->data)))
        {
          /* append transfer node for this source file */
          node = g_slice_new0 (ThunarTransferNode);
          node->source_file = g_object_ref (sp->data);
          node->replace_confirmed = FALSE;
          job->source_node_list = g_list_append (job->source_node_list, node);

          /* append target file */
          job->target_file_list = thunar_g_file_list_append (job->target_file_list, tp->data);
        }
    }

  /* make sure we didn't mess things up */
  _thunar_assert (g_list_length (job->source_node_list) == g_list_length (job->target_file_list));

  return THUNAR_JOB (job);
}



gchar *
thunar_transfer_job_get_status (ThunarTransferJob *job)
{
  gchar             *total_size_str;
  gchar             *total_progress_str;
  gchar             *transfer_rate_str;
  GString           *status;
  gulong             remaining_time;

  _thunar_return_val_if_fail (THUNAR_IS_TRANSFER_JOB (job), NULL);

  status = g_string_sized_new (100);

  /* transfer status like "22.6MB of 134.1MB" */
  total_size_str = g_format_size_full (job->total_size, job->file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
  total_progress_str = g_format_size_full (job->total_progress, job->file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
  g_string_append_printf (status, _("%s of %s"), total_progress_str, total_size_str);
  g_free (total_size_str);
  g_free (total_progress_str);

  /* show time and transfer rate after 10 seconds */
  if (job->transfer_rate > 0
      && (job->last_update_time - job->start_time) > MINIMUM_TRANSFER_TIME)
    {
      /* remaining time based on the transfer speed */
      transfer_rate_str = g_format_size_full (job->transfer_rate, job->file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
      remaining_time = (job->total_size - job->total_progress) / job->transfer_rate;

      if (remaining_time > 0)
        {
          /* insert long dash */
          g_string_append (status, " \xE2\x80\x94 ");

          if (remaining_time > 60 * 60)
            {
              remaining_time = (gulong) (remaining_time / (60 * 60));
              g_string_append_printf (status, ngettext ("%lu hour remaining (%s/sec)",
                                                        "%lu hours remaining (%s/sec)",
                                                        remaining_time),
                                                        remaining_time, transfer_rate_str);
            }
          else if (remaining_time > 60)
            {
              remaining_time = (gulong) (remaining_time / 60);
              g_string_append_printf (status, ngettext ("%lu minute remaining (%s/sec)",
                                                        "%lu minutes remaining (%s/sec)",
                                                        remaining_time),
                                                        remaining_time, transfer_rate_str);
            }
          else
            {
              g_string_append_printf (status, ngettext ("%lu second remaining (%s/sec)",
                                                        "%lu seconds remaining (%s/sec)",
                                                        remaining_time),
                                                        remaining_time, transfer_rate_str);
            }
        }

      g_free (transfer_rate_str);
    }

  return g_string_free (status, FALSE);
}

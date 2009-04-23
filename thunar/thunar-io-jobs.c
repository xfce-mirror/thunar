/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>

#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-io-scan-directory.h>
#include <thunar/thunar-job.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-simple-job.h>
#include <thunar/thunar-transfer-job.h>



static GList *
_tij_collect_nofollow (ThunarJob *job,
                       GList     *base_file_list,
                       GError   **error)
{
  GError *err = NULL;
  GList  *child_file_list = NULL;
  GList  *file_list = NULL;
  GList  *lp;

  /* recursively collect the files */
  for (lp = base_file_list; 
       err == NULL && lp != NULL && !thunar_job_is_cancelled (job); 
       lp = lp->next)
    {
      /* try to scan the directory */
      child_file_list = thunar_io_scan_directory (job, lp->data, 
                                                  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, 
                                                  TRUE, &err);

      /* prepend the new files to the existing list */
      file_list = g_file_list_prepend (file_list, lp->data);
      file_list = g_list_concat (child_file_list, file_list);
    }

  /* check if we failed */
  if (G_UNLIKELY (err != NULL || thunar_job_is_cancelled (job)))
    {
      if (thunar_job_set_error_if_cancelled (job, error))
        g_error_free (err);
      else
        g_propagate_error (error, err);

      /* release the collected files */
      g_file_list_free (file_list);

      return NULL;
    }

  return file_list;
}



static gboolean
_thunar_io_jobs_create (ThunarJob   *job,
                        GValueArray *param_values,
                        GError     **error)
{
  GFileOutputStream *stream;
  ThunarJobResponse  response = THUNAR_JOB_RESPONSE_CANCEL;
  GFileInfo         *info;
  GError            *err = NULL;
  GList             *file_list;
  GList             *lp;
  gchar             *basename;
  gchar             *display_name;
  
  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  _thunar_return_val_if_fail (param_values != NULL, FALSE);
  _thunar_return_val_if_fail (param_values->n_values == 1, FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* get the file list */
  file_list = g_value_get_boxed (g_value_array_get_nth (param_values, 0));

  /* we know the total amount of files to be processed */
  thunar_job_set_total_files (job, file_list);

  /* iterate over all files in the list */
  for (lp = file_list; 
       err == NULL && lp != NULL && !thunar_job_is_cancelled (job); 
       lp = lp->next)
    {
      g_assert (G_IS_FILE (lp->data));

      /* update progress information */
      thunar_job_processing_file (job, lp);

again:
      /* try to create the file */
      stream = g_file_create (lp->data, 
                              G_FILE_CREATE_NONE, 
                              thunar_job_get_cancellable (job),
                              &err);

      /* abort if the job was cancelled */
      if (thunar_job_is_cancelled (job))
        break;

      /* check if creating failed */
      if (stream == NULL)
        {
          if (err->code == G_IO_ERROR_EXISTS)
            {
              g_clear_error (&err);

              /* the file already exists, query its display name */
              info = g_file_query_info (lp->data,
                                        G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                        G_FILE_QUERY_INFO_NONE,
                                        thunar_job_get_cancellable (job),
                                        NULL);

              /* abort if the job was cancelled */
              if (thunar_job_is_cancelled (job))
                break;

              /* determine the display name, using the basename as a fallback */
              if (info != NULL)
                {
                  display_name = g_strdup (g_file_info_get_display_name (info));
                  g_object_unref (info);
                }
              else
                {
                  basename = g_file_get_basename (lp->data);
                  display_name = g_filename_display_name (basename);
                  g_free (basename);
                }

              /* ask the user whether he wants to overwrite the existing file */
              response = thunar_job_ask_overwrite (job, _("The file \"%s\" already exists"), 
                                                   display_name);

              /* check if we should overwrite */
              if (G_UNLIKELY (response == THUNAR_JOB_RESPONSE_YES))
                {
                  /* try to remove the file. fail if not possible */
                  if (g_file_delete (lp->data, thunar_job_get_cancellable (job), &err))
                    goto again;
                }
              
              /* clean up */
              g_free (display_name);
            }
          else 
            {
              /* determine display name of the file */
              basename = g_file_get_basename (lp->data);
              display_name = g_filename_display_basename (basename);
              g_free (basename);

              /* ask the user whether to skip/retry this path (cancels the job if not) */
              response = thunar_job_ask_skip (job, _("Failed to create empty file \"%s\": %s"),
                                              display_name, err->message);
              g_free (display_name);

              g_clear_error (&err);

              /* go back to the beginning if the user wants to retry */
              if (G_UNLIKELY (response == THUNAR_JOB_RESPONSE_RETRY))
                goto again;
            }
        }
      else
        g_object_unref (stream);
    }

  /* check if we have failed */
  if (G_UNLIKELY (err != NULL))
    {
      g_propagate_error (error, err);
      return FALSE;
    }

  /* check if the job was cancelled */
  if (G_UNLIKELY (thunar_job_is_cancelled (job)))
    return FALSE;

  /* emit the "new-files" signal with the given file list */
  thunar_job_new_files (job, file_list);

  return TRUE;
}



ThunarJob *
thunar_io_jobs_create_files (GList *file_list)
{
  return thunar_simple_job_launch (_thunar_io_jobs_create, 1,
                                   G_TYPE_FILE_LIST, file_list);
}



static gboolean
_thunar_io_jobs_mkdir (ThunarJob   *job,
                       GValueArray *param_values,
                       GError     **error)
{
  ThunarJobResponse response;
  GFileInfo        *info;
  GError           *err = NULL;
  GList            *file_list;
  GList            *lp;
  gchar            *basename;
  gchar            *display_name;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  _thunar_return_val_if_fail (param_values != NULL, FALSE);
  _thunar_return_val_if_fail (param_values->n_values == 1, FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  file_list = g_value_get_boxed (g_value_array_get_nth (param_values, 0));

  /* we know the total list of files to process */
  thunar_job_set_total_files (job, file_list);

  for (lp = file_list; 
       err == NULL && lp != NULL && !thunar_job_is_cancelled (job);
       lp = lp->next)
    {
      g_assert (G_IS_FILE (lp->data));

      /* update progress information */
      thunar_job_processing_file (job, lp);

again:
      /* try to create the directory */
      if (!g_file_make_directory (lp->data, thunar_job_get_cancellable (job), &err))
        {
          if (err->code == G_IO_ERROR_EXISTS)
            {
              g_error_free (err);
              err = NULL;

              /* abort if the job was cancelled */
              if (thunar_job_is_cancelled (job))
                break;

              /* the file already exists, query its display name */
              info = g_file_query_info (lp->data,
                                        G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                        G_FILE_QUERY_INFO_NONE,
                                        thunar_job_get_cancellable (job),
                                        NULL);

              /* abort if the job was cancelled */
              if (thunar_job_is_cancelled (job))
                break;

              /* determine the display name, using the basename as a fallback */
              if (info != NULL)
                {
                  display_name = g_strdup (g_file_info_get_display_name (info));
                  g_object_unref (info);
                }
              else
                {
                  basename = g_file_get_basename (lp->data);
                  display_name = g_filename_display_name (basename);
                  g_free (basename);
                }

              /* ask the user whether he wants to overwrite the existing file */
              response = thunar_job_ask_overwrite (job, _("The file \"%s\" already exists"),
                                                   display_name);

              /* check if we should overwrite it */
              if (G_UNLIKELY (response == THUNAR_JOB_RESPONSE_YES))
                {
                  /* try to remove the file, fail if not possible */
                  if (g_file_delete (lp->data, thunar_job_get_cancellable (job), &err))
                    goto again;
                }

              /* clean up */
              g_free (display_name);
            }
          else
            {
              /* determine the display name of the file */
              basename = g_file_get_basename (lp->data);
              display_name = g_filename_display_basename (basename);
              g_free (basename);

              /* ask the user whether to skip/retry this path (cancels the job if not) */
              response = thunar_job_ask_skip (job, _("Failed to create directory \"%s\": %s"),
                                              display_name, err->message);
              g_free (display_name);

              g_error_free (err);
              err = NULL;

              /* go back to the beginning if the user wants to retry */
              if (G_UNLIKELY (response == THUNAR_JOB_RESPONSE_RETRY))
                goto again;
            }
        }
    }

  /* check if we have failed */
  if (G_UNLIKELY (err != NULL))
    {
      g_propagate_error (error, err);
      return FALSE;
    }

  /* check if the job was cancelled */
  if (G_UNLIKELY (thunar_job_is_cancelled (job)))
    return FALSE;

  /* emit the "new-files" signal with the given file list */
  thunar_job_new_files (job, file_list);
  
  return TRUE;
}



ThunarJob *
thunar_io_jobs_make_directories (GList *file_list)
{
  return thunar_simple_job_launch (_thunar_io_jobs_mkdir, 1,
                                   G_TYPE_FILE_LIST, file_list);
}



static gboolean
_thunar_io_jobs_unlink (ThunarJob   *job,
                        GValueArray *param_values,
                        GError     **error)
{
  ThunarJobResponse response;
  GFileInfo        *info;
  GError           *err = NULL;
  GList            *file_list;
  GList            *lp;
  gchar            *basename;
  gchar            *display_name;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  _thunar_return_val_if_fail (param_values != NULL, FALSE);
  _thunar_return_val_if_fail (param_values->n_values == 1, FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* get the file list */
  file_list = g_value_get_boxed (g_value_array_get_nth (param_values, 0));

  /* tell the user that we're preparing to unlink the files */
  thunar_job_info_message (job, _("Preparing..."));

  /* recursively collect files for removal, not following any symlinks */
  file_list = _tij_collect_nofollow (job, file_list, &err);

  /* free the file list and fail if there was an error or the job was cancelled */
  if (err != NULL || thunar_job_is_cancelled (job))
    {
      if (thunar_job_set_error_if_cancelled (job, error))
        g_error_free (err);
      else
        g_propagate_error (error, err);

      g_file_list_free (file_list);
      return FALSE;
    }

  /* we know the total list of files to process */
  thunar_job_set_total_files (job, file_list);

  /* remove all the files */
  for (lp = file_list; lp != NULL && !thunar_job_is_cancelled (job); lp = lp->next)
    {
      g_assert (G_IS_FILE (lp->data));

      /* skip root folders which cannot be deleted anyway */
      if (G_UNLIKELY (g_file_is_root (lp->data)))
        continue;

again:
      /* try to delete the file */
      if (G_UNLIKELY (!g_file_delete (lp->data, thunar_job_get_cancellable (job), &err)))
        {
          /* query the file info for the display name */
          info = g_file_query_info (lp->data, 
                                    G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                    G_FILE_QUERY_INFO_NONE, 
                                    thunar_job_get_cancellable (job), 
                                    NULL);

          /* abort if the job was cancelled */
          if (thunar_job_is_cancelled (job))
            {
              g_clear_error (&err);
              break;
            }

          /* determine the display name, using the basename as a fallback */
          if (info != NULL)
            {
              display_name = g_strdup (g_file_info_get_display_name (info));
              g_object_unref (info);
            }
          else
            {
              basename = g_file_get_basename (lp->data);
              display_name = g_filename_display_name (basename);
              g_free (basename);
            }

          /* ask the user whether he wants to skip this file */
          response = thunar_job_ask_skip (job, _("Could not delete file \"%s\": %s"), 
                                          display_name, err->message);
          g_free (display_name);

          /* clear the error */
          g_clear_error (&err);

          /* check whether to retry */
          if (G_UNLIKELY (response == THUNAR_JOB_RESPONSE_RETRY))
            goto again;
        }
    }

  /* release the file list */
  g_file_list_free (file_list);

  if (thunar_job_set_error_if_cancelled (job, error))
    return FALSE;
  else
    return TRUE;
}



ThunarJob *
thunar_io_jobs_unlink_files (GList *file_list)
{
  return thunar_simple_job_launch (_thunar_io_jobs_unlink, 1,
                                   G_TYPE_FILE_LIST, file_list);
}



ThunarJob *
thunar_io_jobs_move_files (GList *source_file_list,
                           GList *target_file_list)
{
  _thunar_return_val_if_fail (source_file_list != NULL, NULL);
  _thunar_return_val_if_fail (target_file_list != NULL, NULL);
  _thunar_return_val_if_fail (g_list_length (source_file_list) == g_list_length (target_file_list), NULL);
  
  return thunar_job_launch (thunar_transfer_job_new (source_file_list, 
                                                     target_file_list, 
                                                     THUNAR_TRANSFER_JOB_MOVE));
}



ThunarJob *
thunar_io_jobs_copy_files (GList *source_file_list,
                           GList *target_file_list)
{
  _thunar_return_val_if_fail (source_file_list != NULL, NULL);
  _thunar_return_val_if_fail (target_file_list != NULL, NULL);
  _thunar_return_val_if_fail (g_list_length (source_file_list) == g_list_length (target_file_list), NULL);

  return thunar_job_launch (thunar_transfer_job_new (source_file_list,
                                                     target_file_list,
                                                     THUNAR_TRANSFER_JOB_COPY));
}



static gboolean
_thunar_io_jobs_link (ThunarJob   *job,
                      GValueArray *param_values,
                      GError     **error)
{
  ThunarJobResponse response;
  GError           *err = NULL;
  GList            *new_files_list = NULL;
  GList            *source_file_list;
  GList            *sp;
  GList            *target_file_list;
  GList            *tp;
  gchar            *basename;
  gchar            *display_name;
  gchar            *source_path;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  _thunar_return_val_if_fail (param_values != NULL, FALSE);
  _thunar_return_val_if_fail (param_values->n_values == 2, FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  source_file_list = g_value_get_boxed (g_value_array_get_nth (param_values, 0));
  target_file_list = g_value_get_boxed (g_value_array_get_nth (param_values, 1));

  /* we know the total list of paths to process */
  thunar_job_set_total_files (job, source_file_list);

  /* process all files */
  for (sp = source_file_list, tp = target_file_list;
       err == NULL && sp != NULL && tp != NULL;
       sp = sp->next, tp = tp->next)
    {
      _thunar_assert (G_IS_FILE (sp->data));
      _thunar_assert (G_IS_FILE (tp->data));

      /* update progress information */
      thunar_job_processing_file (job, sp);

again:
      source_path = g_file_get_path (sp->data);

      if (G_LIKELY (source_path != NULL))
        {
          /* try to create the symlink */
          g_file_make_symbolic_link (tp->data, source_path, 
                                     thunar_job_get_cancellable (job),
                                     &err);

          g_free (source_path);

          if (err == NULL)
            new_files_list = g_file_list_prepend (new_files_list, sp->data);
          else
            {
              /* check if we have an error from which we can recover */
              if (err->domain == G_IO_ERROR && err->code == G_IO_ERROR_EXISTS)
                {
                  /* ask the user whether he wants to overwrite the existing file */
                  response = thunar_job_ask_overwrite (job, "%s", err->message);

                  /* release the error */
                  g_clear_error (&err);

                  /* try to delete the file */
                  if (G_LIKELY (response == THUNAR_JOB_RESPONSE_YES))
                    {
                      /* try to remove the target file (fail if not possible) */
                      if (g_file_delete (tp->data, thunar_job_get_cancellable (job), &err))
                        goto again;
                    }
                }
            }
        }
      else
        {
          basename = g_file_get_basename (sp->data);
          display_name = g_filename_display_name (basename);
          g_set_error (&err, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                       _("Could not create symbolic link to \"%s\" "
                         "because it is not a local file"), display_name);
          g_free (display_name);
          g_free (basename);
        }
    }

  if (err != NULL)
    {
      g_file_list_free (new_files_list);
      g_propagate_error (error, err);
      return FALSE;
    }
  else
    {
      thunar_job_new_files (job, new_files_list);
      g_file_list_free (new_files_list);
      return TRUE;
    }
}



ThunarJob *
thunar_io_jobs_link_files (GList *source_file_list,
                           GList *target_file_list)
{
  _thunar_return_val_if_fail (source_file_list != NULL, NULL);
  _thunar_return_val_if_fail (target_file_list != NULL, NULL);
  _thunar_return_val_if_fail (g_list_length (source_file_list) == g_list_length (target_file_list), NULL);

  return thunar_simple_job_launch (_thunar_io_jobs_link, 2,
                                   G_TYPE_FILE_LIST, source_file_list,
                                   G_TYPE_FILE_LIST, target_file_list);
}



static gboolean
_thunar_io_jobs_trash (ThunarJob   *job,
                       GValueArray *param_values,
                       GError     **error)
{
  GError *err = NULL;
  GList  *file_list;
  GList  *lp;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  _thunar_return_val_if_fail (param_values != NULL, FALSE);
  _thunar_return_val_if_fail (param_values->n_values == 1, FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  file_list = g_value_get_boxed (g_value_array_get_nth (param_values, 0));

  if (thunar_job_set_error_if_cancelled (job, error))
    return FALSE;

  for (lp = file_list; err == NULL && lp != NULL; lp = lp->next)
    {
      _thunar_assert (G_IS_FILE (lp->data));
      g_file_trash (lp->data, thunar_job_get_cancellable (job), &err);
    }

  if (err != NULL)
    {
      g_propagate_error (error, err);
      return FALSE;
    }
  else
    {
      return TRUE;
    }
}



ThunarJob *
thunar_io_jobs_trash_files (GList *file_list)
{
  _thunar_return_val_if_fail (file_list != NULL, NULL);

  return thunar_simple_job_launch (_thunar_io_jobs_trash, 1,
                                   G_TYPE_FILE_LIST, file_list);
}

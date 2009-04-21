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
#include <thunar/thunar-job.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-simple-job.h>



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
  _thunar_return_val_if_fail (param_values->n_values > 0, FALSE);
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
              g_error_free (err);
              err = NULL;

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

              g_error_free (err);
              err = NULL;

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

  /* TODO emit the "new-files" signal with the given file list 
  thunar_job_new_files (job, file_list);
  */

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
  _thunar_return_val_if_fail (param_values->n_values > 0, FALSE);
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

  /* TODO emit the "new-files" signal with the given file list 
  thunar_job_new_files (job, file_list);
  */

  return TRUE;
}



ThunarJob *
thunar_io_jobs_make_directories (GList *file_list)
{
  return thunar_simple_job_launch (_thunar_io_jobs_mkdir, 1,
                                   G_TYPE_FILE_LIST, file_list);
}

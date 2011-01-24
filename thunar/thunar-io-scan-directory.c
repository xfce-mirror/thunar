/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
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

#include <exo/exo.h>

#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-job.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-io-scan-directory.h>



GList *
thunar_io_scan_directory (ThunarJob          *job,
                          GFile              *file,
                          GFileQueryInfoFlags flags,
                          gboolean            recursively,
                          gboolean            unlinking,
                          GError            **error)
{
  GFileEnumerator *enumerator;
  GFileInfo       *info;
  GFileType        type;
  GError          *err = NULL;
  GFile           *child_file;
  GList           *child_files = NULL;
  GList           *files = NULL;
  
  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), NULL);
  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* abort if the job was cancelled */
  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return NULL;

  /* don't recurse when we are scanning prior to unlinking and the current 
   * file/dir is in the trash. In GVfs, only the top-level directories in 
   * the trash can be modified and deleted directly. See
   * http://bugzilla.xfce.org/show_bug.cgi?id=7147
   * for more information */
  if (unlinking
      && thunar_g_file_is_trashed (file)
      && !thunar_g_file_is_root (file))
    {
      return NULL;
    }

  /* query the file type */
  type = g_file_query_file_type (file, flags, exo_job_get_cancellable (EXO_JOB (job)));

  /* abort if the job was cancelled */
  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return NULL;

  /* ignore non-directory nodes */
  if (type != G_FILE_TYPE_DIRECTORY)
    return NULL;

  /* try to read from the direectory */
  enumerator = g_file_enumerate_children (file,
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          flags, exo_job_get_cancellable (EXO_JOB (job)),
                                          &err);

  /* abort if there was an error or the job was cancelled */
  if (err != NULL)
    {
      g_propagate_error (error, err);
      return NULL;
    }
        
  /* query info of the first child */
  info = g_file_enumerator_next_file (enumerator,
                                      exo_job_get_cancellable (EXO_JOB (job)), 
                                      &err);

  /* iterate over children one by one as long as there's no error */
  while (info != NULL && err == NULL && !exo_job_is_cancelled (EXO_JOB (job)))
    {
      /* create GFile for the child and prepend it to the file list */
      child_file = g_file_get_child (file, g_file_info_get_name (info));
      files = thunar_g_file_list_prepend (files, child_file);

      /* if the child is a directory and we need to recurse ... just do so */
      if (recursively && g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
        {
          child_files = thunar_io_scan_directory (job, child_file, flags, recursively, 
                                                  unlinking, &err);

          /* prepend children to the file list to make sure they're 
           * processed first (required for unlinking) */
          files = g_list_concat (child_files, files);
        }
      
      g_object_unref (child_file);
      g_object_unref (info);

      info = g_file_enumerator_next_file (enumerator, 
                                          exo_job_get_cancellable (EXO_JOB (job)), 
                                          &err);
    }

  /* release the enumerator */
  g_object_unref (enumerator);

  if (G_UNLIKELY (err != NULL))
    {
      g_propagate_error (error, err);
      thunar_g_file_list_free (files);
      return NULL;
    }
  else if (exo_job_set_error_if_cancelled (EXO_JOB (job), &err))
    {
      g_propagate_error (error, err);
      thunar_g_file_list_free (files);
      return NULL;
    }
  
  return files;
}

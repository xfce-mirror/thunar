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
#include <thunar/thunar-io-jobs-util.h>
#include <thunar/thunar-job.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-util.h>



/**
 * thunar_io_jobs_util_next_duplicate_file:
 * @job   : a #ThunarJob.
 * @file  : the source #GFile.
 * @type  : the operation type (copy or link).
 * @n     : the @n<!---->th copy/link to create the #GFile for.
 * @error : return location for errors or %NULL.
 *
 * Determines the #GFile for the next copy/link of/to @file.
 *
 * Copies of a file called X are named "X (copy 1)"
 *
 * Links follow have a bit different scheme, since the first link
 * is renamed to "link to #" and after that "link Y to X".
 *
 * If there are errors or the job was cancelled, the return value
 * will be %NULL and @error will be set.
 *
 * Return value: the #GFile referencing the @n<!---->th copy or link
 *               of @file or %NULL on error/cancellation.
 **/
GFile *
thunar_io_jobs_util_next_duplicate_file (ThunarJob *job,
                                         GFile     *file,
                                         gboolean   copy,
                                         guint      n,
                                         GError   **error)
{
  GFileInfo   *info;
  GError      *err = NULL;
  GFile       *duplicate_file = NULL;
  GFile       *parent_file = NULL;
  const gchar *old_display_name;
  gchar       *display_name;
  gchar       *file_basename;
  gchar       *dot = NULL;
  
  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), NULL);
  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);
  _thunar_return_val_if_fail (0 < n, NULL);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, NULL);
  _thunar_return_val_if_fail (!thunar_g_file_is_root (file), NULL);

  /* abort on cancellation */
  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return NULL;

  /* query the source file info / display name */
  info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                            G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                            G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                            exo_job_get_cancellable (EXO_JOB (job)), &err);

  /* abort on error */
  if (info == NULL)
    {
      g_propagate_error (error, err);
      return NULL;
    }

  old_display_name = g_file_info_get_display_name (info);
  if (copy)
    {
      /* get file extension if file is not a directory */
      if (g_file_info_get_file_type (info) != G_FILE_TYPE_DIRECTORY)
        dot = thunar_util_str_get_extension (old_display_name);

      if (dot != NULL)
        {
          file_basename = g_strndup (old_display_name, dot - old_display_name);
          /* I18N: put " (copy #) between basename and extension */
          display_name = g_strdup_printf (_("%s (copy %u)%s"), file_basename, n, dot);
          g_free(file_basename);
        }
      else
        {
          /* I18N: put " (copy #)" after filename (for files without extension) */
          display_name = g_strdup_printf (_("%s (copy %u)"), old_display_name, n);
        }
    }
  else
    {
      /* create name for link */
      if (n == 1)
        {
          /* I18N: name for first link to basename */
          display_name = g_strdup_printf (_("link to %s"), old_display_name);
        }
      else
        {
          /* I18N: name for nth link to basename */
          display_name = g_strdup_printf (_("link %u to %s"), n, old_display_name);
        }
    }

  /* create the GFile for the copy/link */
  parent_file = g_file_get_parent (file);
  duplicate_file = g_file_get_child (parent_file, display_name);
  g_object_unref (parent_file);

  /* free resources */
  g_object_unref (info);
  g_free (display_name);

  return duplicate_file;
}




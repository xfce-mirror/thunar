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



static const gchar *duplicate_names[4][2] = 
{
  /* Copy/link name for n <= 3 */
  { N_("copy of %s"),         N_("link to %s"),         },
  { N_("another copy of %s"), N_("another link to %s"), },
  { N_("third copy of %s"),   N_("third link to %s"),   },

  /* Fallback copy/link name for n >= 4 */
  { N_("%uth copy of %s"),    N_("%uth link to %s"),    },
};



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
 * Copies of a file called X are named:
 *   n  = 1: "copy of X"
 *   n  = 2: "another copy of X"
 *   n  = 3: "third copy of X"
 *   n >= 4: "@n<!---->th copy of X"
 *
 * Links follow the same naming scheme, except that they use
 * "link to X" instead of "copy of X". 
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
  GFileInfo *info;
  GError    *err = NULL;
  GFile     *duplicate_file = NULL;
  GFile     *parent_file = NULL;
  gchar     *display_name;
  gint       type_index;
  gint       name_index;
  
  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), NULL);
  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);
  _thunar_return_val_if_fail (0 < n, NULL);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, NULL);
  _thunar_return_val_if_fail (!thunar_g_file_is_root (file), NULL);

  /* abort on cancellation */
  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return NULL;

  /* query the source file info / display name */
  info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, 
                            G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                            exo_job_get_cancellable (EXO_JOB (job)), &err);

  /* abort on error */
  if (info == NULL)
    {
      g_propagate_error (error, err);
      return NULL;
    }

  /* determine the type index (copy = 0, link = 1) */
  type_index = (copy ? 0 : 1);

  /* make sure the name index is not out of bounds */
  name_index = MIN (n-1, G_N_ELEMENTS (duplicate_names)-1);
  
  /* generate the display name for the nth copy/link of the source file */
  if (name_index < (gint) G_N_ELEMENTS (duplicate_names)-1)
    {
      display_name = g_strdup_printf (gettext (duplicate_names[name_index][type_index]),
                                      g_file_info_get_display_name (info));
    }
  else
    {
      display_name = g_strdup_printf (gettext (duplicate_names[name_index][type_index]),
                                      n, g_file_info_get_display_name (info));
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




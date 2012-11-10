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

#include <thunar/thunar-io-scan-directory.h>
#include <thunar/thunar-job.h>
#include <thunar/thunar-misc-jobs.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-simple-job.h>



static gboolean
_thunar_misc_jobs_load_templates (ThunarJob  *job,
                                  GArray     *param_values,
                                  GError    **error)
{
  GtkWidget   *menu;
  GFile       *home_dir;
  GFile       *templates_dir;
  GList       *files = NULL;
  const gchar *path;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_return_val_if_fail (param_values != NULL && param_values->len == 1, FALSE);

  menu = g_value_get_object (&g_array_index (param_values, GValue, 0));
  g_object_set_data (G_OBJECT (job), "menu", menu);

  home_dir = thunar_g_file_new_for_home ();
  path = g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES);
  if (G_LIKELY (path != NULL))
    templates_dir = g_file_new_for_path (path);
  else
    templates_dir = g_file_resolve_relative_path (home_dir, "Templates");

  if (G_LIKELY (!g_file_equal (templates_dir, home_dir)))
    {
      /* load the ThunarFiles */
      files = thunar_io_scan_directory (job, templates_dir,
                                        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                        TRUE, FALSE, TRUE, NULL);
    }

  g_object_unref (templates_dir);
  g_object_unref (home_dir);

  if (files == NULL || exo_job_is_cancelled (EXO_JOB (job)))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, 
                   _("No templates installed"));

      return FALSE;
    }
  else
    {
      if (!thunar_job_files_ready (job, files))
        thunar_g_file_list_free (files);

      return TRUE;
    }
}



ThunarJob *
thunar_misc_jobs_load_template_files (GtkWidget *menu)
{
  return thunar_simple_job_launch (_thunar_misc_jobs_load_templates, 1,
                                   GTK_TYPE_MENU, menu);
}

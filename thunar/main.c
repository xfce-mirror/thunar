/* $Id$ */
/*-
 * Copyright (c) 2005 os-cillation e.K.
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <thunar/thunar-window.h>



int
main (int argc, char **argv)
{
  ThunarVfsURI *uri;
  ThunarFolder *folder;
  const gchar  *path;
  GtkWidget    *window;
  GError       *error = NULL;

  /* Do NOT remove this line for now, If something doesn't work,
   * fix your code instead!
   */
  g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);

  /* initialize the GLib thread support */
  if (!g_thread_supported ())
    g_thread_init (NULL);

  /* initialize Gtk+ */
  gtk_init (&argc, &argv);

  path = (argc > 1) ? argv[1] : xfce_get_homedir ();

  uri = thunar_vfs_uri_new_for_path (path);
  folder = thunar_folder_get_for_uri (uri, &error);
  g_object_unref (G_OBJECT (uri));

  if (folder == NULL)
    {
      fprintf (stderr, "%s: Failed to open `%s': %s\n",
               argv[0], path, error->message);
      g_error_free (error);
      return EXIT_FAILURE;
    }

  window = thunar_window_new_with_folder (folder);
  g_signal_connect (G_OBJECT (window), "destroy",
                    G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show (window);

  g_object_unref (G_OBJECT (folder));

  gtk_main ();

  return EXIT_SUCCESS;
}

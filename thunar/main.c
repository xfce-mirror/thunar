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

#include <thunar/thunar-application.h>



int
main (int argc, char **argv)
{
  ThunarApplication *application;
  ThunarVfsPath     *path;
  ThunarFile        *file = NULL;
  GError            *error = NULL;

  /* setup translation domain */
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* setup application name */
  g_set_application_name (_("Thunar"));

#ifdef G_ENABLE_DEBUG
  /* Do NOT remove this line for now, If something doesn't work,
   * fix your code instead!
   */
  g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
#endif

  /* initialize the GLib thread support */
  if (!g_thread_supported ())
    g_thread_init (NULL);

  /* initialize Gtk+ */
  gtk_init (&argc, &argv);

  /* initialize the ThunarVFS library */
  thunar_vfs_init ();

  /* acquire a reference on the global application */
  application = thunar_application_get ();

  path = (argc > 1) ? thunar_vfs_path_new (argv[1], &error) : thunar_vfs_path_get_for_home ();
  if (G_LIKELY (path != NULL))
    {
      file = thunar_file_get_for_path (path, &error);
      thunar_vfs_path_unref (path);
    }

  if (path == NULL || file == NULL)
    {
      fprintf (stderr, "%s: Failed to open `%s': %s\n",
               argv[0], (argc > 1) ? argv[1] : xfce_get_homedir (),
               error->message);
      g_error_free (error);
      return EXIT_FAILURE;
    }

  /* open the first window */
  thunar_application_open_window (application, file, NULL);

  g_object_unref (G_OBJECT (file));

  gtk_main ();

  /* release the application reference */
  g_object_unref (G_OBJECT (application));

  /* shutdown the VFS library */
  thunar_vfs_shutdown ();

  return EXIT_SUCCESS;
}

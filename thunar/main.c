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
#include <thunar/thunar-desktop-window.h>



int
main (int argc, char **argv)
{
  ThunarApplication *application;
  ThunarVfsURI      *uri;
  const gchar       *path;
  ThunarFile        *file = NULL;
  GError            *error = NULL;

  /* setup translation domain */
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* setup application name */
  g_set_application_name (_("Thunar"));

#ifndef G_DISABLE_CHECKS
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

  if (argc >= 2 && exo_str_is_equal (argv[1], "--desktop"))
    {
      GtkWidget *window = thunar_desktop_window_new ();
      gtk_widget_show (window);
      gtk_main ();
      return 0;
    }

  /* acquire a reference on the global application */
  application = thunar_application_get ();

  path = (argc > 1) ? argv[1] : xfce_get_homedir ();

  uri = thunar_vfs_uri_new (path, &error);
  if (G_LIKELY (uri != NULL))
    {
      file = thunar_file_get_for_uri (uri, &error);
      thunar_vfs_uri_unref (uri);
    }

  if (uri == NULL || file == NULL)
    {
      fprintf (stderr, "%s: Failed to open `%s': %s\n",
               argv[0], path, error->message);
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

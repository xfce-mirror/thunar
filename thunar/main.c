/* $Id$ */
/*-
 * Copyright (c) 2005-2006 os-cillation e.K.
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

#include <glib/gstdio.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-dbus-client.h>
#include <thunar/thunar-dbus-service.h>
#include <thunar/thunar-stock.h>



/* --- globals --- */
static gboolean opt_daemon = FALSE;


/* --- command line options --- */
static GOptionEntry option_entries[] =
{
  { "daemon", 0, 0, G_OPTION_ARG_NONE, &opt_daemon, N_ ("Run in daemon mode"), NULL, },
  { NULL, },
};



int
main (int argc, char **argv)
{
#ifdef HAVE_DBUS
  ThunarDBusService *dbus_service;
#endif
  ThunarApplication *application;
  GError            *error = NULL;
  gchar             *working_directory = NULL;
  gchar            **filenames = NULL;

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

  /* initialize Gtk+ */
  if (!gtk_init_with_args (&argc, &argv, _("[FILES...]"), option_entries, GETTEXT_PACKAGE, &error))
    {
      /* check if we have an error message */
      if (G_LIKELY (error == NULL))
        {
          /* no error message, the GUI initialization failed */
          const gchar *display_name = gdk_get_display_arg_name ();
          g_fprintf (stderr, _("Thunar: Failed to open display: %s\n"), (display_name != NULL) ? display_name : " ");
        }
      else
        {
          /* yep, there's an error, so print it */
          g_fprintf (stderr, _("Thunar: %s\n"), error->message);
          g_error_free (error);
        }
      return EXIT_FAILURE;
    }

  /* do not try to connect to a running instance, or
   * open any windows if run as daemon.
   */
  if (G_LIKELY (!opt_daemon))
    {
      /* determine the current working directory */
      working_directory = g_get_current_dir ();

      /* check if atleast one filename was specified */
      if (G_LIKELY (argc > 1))
        {
          /* use the specified filenames */
          filenames = g_strdupv (argv + 1);
        }
      else
        {
          /* use the current working directory */
          filenames = g_new (gchar *, 2);
          filenames[0] = g_strdup (working_directory);
          filenames[1] = NULL;
        }

#ifdef HAVE_DBUS
      /* try to reuse an existing instance */
      if (thunar_dbus_client_launch_files (working_directory, filenames, NULL, NULL))
        {
          /* that worked, let's get outa here */
          g_free (working_directory);
          g_strfreev (filenames);
          return EXIT_SUCCESS;
        }
#endif
    }

  /* initialize the GLib thread support */
  if (!g_thread_supported ())
    g_thread_init (NULL);

  /* initialize the ThunarVFS library */
  thunar_vfs_init ();

  /* initialize the thunar stock items/icons */
  thunar_stock_init ();

  /* acquire a reference on the global application */
  application = thunar_application_get ();

  /* use the Thunar icon as default for new windows */
  gtk_window_set_default_icon_name ("Thunar");

  /* if not in daemon mode, try to process the filenames here */
  if (G_LIKELY (!opt_daemon))
    {
      /* try to process the given filenames */
      if (!thunar_application_process_filenames (application, working_directory, filenames, NULL, &error))
        {
          g_fprintf (stderr, "Thunar: %s\n", error->message);
          g_object_unref (G_OBJECT (application));
          thunar_vfs_shutdown ();
          g_error_free (error);
          return EXIT_FAILURE;
        }

      /* release working directory and filenames */
      g_free (working_directory);
      g_strfreev (filenames);
    }
  else
    {
      /* run in daemon mode, since we were started by the message bus */
      thunar_application_set_daemon (application, TRUE);
    }

  /* do not enter the main loop, unless we have atleast one window or we are in daemon mode */
  if (thunar_application_has_windows (application) || thunar_application_get_daemon (application))
    {
      /* attach the D-BUS service */
#ifdef HAVE_DBUS
      dbus_service = g_object_new (THUNAR_TYPE_DBUS_SERVICE, NULL);
#endif

      /* enter the main loop */
      gtk_main ();

      /* detach the D-BUS service */
#ifdef HAVE_DBUS
      g_object_unref (G_OBJECT (dbus_service));
#endif
    }

  /* release the application reference */
  g_object_unref (G_OBJECT (application));

  /* shutdown the VFS library */
  thunar_vfs_shutdown ();

  return EXIT_SUCCESS;
}

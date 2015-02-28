/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2011 os-cillation e.K.
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
#ifdef HAVE_GIO_UNIX
#include <gio/gdesktopappinfo.h>
#endif

#include <xfconf/xfconf.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-dbus-client.h>
#include <thunar/thunar-dbus-service.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-notify.h>
#include <thunar/thunar-session-client.h>
#include <thunar/thunar-stock.h>
#include <thunar/thunar-preferences.h>



/* --- globals --- */
static gboolean opt_bulk_rename = FALSE;
static gboolean opt_daemon = FALSE;
static gchar   *opt_sm_client_id = NULL;
static gboolean opt_quit = FALSE;
static gboolean opt_version = FALSE;



/* --- command line options --- */
static GOptionEntry option_entries[] =
{
  { "bulk-rename", 'B', 0, G_OPTION_ARG_NONE, &opt_bulk_rename, N_ ("Open the bulk rename dialog"), NULL, },
#ifdef HAVE_DBUS
  { "daemon", 0, 0, G_OPTION_ARG_NONE, &opt_daemon, N_ ("Run in daemon mode"), NULL, },
#else
  { "daemon", 0, 0, G_OPTION_ARG_NONE, &opt_daemon, N_ ("Run in daemon mode (not supported)"), NULL, },
#endif
  { "sm-client-id", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &opt_sm_client_id, NULL, NULL, },
#ifdef HAVE_DBUS
  { "quit", 'q', 0, G_OPTION_ARG_NONE, &opt_quit, N_ ("Quit a running Thunar instance"), NULL, },
#else
  { "quit", 'q', 0, G_OPTION_ARG_NONE, &opt_quit, N_ ("Quit a running Thunar instance (not supported)"), NULL, },
#endif
  { "version", 'V', 0, G_OPTION_ARG_NONE, &opt_version, N_ ("Print version information and exit"), NULL, },
  { NULL, },
};



static gboolean
thunar_delayed_exit_check (gpointer user_data)
{
  ThunarApplication *application = user_data;

  _thunar_return_val_if_fail (THUNAR_IS_APPLICATION (application), FALSE);

  /* call this function again later if the application is still processing the
   * command line arguments */
  if (thunar_application_is_processing (application))
    return TRUE;

  /* the application has processed all command line arguments. don't call
   * this function again if it could load at least one of them */
  if (thunar_application_has_windows (application))
    {
      return FALSE;
    }
  else
    {
      /* no command line arguments opened in Thunar, exit now */
      gtk_main_quit ();

      /* don't call this function again */
      return FALSE;
    }

}



int
main (int argc, char **argv)
{
  ThunarSessionClient *session_client;
#ifdef HAVE_DBUS
  ThunarDBusService   *dbus_service = NULL;
#endif
  ThunarApplication   *application;
  GError              *error = NULL;
  gchar               *working_directory;
  gchar              **filenames = NULL;
  const gchar         *startup_id;

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

#if !GLIB_CHECK_VERSION (2, 32, 0)
  /* initialize the GThread system */
  if (!g_thread_supported ())
    g_thread_init (NULL);
#endif

  /* get the startup notification id */
  startup_id = g_getenv ("DESKTOP_STARTUP_ID");

  /* initialize Gtk+ */
  if (!gtk_init_with_args (&argc, &argv, _("[FILES...]"), option_entries, GETTEXT_PACKAGE, &error))
    {
      /* check if we have an error message */
      if (G_LIKELY (error == NULL))
        {
          /* no error message, the GUI initialization failed */
          const gchar *display_name = gdk_get_display_arg_name ();
          g_printerr (_("Thunar: Failed to open display: %s\n"), (display_name != NULL) ? display_name : " ");
        }
      else
        {
          /* yep, there's an error, so print it */
          g_printerr (_("Thunar: %s\n"), error->message);
          g_error_free (error);
        }
      return EXIT_FAILURE;
    }

  /* check if we should print version information */
  if (G_UNLIKELY (opt_version))
    {
      g_print ("%s %s (Xfce %s)\n\n", PACKAGE_NAME, PACKAGE_VERSION, xfce_version_string ());
      g_print ("%s\n", "Copyright (c) 2004-2015");
      g_print ("\t%s\n\n", _("The Thunar development team. All rights reserved."));
      g_print ("%s\n\n", _("Written by Benedikt Meurer <benny@xfce.org>."));
      g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
      g_print ("\n");
      return EXIT_SUCCESS;
    }

  /* initialize xfconf */
  if (!xfconf_init (&error))
    {
      g_printerr (PACKAGE_NAME ": Failed to initialize Xfconf: %s\n\n", error->message);
      g_clear_error (&error);

      /* disable get/set properties */
      thunar_preferences_xfconf_init_failed ();
    }

#ifdef HAVE_GIO_UNIX
#if !GLIB_CHECK_VERSION (2, 42, 0)
  /* set desktop environment for app infos */
  g_desktop_app_info_set_desktop_env ("XFCE");
#endif
#endif

  /* register additional transformation functions */
  thunar_g_initialize_transformations ();

#ifdef HAVE_DBUS
  /* check if we should terminate a running Thunar instance */
  if (G_UNLIKELY (opt_quit))
    {
      /* try to terminate whatever is running */
      if (!thunar_dbus_client_terminate (&error))
        {
          g_printerr ("Thunar: Failed to terminate running instance: %s\n", error->message);
          g_error_free (error);
          return EXIT_FAILURE;
        }

      return EXIT_SUCCESS;
    }
#endif

  /* determine the current working directory */
  working_directory = g_get_current_dir ();

  /* check if atleast one filename was specified, else
   * fall back to opening the current working directory
   * if daemon mode is not requested.
   */
  if (G_LIKELY (argc > 1))
    {
      /* use the specified filenames */
      filenames = g_strdupv (argv + 1);
    }
  else if (opt_bulk_rename)
    {
      /* default to an empty list */
      filenames = g_new (gchar *, 1);
      filenames[0] = NULL;
    }
  else if (!opt_daemon)
    {
      /* use the current working directory */
      filenames = g_new (gchar *, 2);
      filenames[0] = g_strdup (working_directory);
      filenames[1] = NULL;
    }

#ifdef HAVE_DBUS
  /* check if we can reuse an existing instance */
  if ((!opt_bulk_rename && filenames != NULL && thunar_dbus_client_launch_files (working_directory, filenames, NULL, startup_id, NULL))
      || (opt_bulk_rename && thunar_dbus_client_bulk_rename (working_directory, filenames, TRUE, NULL, startup_id, NULL)))
    {
      /* that worked, let's get outa here */
      g_free (working_directory);
      g_strfreev (filenames);
      return EXIT_SUCCESS;
    }
#endif

  /* initialize the thunar stock items/icons */
  thunar_stock_init ();

  /* acquire a reference on the global application */
  application = thunar_application_get ();

#ifdef HAVE_DBUS
  /* setup daemon mode if requested and supported */
  thunar_application_set_daemon (application, opt_daemon);
#endif

  /* use the Thunar icon as default for new windows */
  gtk_window_set_default_icon_name ("Thunar");

  /* check if we should open the bulk rename dialog */
  if (G_UNLIKELY (opt_bulk_rename))
    {
      /* try to open the bulk rename dialog */
      if (!thunar_application_bulk_rename (application, working_directory, filenames, TRUE, NULL, startup_id, &error))
        goto error0;
    }
  else if (filenames != NULL && !thunar_application_process_filenames (application, working_directory, filenames, NULL, startup_id, &error))
    {
      /* we failed to process the filenames or the bulk rename failed */
error0:
      g_printerr ("Thunar: %s\n", error->message);
      g_object_unref (G_OBJECT (application));
      g_error_free (error);
      return EXIT_FAILURE;
    }

  /* release working directory and filenames */
  g_free (working_directory);
  g_strfreev (filenames);

  /* connect to the session manager */
  session_client = thunar_session_client_new (opt_sm_client_id);

  /* check if the application should run as a daemon */
  if (thunar_application_get_daemon (application))
    {
#ifdef HAVE_DBUS
      /* attach the D-Bus service */
      dbus_service = g_object_new (THUNAR_TYPE_DBUS_SERVICE, NULL);

      /* check if the name was requested successfully */
      if (!thunar_dbus_service_has_connection (dbus_service))
        thunar_application_set_daemon (application, FALSE);
#endif
    }
  else
    {
      /* processing the command line arguments is done asynchronously. Thus, we
       * schedule an idle source which repeatedly checks whether we are done
       * processing. Once we're done, it'll make the application quit if there
       * are no open windows */
      g_idle_add_full (G_PRIORITY_LOW, thunar_delayed_exit_check,
                       g_object_ref (application), g_object_unref);
    }

  /* enter the main loop */
  gtk_main ();

#ifdef HAVE_DBUS
  if (dbus_service != NULL)
    g_object_unref (G_OBJECT (dbus_service));
#endif

  /* disconnect from the session manager */
  g_object_unref (G_OBJECT (session_client));

  /* release the application reference */
  g_object_unref (G_OBJECT (application));

#ifdef HAVE_LIBNOTIFY
  thunar_notify_uninit ();
#endif

  return EXIT_SUCCESS;
}

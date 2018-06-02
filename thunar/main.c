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
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-notify.h>
#include <thunar/thunar-session-client.h>
#include <thunar/thunar-preferences.h>



static void
thunar_terminate_running_thunar (void)
{
  GDBusConnection *connection;
  GDBusProxy      *proxy;
  GError          *error;

  error = NULL;
  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  proxy = g_dbus_proxy_new_sync (connection,
                                 G_DBUS_PROXY_FLAGS_NONE,
                                 NULL,
                                 "org.xfce.Thunar",       /* bus name */
                                 "/org/xfce/FileManager", /* object path */
                                 "org.xfce.Thunar",       /* interface name */
                                 NULL,
                                 &error);
  g_assert_no_error (error);
  g_dbus_proxy_call_sync (proxy,
                          "Terminate",                    /* method name */
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          &error);
  g_assert_no_error (error);
  g_object_unref (proxy);
  g_object_unref (connection);
}



static void
thunar_dialog_ask_terminate_old_daemon_activate (GtkApplication* app, gpointer user_data)
{
  GtkWidget *window;
  GtkWidget *dialog;
  gint       result;

  window = gtk_application_window_new (app);
  dialog = gtk_message_dialog_new (GTK_WINDOW (window), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
          _("Thunar could not be launched because an older instance of thunar is still running.\n"
            "Would you like to terminate the old thunar instance now?\n\n"
            "Before accepting please make sure there are no pending operations (e.g. file copying) as terminating them may leave your files corrupted.\n\n"
            "Please restart thunar afterwards."));
  result = gtk_dialog_run (GTK_DIALOG (dialog));
  if (result == GTK_RESPONSE_YES)
    thunar_terminate_running_thunar ();
  gtk_widget_destroy (window);
  g_application_quit (G_APPLICATION (app));
}



static void
thunar_dialog_ask_terminate_old_daemon (void)
{
  GtkApplication *app;

  app = gtk_application_new ("terminate.old.thunar.daemon", G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate", G_CALLBACK (thunar_dialog_ask_terminate_old_daemon_activate), NULL);
  g_application_run (G_APPLICATION (app), 0, 0);
  g_object_unref (app);
}



int
main (int argc, char **argv)
{
  ThunarApplication   *application;
  GError              *error = NULL;

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

  /* acquire a reference on the global application */
  application = thunar_application_get ();

  /* use the Thunar icon as default for new windows */
  gtk_window_set_default_icon_name ("Thunar");

  /* do further processing inside gapplication */
  g_application_run (G_APPLICATION (application), argc, argv);

  /* Workaround to bypass "silent fail" if new thunar version is installed while an old version still runs as daemon */
  /* FIXME: "g_application_register" and the following logic can be removed as soon as g_application/gdbus provides a way to prevent this error */
  g_application_register (G_APPLICATION (application), NULL, &error);
  if (error != NULL)
    {
      if (error->code == G_DBUS_ERROR_UNKNOWN_METHOD && strstr (error->message, "GDBus.Error:org.freedesktop.DBus.Error.UnknownMethod: Method \"DescribeAll\" with signature \"\" on interface \"org.gtk.Actions"))
          thunar_dialog_ask_terminate_old_daemon ();
      g_error_free (error);
    }

  /* release the application reference */
  g_object_unref (G_OBJECT (application));

#ifdef HAVE_LIBNOTIFY
  thunar_notify_uninit ();
#endif

  return EXIT_SUCCESS;
}

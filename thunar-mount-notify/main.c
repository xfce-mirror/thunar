/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2010      Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of 
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write to the Free 
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include <libnotify/notify.h>

#include <libxfce4util/libxfce4util.h>



/* make sure all defines are present */
#ifndef NOTIFY_EXPIRES_NEVER
#define NOTIFY_EXPIRES_NEVER 0
#endif
#ifndef NOTIFY_EXPIRES_DEFAULT
#define NOTIFY_EXPIRES_DEFAULT -1
#endif



/* --- globals --- */
static gboolean            opt_eject = FALSE;
static gchar              *opt_icon = NULL;
static gchar              *opt_name = NULL;
static gboolean            opt_readonly = FALSE;
static gboolean            opt_version = FALSE;
static gint                signal_fds[2];
static NotifyNotification *notification = NULL;



/* --- command line options --- */
static GOptionEntry entries[] =
{
  { "eject", 'e', 0, G_OPTION_ARG_NONE, &opt_eject, NULL, NULL, },
  { "icon", 'i', 0, G_OPTION_ARG_STRING, &opt_icon, NULL, NULL, },
  { "name", 'n', 0, G_OPTION_ARG_STRING, &opt_name, NULL, NULL, },
  { "readonly", 'r', 0, G_OPTION_ARG_NONE, &opt_readonly, NULL, NULL, },
  { "version", 'V', 0, G_OPTION_ARG_NONE, &opt_version, N_ ("Print version information and exit"), NULL, },
  { NULL, },
};



static void
signal_func (int signo)
{
  gint ignore;

  /* SIGUSR1 means success */
  ignore = write (signal_fds[1], (signo == SIGUSR1) ? "U" : "K", 1);
}



static gboolean
signal_io_func (GIOChannel  *channel,
                GIOCondition condition,
                gpointer     user_data)
{
  gchar *message;
  gchar  c;

  /* read the first character from signal pipe */
  if (read (signal_fds[0], &c, 1) < 1)
    return TRUE;

  /* perform the appropriate operation */
  if (c == 'U') /* SIGUSR1 */
    {
      /* the operation succeed */
      if (G_LIKELY (!opt_eject))
        {
          /* tell the user that the device can be removed now */
          message = g_strdup_printf (_("The device \"%s\" is now safe to remove."), opt_name);
          notify_notification_update (notification, _("Device is now safe to remove"), message, opt_icon);
          notify_notification_set_timeout (notification, NOTIFY_EXPIRES_DEFAULT);
          notify_notification_set_urgency (notification, NOTIFY_URGENCY_LOW);
          notify_notification_show (notification, NULL);
          g_free (message);

          /* release the notification, so it stays active even
           * after the process exits, otherwise we'll block
           * Thunar's unmount operations...
           */
          g_object_unref (G_OBJECT (notification));
        }
    }

  /* terminate the process */
  gtk_main_quit ();

  return TRUE;
}



int
main (int argc, char **argv)
{
  const gchar *summary;
  GIOChannel  *channel;
  GError      *err = NULL;
  gchar       *message;
  const gchar *display_name;

  /* initialize the i18n support */
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* initialize GTK+ */
  if (!gtk_init_with_args (&argc, &argv, "Xfce Mount Notify", entries, GETTEXT_PACKAGE, &err))
    {
      /* check if we have an error message */
      if (G_LIKELY (err == NULL))
        {
          /* no error message, the GUI initialization failed */
          display_name = gdk_get_display_arg_name ();
          g_fprintf (stderr, "thunar-mount-notify: %s: %s\n", _("Failed to open display"), (display_name != NULL) ? display_name : " ");
        }
      else
        {
          /* yep, there's an error, so print it */
          g_fprintf (stderr, "thunar-mount-notify: %s\n", err->message);
        }
      return EXIT_FAILURE;
    }

  /* check if we should print version */
  if (G_UNLIKELY (opt_version))
    {
      g_print ("%s %s\n\n", g_get_prgname (), PACKAGE_VERSION);
      g_print (_("Copyright (c) %s\n"
                 "        os-cillation e.K. All rights reserved.\n\n"
                 "Written by Benedikt Meurer <benny@xfce.org>.\n\n"),
               "2006-2007");
      g_print (_("%s comes with ABSOLUTELY NO WARRANTY,\n"
                 "You may redistribute copies of %s under the terms of\n"
                 "the GNU Lesser General Public License which can be found in the\n"
                 "%s source package.\n\n"), g_get_prgname (), g_get_prgname (), PACKAGE_TARNAME);
      g_print (_("Please report bugs to <%s>.\n"), PACKAGE_BUGREPORT);
      return EXIT_SUCCESS;
    }

  /* icon defaults to "gnome-dev-harddisk" */
  if (G_UNLIKELY (opt_icon == NULL || *opt_icon == '\0'))
    opt_icon = "gnome-dev-harddisk";

  /* make sure that a device name was specified */
  if (G_UNLIKELY (opt_name == NULL || *opt_icon == '\0'))
    {
      /* the caller must specify a usable device name */
      g_printerr ("%s: %s.\n", g_get_prgname (), "Must specify a device name");
      return EXIT_FAILURE;
    }

  /* try to initialize libnotify */
  if (!notify_init ("thunar-mount-notify"))
    {
      /* it doesn't make sense to continue from here on */
      g_printerr ("%s: %s.\n", g_get_prgname (), "Failed to initialize libnotify");
      return EXIT_FAILURE;
    }

  /* setup the signal pipe */
  if (pipe (signal_fds) < 0)
    {
      g_printerr ("%s: Failed to setup signal pipe: %s.\n", g_get_prgname (), g_strerror (errno));
      return EXIT_FAILURE;
    }

  /* register the appropriate signal handlers */
  signal (SIGTERM, signal_func);
  signal (SIGHUP, signal_func);
  signal (SIGINT, signal_func);
  signal (SIGUSR1, signal_func);

  /* watch the read side of the signal pipe */
  channel = g_io_channel_unix_new (signal_fds[0]);
  g_io_add_watch (channel, G_IO_IN, signal_io_func, NULL);
  g_io_channel_unref (channel);

  /* different summary/message based on whether it's readonly */
  if (G_UNLIKELY (opt_readonly))
    {
      /* check if we eject */
      if (G_LIKELY (opt_eject))
        {
          /* read-only, just ejecting */
          summary = _("Ejecting device");
          message = g_strdup_printf (_("The device \"%s\" is being ejected. This may take some time."), opt_name);
        }
      else
        {
          /* read-only, just unmounting */
          summary = _("Unmounting device");
          message = g_strdup_printf (_("The device \"%s\" is being unmounted by the system. Please do "
                                       "not remove the media or disconnect the drive."), opt_name);
        }
    }
  else
    {
      /* not read-only, writing back data */
      summary = _("Writing data to device");
      message = g_strdup_printf (_("There is data that needs to be written to the device \"%s\" before it can be "
                                   "removed. Please do not remove the media or disconnect the drive."), opt_name);
    }

  /* setup the notification */
  notification = notify_notification_new (summary, message, opt_icon, NULL);
  g_signal_connect (G_OBJECT (notification), "closed", G_CALLBACK (gtk_main_quit), NULL);
  g_object_add_weak_pointer (G_OBJECT (notification), (gpointer) &notification);
  notify_notification_set_urgency (notification, NOTIFY_URGENCY_CRITICAL);
  notify_notification_set_timeout (notification, NOTIFY_EXPIRES_NEVER);
  notify_notification_show (notification, NULL);
  g_free (message);

  /* enter the main loop */
  gtk_main ();

  /* release the notification */
  if (G_LIKELY (notification != NULL))
    {
      notify_notification_close (notification, NULL);
      g_object_unref (G_OBJECT (notification));
    }

  return EXIT_SUCCESS;
}

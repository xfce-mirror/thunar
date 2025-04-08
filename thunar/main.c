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
#include "config.h"
#endif

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <glib/gstdio.h>
#ifdef HAVE_GIO_UNIX
#include <gio/gdesktopappinfo.h>
#endif

#include "thunar/thunar-application.h"
#include "thunar/thunar-gobject-extensions.h"
#include "thunar/thunar-notify.h"
#include "thunar/thunar-preferences.h"
#include "thunar/thunar-private.h"
#include "thunar/thunar-session-client.h"

#include <xfconf/xfconf.h>



int
main (int argc, char **argv)
{
  ThunarApplication *application;
  GError            *error = NULL;
  gchar             *app_id = NULL;
  gchar             *window_class = NULL;
  GOptionContext    *context;
  GOptionEntry       entries[] = {
    { "wayland-app-id", 'a', 0, G_OPTION_ARG_STRING, &app_id, "Set Wayland application ID", NULL },
    { "x11-name", 'c', 0, G_OPTION_ARG_STRING, &window_class, "Set x11 Window class", NULL },
    { NULL }
  };

  /* parse command line options */
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_add_group (context, gtk_get_option_group (TRUE));
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_warning ("Error parsing options: %s", error->message);
      g_error_free (error);
      g_option_context_free (context);
      return EXIT_FAILURE;
    }
  g_option_context_free (context);
  /* setup translation domain */
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* setup application name */
  g_set_application_name (_("Thunar"));

  /* set custom program name if specified */
  if (app_id != NULL)
    g_set_prgname (app_id);

#ifdef G_ENABLE_DEBUG
  /* Do NOT remove this line for now, If something doesn't work,
   * fix your code instead!
   */
  g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
#endif

  /* initialize xfconf */
  if (!xfconf_init (&error))
    {
      g_warning (PACKAGE_NAME ": Failed to initialize Xfconf: %s", error->message);
      g_clear_error (&error);

      /* disable get/set properties */
      thunar_preferences_xfconf_init_failed ();
    }

  /* register additional transformation functions */
  thunar_g_initialize_transformations ();

  /* acquire a reference on the global application */
  application = thunar_application_get ();

  /* set window class if specified */
  if (window_class != NULL)
    gdk_set_program_class (window_class);

  /* use the Thunar icon as default for new windows */
  gtk_window_set_default_icon_name ("org.xfce.thunar");

  /* do further processing inside gapplication */
  g_application_run (G_APPLICATION (application), argc, argv);

  /* release the application reference */
  g_object_unref (G_OBJECT (application));

  /* free option strings */
  g_free (app_id);
  g_free (window_class);

#ifdef HAVE_LIBNOTIFY
  thunar_notify_uninit ();
#endif

  return EXIT_SUCCESS;
}

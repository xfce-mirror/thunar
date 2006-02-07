/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-dbus-service.h>
#include <thunar/thunar-file.h>
#include <thunar/thunar-gdk-extensions.h>
#include <thunar/thunar-preferences-dialog.h>
#include <thunar/thunar-properties-dialog.h>



static void     thunar_dbus_service_class_init                  (ThunarDBusServiceClass *klass);
static void     thunar_dbus_service_init                        (ThunarDBusService      *dbus_service);
static void     thunar_dbus_service_finalize                    (GObject                *object);
static gboolean thunar_dbus_service_parse_uri_and_display       (ThunarDBusService      *dbus_service,
                                                                 const gchar            *uri,
                                                                 const gchar            *display,
                                                                 ThunarFile            **file_return,
                                                                 GdkScreen             **screen_return,
                                                                 GError                **error);
static gboolean thunar_dbus_service_display_folder              (ThunarDBusService      *dbus_service,
                                                                 const gchar            *uri,
                                                                 const gchar            *display,
                                                                 GError                **error);
static gboolean thunar_dbus_service_display_file_properties     (ThunarDBusService      *dbus_service,
                                                                 const gchar            *uri,
                                                                 const gchar            *display,
                                                                 GError                **error);
static gboolean thunar_dbus_service_launch                      (ThunarDBusService      *dbus_service,
                                                                 const gchar            *uri,
                                                                 const gchar            *display,
                                                                 GError                **error);
static gboolean thunar_dbus_service_display_preferences_dialog  (ThunarDBusService      *dbus_service,
                                                                 const gchar            *display,
                                                                 GError                **error);



struct _ThunarDBusServiceClass
{
  GObjectClass __parent__;
};

struct _ThunarDBusService
{
  GObject __parent__;

  DBusGConnection *connection;
};



static GObjectClass *thunar_dbus_service_parent_class;



GType
thunar_dbus_service_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarDBusServiceClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_dbus_service_class_init,
        NULL,
        NULL,
        sizeof (ThunarDBusService),
        0,
        (GInstanceInitFunc) thunar_dbus_service_init,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, I_("ThunarDBusService"), &info, 0);
    }

  return type;
}



static void
thunar_dbus_service_class_init (ThunarDBusServiceClass *klass)
{
  extern const DBusGObjectInfo dbus_glib_thunar_dbus_service_object_info;
  GObjectClass                *gobject_class;

  /* determine the parent type class */
  thunar_dbus_service_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_dbus_service_finalize;

  /* install the D-BUS info for our class */
  dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (klass), &dbus_glib_thunar_dbus_service_object_info);
}



static void
thunar_dbus_service_init (ThunarDBusService *dbus_service)
{
  GError *error = NULL;

  /* try to connect to the session bus */
  dbus_service->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
  if (G_LIKELY (dbus_service->connection != NULL))
    {
      /* register the /org/xfce/FileManager object for Thunar */
      dbus_g_connection_register_g_object (dbus_service->connection, "/org/xfce/FileManager", G_OBJECT (dbus_service));

      /* request the org.xfce.FileManager name for Thunar */
      dbus_bus_request_name (dbus_g_connection_get_connection (dbus_service->connection), "org.xfce.FileManager", DBUS_NAME_FLAG_REPLACE_EXISTING, NULL);
    }
  else
    {
      /* notify the user that D-BUS service won't be available */
      g_warning ("Failed to connect to the D-BUS session bus: %s", error->message);
      g_error_free (error);
    }
}



static void
thunar_dbus_service_finalize (GObject *object)
{
  ThunarDBusService *dbus_service = THUNAR_DBUS_SERVICE (object);

  /* release the D-BUS connection object */
  if (G_LIKELY (dbus_service->connection != NULL))
    dbus_g_connection_unref (dbus_service->connection);

  (*G_OBJECT_CLASS (thunar_dbus_service_parent_class)->finalize) (object);
}



static gboolean
thunar_dbus_service_parse_uri_and_display (ThunarDBusService *dbus_service,
                                           const gchar       *uri,
                                           const gchar       *display,
                                           ThunarFile       **file_return,
                                           GdkScreen        **screen_return,
                                           GError           **error)
{
  ThunarVfsPath *path;

  /* try to open the display */
  *screen_return = thunar_gdk_screen_open (display, error);
  if (G_UNLIKELY (*screen_return == NULL))
    return FALSE;

  /* try to determine the path for the URI */
  path = thunar_vfs_path_new (uri, error);
  if (G_UNLIKELY (path == NULL))
    {
      g_object_unref (G_OBJECT (*screen_return));
      return FALSE;
    }

  /* try to determine the file for the path */
  *file_return = thunar_file_get_for_path (path, error);
  thunar_vfs_path_unref (path);

  /* check if we have a file */
  if (G_UNLIKELY (*file_return == NULL))
    {
      g_object_unref (G_OBJECT (*screen_return));
      return FALSE;
    }

  return TRUE;
}



static gboolean
thunar_dbus_service_display_folder (ThunarDBusService *dbus_service,
                                    const gchar       *uri,
                                    const gchar       *display,
                                    GError           **error)
{
  ThunarApplication *application;
  ThunarFile        *file;
  GdkScreen         *screen;

  /* parse uri and display parameters */
  if (!thunar_dbus_service_parse_uri_and_display (dbus_service, uri, display, &file, &screen, error))
    return FALSE;

  /* popup a new window for the folder */
  application = thunar_application_get ();
  thunar_application_open_window (application, file, screen);
  g_object_unref (G_OBJECT (application));

  /* cleanup */
  g_object_unref (G_OBJECT (screen));
  g_object_unref (G_OBJECT (file));

  return TRUE;
}



static gboolean
thunar_dbus_service_display_file_properties (ThunarDBusService *dbus_service,
                                             const gchar       *uri,
                                             const gchar       *display,
                                             GError           **error)
{
  ThunarApplication *application;
  ThunarFile        *file;
  GdkScreen         *screen;
  GtkWidget         *dialog;

  /* parse uri and display parameters */
  if (!thunar_dbus_service_parse_uri_and_display (dbus_service, uri, display, &file, &screen, error))
    return FALSE;

  /* popup the file properties dialog */
  dialog = thunar_properties_dialog_new ();
  gtk_window_set_screen (GTK_WINDOW (dialog), screen);
  thunar_properties_dialog_set_file (THUNAR_PROPERTIES_DIALOG (dialog), file);
  gtk_widget_show (dialog);

  /* let the application take control over the dialog */
  application = thunar_application_get ();
  thunar_application_take_window (application, GTK_WINDOW (dialog));
  g_object_unref (G_OBJECT (application));

  /* cleanup */
  g_object_unref (G_OBJECT (screen));
  g_object_unref (G_OBJECT (file));

  return TRUE;
}



static gboolean
thunar_dbus_service_launch (ThunarDBusService *dbus_service,
                            const gchar       *uri,
                            const gchar       *display,
                            GError           **error)
{
  ThunarFile *file;
  GdkScreen  *screen;
  gboolean    result = FALSE;

  /* parse uri and display parameters */
  if (thunar_dbus_service_parse_uri_and_display (dbus_service, uri, display, &file, &screen, error))
    {
      /* try to launch the file on the given screen */
      result = thunar_file_launch (file, screen, error);

      /* cleanup */
      g_object_unref (G_OBJECT (screen));
      g_object_unref (G_OBJECT (file));
    }

  return result;
}



static gboolean
thunar_dbus_service_display_preferences_dialog (ThunarDBusService *dbus_service,
                                                const gchar       *display,
                                                GError           **error)
{
  ThunarApplication *application;
  GdkScreen         *screen;
  GtkWidget         *dialog;

  /* try to open the screen for the display name */
  screen = thunar_gdk_screen_open (display, error);
  if (G_UNLIKELY (screen == NULL))
    return FALSE;

  /* popup the preferences dialog... */
  dialog = thunar_preferences_dialog_new (NULL);
  gtk_window_set_screen (GTK_WINDOW (dialog), screen);
  gtk_widget_show (dialog);

  /* ...and let the application take care of it */
  application = thunar_application_get ();
  thunar_application_take_window (application, GTK_WINDOW (dialog));
  g_object_unref (G_OBJECT (application));

  /* cleanup */
  g_object_unref (G_OBJECT (screen));

  return TRUE;
}



#include <thunar/thunar-dbus-service-infos.h>

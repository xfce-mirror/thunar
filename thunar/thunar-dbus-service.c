/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006      Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2010 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2010      Daniel Morales <daniel@daniel.com.uy>
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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>

#include <glib/gstdio.h>

#include <exo/exo.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-chooser-dialog.h>
#include <thunar/thunar-dbus-service.h>
#include <thunar/thunar-file.h>
#include <thunar/thunar-gdk-extensions.h>
#include <thunar/thunar-preferences-dialog.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-properties-dialog.h>
#include <thunar/thunar-util.h>


typedef enum
{
  THUNAR_DBUS_TRANSFER_MODE_COPY_TO,
  THUNAR_DBUS_TRANSFER_MODE_COPY_INTO,
  THUNAR_DBUS_TRANSFER_MODE_MOVE_INTO,
  THUNAR_DBUS_TRANSFER_MODE_LINK_INTO,
} ThunarDBusTransferMode;


static void     thunar_dbus_service_finalize                    (GObject                *object);
static gboolean thunar_dbus_service_connect_trash_bin           (ThunarDBusService      *dbus_service,
                                                                 GError                **error);
static gboolean thunar_dbus_service_parse_uri_and_display       (ThunarDBusService      *dbus_service,
                                                                 const gchar            *uri,
                                                                 const gchar            *display,
                                                                 ThunarFile            **file_return,
                                                                 GdkScreen             **screen_return,
                                                                 GError                **error);
static gboolean thunar_dbus_service_transfer_files              (ThunarDBusTransferMode  transfer_mode,
                                                                 const gchar            *working_directory,
                                                                 const gchar * const    *source_filenames,
                                                                 const gchar * const    *target_filenames,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static void     thunar_dbus_service_trash_bin_changed           (ThunarDBusService      *dbus_service,
                                                                 ThunarFile             *trash_bin);
static gboolean thunar_dbus_service_display_chooser_dialog      (ThunarDBusService      *dbus_service,
                                                                 const gchar            *uri,
                                                                 gboolean                open,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_display_folder              (ThunarDBusService      *dbus_service,
                                                                 const gchar            *uri,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_display_folder_and_select   (ThunarDBusService      *dbus_service,
                                                                 const gchar            *uri,
                                                                 const gchar            *filename,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_display_file_properties     (ThunarDBusService      *dbus_service,
                                                                 const gchar            *uri,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_launch                      (ThunarDBusService      *dbus_service,
                                                                 const gchar            *uri,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_execute                     (ThunarDBusService      *dbus_service,
                                                                 const gchar            *working_directory,
                                                                 const gchar            *uri,
                                                                 const gchar           **files,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_display_preferences_dialog  (ThunarDBusService      *dbus_service,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_display_trash               (ThunarDBusService      *dbus_service,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_empty_trash                 (ThunarDBusService      *dbus_service,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_move_to_trash               (ThunarDBusService      *dbus_service,
                                                                 gchar                 **filenames,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_query_trash                 (ThunarDBusService      *dbus_service,
                                                                 gboolean               *empty,
                                                                 GError                **error);
static gboolean thunar_dbus_service_bulk_rename                 (ThunarDBusService      *dbus_service,
                                                                 const gchar            *working_directory,
                                                                 gchar                 **filenames,
                                                                 gboolean                standalone,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_launch_files                (ThunarDBusService      *dbus_service,
                                                                 const gchar            *working_directory,
                                                                 gchar                 **filenames,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_rename_file                 (ThunarDBusService      *dbus_service,
                                                                 const gchar            *uri,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_create_file                 (ThunarDBusService      *dbus_service,
                                                                 const gchar            *parent_directory,
                                                                 const gchar            *content_type,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_create_file_from_template   (ThunarDBusService      *dbus_service,
                                                                 const gchar            *parent_directory,
                                                                 const gchar            *template_uri,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_copy_to                     (ThunarDBusService      *dbus_service,
                                                                 const gchar            *working_directory,
                                                                 gchar                 **source_filenames,
                                                                 gchar                 **target_filenames,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_copy_into                   (ThunarDBusService      *dbus_service,
                                                                 const gchar            *working_directory,
                                                                 gchar                 **source_filenames,
                                                                 const gchar            *target_filename,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_move_into                   (ThunarDBusService      *dbus_service,
                                                                 const gchar            *working_directory,
                                                                 gchar                 **source_filenames,
                                                                 const gchar            *target_filenames,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_link_into                   (ThunarDBusService      *dbus_service,
                                                                 const gchar            *working_directory,
                                                                 gchar                 **source_filenames,
                                                                 const gchar            *target_filename,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_unlink_files                (ThunarDBusService      *dbus_service,
                                                                 const gchar            *working_directory,
                                                                 gchar                 **filenames,
                                                                 const gchar            *display,
                                                                 const gchar            *startup_id,
                                                                 GError                **error);
static gboolean thunar_dbus_service_terminate                   (ThunarDBusService      *dbus_service,
                                                                 GError                **error);



/* include generate dbus infos */
#include <thunar/thunar-dbus-service-infos.h>



struct _ThunarDBusServiceClass
{
  GObjectClass __parent__;
};

struct _ThunarDBusService
{
  GObject __parent__;

  DBusGConnection *connection;
  ThunarFile      *trash_bin;
};



G_DEFINE_TYPE (ThunarDBusService, thunar_dbus_service, G_TYPE_OBJECT)



static void
thunar_dbus_service_class_init (ThunarDBusServiceClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_dbus_service_finalize;

  /* install the D-BUS info for our class */
  dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (klass), 
                                   &dbus_glib_thunar_dbus_service_object_info);

  /**
   * ThunarDBusService::trash-changed:
   * @dbus_service : a #ThunarDBusService.
   * @full         : %TRUE if the trash bin now contains at least
   *                 one item, %FALSE otherwise.
   *
   * This signal is emitted whenever the state of the trash bin
   * changes. Note that this signal is only emitted after the
   * trash has previously been queried by a D-BUS client.
   **/
  g_signal_new (I_("trash-changed"),
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL,
                g_cclosure_marshal_VOID__BOOLEAN,
                G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}



static void
thunar_dbus_service_init (ThunarDBusService *dbus_service)
{
  GError         *error = NULL;
  DBusConnection *dbus_connection;
  gint            result;

  /* try to connect to the session bus */
  dbus_service->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
  if (G_LIKELY (dbus_service->connection != NULL))
    {
      /* register the /org/xfce/FileManager object for Thunar */
      dbus_g_connection_register_g_object (dbus_service->connection, "/org/xfce/FileManager", G_OBJECT (dbus_service));

      /* request the org.xfce.Thunar name for Thunar */
      dbus_connection = dbus_g_connection_get_connection (dbus_service->connection);
      result = dbus_bus_request_name (dbus_connection, "org.xfce.Thunar",
                                      DBUS_NAME_FLAG_ALLOW_REPLACEMENT | DBUS_NAME_FLAG_DO_NOT_QUEUE, NULL);

      /* check if we successfully acquired the name */
      if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
        {
          g_printerr ("Thunar: D-BUS name org.xfce.Thunar already registered.\n");

          /* unset connection */
          dbus_g_connection_unref (dbus_service->connection);
          dbus_service->connection = NULL;

          return;
        }

      /* request the org.xfce.FileManager name for Thunar */
      dbus_bus_request_name (dbus_connection, "org.xfce.FileManager", DBUS_NAME_FLAG_REPLACE_EXISTING, NULL);

      /* once we registered, unset dbus variables (bug #8800) */
      g_unsetenv ("DBUS_STARTER_ADDRESS");
      g_unsetenv ("DBUS_STARTER_BUS_TYPE");
    }
  else
    {
      /* notify the user that D-BUS service won't be available */
      g_printerr ("Thunar: Failed to connect to the D-BUS session bus: %s\n", error->message);
      g_error_free (error);
    }
}



static void
thunar_dbus_service_finalize (GObject *object)
{
  ThunarDBusService *dbus_service = THUNAR_DBUS_SERVICE (object);
  DBusConnection    *dbus_connection;

  /* release the D-BUS connection object */
  if (G_LIKELY (dbus_service->connection != NULL))
    {
      /* release the names */
      dbus_connection = dbus_g_connection_get_connection (dbus_service->connection);
      dbus_bus_release_name (dbus_connection, "org.xfce.Thunar", NULL);
      dbus_bus_release_name (dbus_connection, "org.xfce.FileManager", NULL);

      dbus_g_connection_unref (dbus_service->connection);
    }

  /* check if we are connected to the trash bin */
  if (G_LIKELY (dbus_service->trash_bin != NULL))
    {
      /* unwatch the trash bin */
      thunar_file_unwatch (dbus_service->trash_bin);

      /* release the trash bin */
      g_signal_handlers_disconnect_by_func (G_OBJECT (dbus_service->trash_bin), thunar_dbus_service_trash_bin_changed, dbus_service);
      g_object_unref (G_OBJECT (dbus_service->trash_bin));
    }

  (*G_OBJECT_CLASS (thunar_dbus_service_parent_class)->finalize) (object);
}



static gboolean
thunar_dbus_service_connect_trash_bin (ThunarDBusService *dbus_service,
                                       GError           **error)
{
  GFile *trash_bin_path;

  /* check if we're not already connected to the trash bin */
  if (G_UNLIKELY (dbus_service->trash_bin == NULL))
    {
      /* try to connect to the trash bin */
      trash_bin_path = thunar_g_file_new_for_trash ();
      dbus_service->trash_bin = thunar_file_get (trash_bin_path, error);
      if (G_LIKELY (dbus_service->trash_bin != NULL))
        {
          /* watch the trash bin for changes */
          thunar_file_watch (dbus_service->trash_bin);

          /* stay informed about changes to the trash bin */
          g_signal_connect_swapped (G_OBJECT (dbus_service->trash_bin), "changed",
                                    G_CALLBACK (thunar_dbus_service_trash_bin_changed),
                                    dbus_service);
          thunar_file_reload_idle (dbus_service->trash_bin);
        }
      g_object_unref (trash_bin_path);
    }

  return (dbus_service->trash_bin != NULL);
}



static gboolean
thunar_dbus_service_parse_uri_and_display (ThunarDBusService *dbus_service,
                                           const gchar       *uri,
                                           const gchar       *display,
                                           ThunarFile       **file_return,
                                           GdkScreen        **screen_return,
                                           GError           **error)
{
  /* try to open the display */
  *screen_return = thunar_gdk_screen_open (display, error);
  if (G_UNLIKELY (*screen_return == NULL))
    return FALSE;

  /* try to determine the file for the URI */
  *file_return = thunar_file_get_for_uri (uri, error);
  if (G_UNLIKELY (*file_return == NULL))
    {
      g_object_unref (G_OBJECT (*screen_return));
      return FALSE;
    }

  return TRUE;
}



static void
thunar_dbus_service_trash_bin_changed (ThunarDBusService *dbus_service,
                                       ThunarFile        *trash_bin)
{
  _thunar_return_if_fail (THUNAR_IS_DBUS_SERVICE (dbus_service));
  _thunar_return_if_fail (dbus_service->trash_bin == trash_bin);
  _thunar_return_if_fail (THUNAR_IS_FILE (trash_bin));

  /* emit the "trash-changed" signal with the new state */
  g_signal_emit_by_name (G_OBJECT (dbus_service), "trash-changed", 
                         thunar_file_get_item_count (trash_bin) > 0);
}



static gboolean
thunar_dbus_service_display_chooser_dialog (ThunarDBusService *dbus_service,
                                            const gchar       *uri,
                                            gboolean           open,
                                            const gchar       *display,
                                            const gchar       *startup_id,
                                            GError           **error)
{
  ThunarFile *file;
  GdkScreen  *screen;

  /* parse uri and display parameters */
  if (!thunar_dbus_service_parse_uri_and_display (dbus_service, uri, display, &file, &screen, error))
    return FALSE;

  /* popup the chooser dialog */
  /* TODO use the startup id! */
  thunar_show_chooser_dialog (screen, file, open);

  /* cleanup */
  g_object_unref (G_OBJECT (screen));
  g_object_unref (G_OBJECT (file));

  return TRUE;
}



static gboolean
thunar_dbus_service_display_folder (ThunarDBusService *dbus_service,
                                    const gchar       *uri,
                                    const gchar       *display,
                                    const gchar       *startup_id,
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
  thunar_application_open_window (application, file, screen, startup_id);
  g_object_unref (G_OBJECT (application));

  /* cleanup */
  g_object_unref (G_OBJECT (screen));
  g_object_unref (G_OBJECT (file));

  return TRUE;
}



static gboolean
thunar_dbus_service_display_folder_and_select (ThunarDBusService *dbus_service,
                                               const gchar       *uri,
                                               const gchar       *filename,
                                               const gchar       *display,
                                               const gchar       *startup_id,
                                               GError           **error)
{
  ThunarApplication *application;
  ThunarFile        *file;
  ThunarFile        *folder;
  GdkScreen         *screen;
  GtkWidget         *window;
  GFile             *path;

  /* verify that filename is valid */
  if (G_UNLIKELY (filename == NULL || *filename == '\0' || strchr (filename, '/') != NULL))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Invalid filename \"%s\""), filename);
      return FALSE;
    }

  /* parse uri and display parameters */
  if (!thunar_dbus_service_parse_uri_and_display (dbus_service, uri, display, &folder, &screen, error))
    return FALSE;

  /* popup a new window for the folder */
  application = thunar_application_get ();
  window = thunar_application_open_window (application, folder, screen, startup_id);
  g_object_unref (application);

  /* determine the path for the filename relative to the folder */
  path = g_file_resolve_relative_path (thunar_file_get_file (folder), filename);
  if (G_LIKELY (path != NULL))
    {
      /* try to determine the file for the path */
      file = thunar_file_get (path, NULL);
      if (G_LIKELY (file != NULL))
        {
          /* tell the window to scroll to the given file and select it */
          thunar_window_scroll_to_file (THUNAR_WINDOW (window), file, TRUE, TRUE, 0.5f, 0.5f);

          /* release the file reference */
          g_object_unref (file);
        }

      /* release the path */
      g_object_unref (path);
    }

  /* cleanup */
  g_object_unref (screen);
  g_object_unref (folder);

  return TRUE;
}



static gboolean
thunar_dbus_service_display_file_properties (ThunarDBusService *dbus_service,
                                             const gchar       *uri,
                                             const gchar       *display,
                                             const gchar       *startup_id,
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
  dialog = thunar_properties_dialog_new (NULL);
  gtk_window_set_screen (GTK_WINDOW (dialog), screen);
  gtk_window_set_startup_id (GTK_WINDOW (dialog), startup_id);
  thunar_properties_dialog_set_file (THUNAR_PROPERTIES_DIALOG (dialog), file);
  gtk_window_present (GTK_WINDOW (dialog));

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
                            const gchar       *startup_id,
                            GError           **error)
{
  ThunarFile *file;
  GdkScreen  *screen;
  gboolean    result = FALSE;

  /* parse uri and display parameters */
  if (thunar_dbus_service_parse_uri_and_display (dbus_service, uri, display, &file, &screen, error))
    {
      /* try to launch the file on the given screen */
      result = thunar_file_launch (file, screen, startup_id, error);

      /* cleanup */
      g_object_unref (G_OBJECT (screen));
      g_object_unref (G_OBJECT (file));
    }

  return result;
}



static gboolean
thunar_dbus_service_execute (ThunarDBusService *dbus_service,
                             const gchar       *working_directory,
                             const gchar       *uri,
                             const gchar      **files,
                             const gchar       *display,
                             const gchar       *startup_id,
                             GError           **error)
{
  ThunarFile *file;
  GdkScreen  *screen;
  gboolean    result = FALSE;
  GFile      *working_dir;
  GList      *file_list = NULL;
  gchar      *tmp_working_dir = NULL;
  gchar      *old_working_dir = NULL;
  guint       n;

  /* parse uri and display parameters */
  if (thunar_dbus_service_parse_uri_and_display (dbus_service, uri, display, &file, &screen, error))
    {
      if (working_directory != NULL && *working_directory != '\0')
        old_working_dir = thunar_util_change_working_directory (working_directory);

      for (n = 0; files != NULL && files[n] != NULL; ++n)
        file_list = g_list_prepend (file_list, g_file_new_for_commandline_arg (files[n]));

      file_list = g_list_reverse (file_list);

      if (old_working_dir != NULL)
        {
          tmp_working_dir = thunar_util_change_working_directory (old_working_dir);
          g_free (tmp_working_dir);
          g_free (old_working_dir);
        }

      /* try to launch the file on the given screen */
      working_dir = g_file_new_for_commandline_arg (working_directory);
      result = thunar_file_execute (file, working_dir, screen, file_list, startup_id, error);
      g_object_unref (working_dir);

      /* cleanup */
      g_list_free_full (file_list, g_object_unref);
      g_object_unref (screen);
      g_object_unref (file);
    }

  return result;
}



static gboolean
thunar_dbus_service_display_preferences_dialog (ThunarDBusService *dbus_service,
                                                const gchar       *display,
                                                const gchar       *startup_id,
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
  gtk_window_set_startup_id (GTK_WINDOW (dialog), startup_id);
  gtk_widget_show (GTK_WIDGET (dialog));

  /* ...and let the application take care of it */
  application = thunar_application_get ();
  thunar_application_take_window (application, GTK_WINDOW (dialog));
  g_object_unref (G_OBJECT (application));

  /* cleanup */
  g_object_unref (G_OBJECT (screen));

  return TRUE;
}



static gboolean
thunar_dbus_service_display_trash (ThunarDBusService *dbus_service,
                                   const gchar       *display,
                                   const gchar       *startup_id,
                                   GError           **error)
{
  ThunarApplication *application;
  GdkScreen         *screen;

  /* connect to the trash bin on-demand */
  if (!thunar_dbus_service_connect_trash_bin (dbus_service, error))
    return FALSE;

  /* try to open the screen for the display name */
  screen = thunar_gdk_screen_open (display, error);
  if (G_LIKELY (screen != NULL))
    {
      /* tell the application to display the trash bin */
      application = thunar_application_get ();
      thunar_application_open_window (application, dbus_service->trash_bin, screen, startup_id);
      g_object_unref (G_OBJECT (application));

      /* release the screen */
      g_object_unref (G_OBJECT (screen));
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_dbus_service_empty_trash (ThunarDBusService *dbus_service,
                                 const gchar       *display,
                                 const gchar       *startup_id,
                                 GError           **error)
{
  ThunarApplication *application;
  GdkScreen         *screen;

  /* try to open the screen for the display name */
  screen = thunar_gdk_screen_open (display, error);
  if (G_LIKELY (screen != NULL))
    {
      /* tell the application to empty the trash bin */
      application = thunar_application_get ();
      thunar_application_empty_trash (application, screen, startup_id);
      g_object_unref (G_OBJECT (application));

      /* release the screen */
      g_object_unref (G_OBJECT (screen));
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_dbus_service_move_to_trash (ThunarDBusService *dbus_service,
                                   gchar            **filenames,
                                   const gchar       *display,
                                   const gchar       *startup_id,
                                   GError           **error)
{
  ThunarApplication *application;
  GFile             *file;
  GdkScreen         *screen;
  GError            *err = NULL;
  GList             *file_list = NULL;
  gchar             *filename;
  guint              n;

  /* try to open the screen for the display name */
  screen = thunar_gdk_screen_open (display, &err);
  if (G_LIKELY (screen != NULL))
    {
      /* try to parse the specified filenames */
      for (n = 0; err == NULL && filenames[n] != NULL; ++n)
        {
          /* decode the filename (D-BUS uses UTF-8) */
          filename = g_filename_from_utf8 (filenames[n], -1, NULL, NULL, &err);
          if (G_LIKELY (err == NULL))
            {
              /* determine the path for the filename */
              /* TODO Not sure this will work as expected */
              file = g_file_new_for_commandline_arg (filename);
              file_list = thunar_g_file_list_append (file_list, file);
              g_object_unref (file);
            }

          /* cleanup */
          g_free (filename);
        }

      /* check if we succeed */
      if (G_LIKELY (err == NULL))
        {
          /* tell the application to move the specified files to the trash */
          application = thunar_application_get ();
          thunar_application_trash (application, screen, file_list);
          g_object_unref (application);
        }

      /* cleanup */
      thunar_g_file_list_free (file_list);
      g_object_unref (screen);
    }

  /* check if we failed */
  if (G_UNLIKELY (err != NULL))
    {
      /* propagate the error */
      g_propagate_error (error, err);
      return FALSE;
    }

  return TRUE;
}



static gboolean
thunar_dbus_service_query_trash (ThunarDBusService *dbus_service,
                                 gboolean          *full,
                                 GError           **error)
{
  /* connect to the trash bin on-demand */
  if (thunar_dbus_service_connect_trash_bin (dbus_service, error))
    {
      /* check whether the trash bin is not empty */
      *full = (thunar_file_get_item_count (dbus_service->trash_bin) > 0);
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_dbus_service_bulk_rename (ThunarDBusService *dbus_service,
                                 const gchar       *working_directory,
                                 gchar            **filenames,
                                 gboolean           standalone,
                                 const gchar       *display,
                                 const gchar       *startup_id,
                                 GError           **error)
{
  ThunarApplication *application;
  GdkScreen         *screen;
  gboolean           result = FALSE;
  gchar             *cwd;

  /* determine a proper working directory */
  cwd = (working_directory != NULL && *working_directory != '\0')
      ? g_strdup (working_directory)
      : g_get_current_dir ();

  /* try to open the screen for the display name */
  screen = thunar_gdk_screen_open (display, error);
  if (G_LIKELY (screen != NULL))
    {
      /* tell the application to display the bulk rename dialog */
      application = thunar_application_get ();
      result = thunar_application_bulk_rename (application, cwd, filenames, standalone, screen, startup_id, error);
      g_object_unref (G_OBJECT (application));

      /* release the screen */
      g_object_unref (G_OBJECT (screen));
    }

  /* release the cwd */
  g_free (cwd);

  return result;
}



static gboolean
thunar_dbus_service_launch_files (ThunarDBusService *dbus_service,
                                  const gchar       *working_directory,
                                  gchar            **filenames,
                                  const gchar       *display,
                                  const gchar       *startup_id,
                                  GError           **error)
{
  ThunarApplication *application;
  GdkScreen         *screen;
  gboolean           result = FALSE;

  /* verify that a valid working directory is given */
  if (G_UNLIKELY (!g_path_is_absolute (working_directory)))
    {
      /* LaunchFiles() invoked without a valid working directory */
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("The working directory must be an absolute path"));
      return FALSE;
    }

  /* verify that at least one filename is given */
  if (G_UNLIKELY (filenames == NULL || *filenames == NULL))
    {
      /* LaunchFiles() invoked with an empty filename list */
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("At least one filename must be specified"));
      return FALSE;
    }

  /* try to open the screen for the display name */
  screen = thunar_gdk_screen_open (display, error);
  if (G_LIKELY (screen != NULL))
    {
      /* let the application process the filenames */
      application = thunar_application_get ();
      result = thunar_application_process_filenames (application, working_directory, filenames, screen, startup_id, error);
      g_object_unref (G_OBJECT (application));

      /* release the screen */
      g_object_unref (G_OBJECT (screen));
    }

  return result;
}



static gboolean
thunar_dbus_service_rename_file (ThunarDBusService *dbus_service,
                                 const gchar       *uri,
                                 const gchar       *display,
                                 const gchar       *startup_id,
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
  thunar_application_rename_file (application, file, screen, startup_id);
  g_object_unref (G_OBJECT (application));

  /* cleanup */
  g_object_unref (G_OBJECT (screen));
  g_object_unref (G_OBJECT (file));

  return TRUE;
}



static gboolean
thunar_dbus_service_create_file (ThunarDBusService *dbus_service,
                                 const gchar       *parent_directory,
                                 const gchar       *content_type,
                                 const gchar       *display,
                                 const gchar       *startup_id,
                                 GError           **error)
{
  ThunarApplication *application;
  ThunarFile        *file;
  GdkScreen         *screen;

  /* parse uri and display parameters */
  if (!thunar_dbus_service_parse_uri_and_display (dbus_service, parent_directory, display, &file, &screen, error))
    return FALSE;

  /* fall back to plain text file if no content type is provided */
  if (content_type == NULL || *content_type == '\0')
    content_type = "text/plain";

  /* popup a new window for the folder */
  application = thunar_application_get ();
  thunar_application_create_file (application, file, content_type, screen, startup_id);
  g_object_unref (G_OBJECT (application));

  /* cleanup */
  g_object_unref (G_OBJECT (screen));
  g_object_unref (G_OBJECT (file));

  return TRUE;
}



static gboolean
thunar_dbus_service_create_file_from_template (ThunarDBusService *dbus_service,
                                               const gchar       *parent_directory,
                                               const gchar       *template_uri,
                                               const gchar       *display,
                                               const gchar       *startup_id,
                                               GError           **error)
{
  ThunarApplication *application;
  ThunarFile        *file;
  ThunarFile        *template_file;
  GdkScreen         *screen;

  /* parse uri and display parameters */
  if (!thunar_dbus_service_parse_uri_and_display (dbus_service, parent_directory, display, &file, &screen, error))
    return FALSE;

  /* try to determine the file for the template URI */
  template_file = thunar_file_get_for_uri (template_uri, error);
  if(template_file == NULL)
    return FALSE;

  /* popup a new window for the folder */
  application = thunar_application_get ();
  thunar_application_create_file_from_template (application, file, template_file, screen, startup_id);
  g_object_unref (G_OBJECT (application));

  /* cleanup */
  g_object_unref (G_OBJECT (screen));
  g_object_unref (G_OBJECT (file));

  return TRUE;
}



static gboolean
thunar_dbus_service_transfer_files (ThunarDBusTransferMode transfer_mode,
                                    const gchar           *working_directory,
                                    const gchar * const   *source_filenames,
                                    const gchar * const   *target_filenames,
                                    const gchar           *display,
                                    const gchar           *startup_id,
                                    GError               **error)
{
  ThunarApplication *application;
  GdkScreen         *screen;
  GError            *err = NULL;
  GFile             *file;
  GList             *source_file_list = NULL;
  GList             *target_file_list = NULL;
  gchar             *filename;
  gchar             *new_working_dir = NULL;
  gchar             *old_working_dir = NULL;
  guint              n;

  /* verify that at least one file to transfer is given */
  if (source_filenames == NULL || *source_filenames == NULL)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, 
                   _("At least one source filename must be specified"));
      return FALSE;
    }

  /* verify that the target filename is set / enough target filenames are given */
  if (transfer_mode == THUNAR_DBUS_TRANSFER_MODE_COPY_TO) 
    {
      if (g_strv_length ((gchar **)source_filenames) != g_strv_length ((gchar **)target_filenames))
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                       _("The number of source and target filenames must be the same"));
          return FALSE;
        }
    }
  else
    {
      if (target_filenames == NULL || *target_filenames == NULL)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                       _("A destination directory must be specified"));
          return FALSE;
        }
    }

  /* try to open the screen for the display name */
  screen = thunar_gdk_screen_open (display, &err);
  if (screen != NULL)
    {
      /* change the working directory if necessary */
      if (!exo_str_is_empty (working_directory))
        old_working_dir = thunar_util_change_working_directory (working_directory);

      /* transform the source filenames into GFile objects */
      for (n = 0; err == NULL && source_filenames[n] != NULL; ++n)
        {
          filename = g_filename_from_utf8 (source_filenames[n], -1, NULL, NULL, &err);
          if (filename != NULL)
            {
              file = g_file_new_for_commandline_arg (filename);
              source_file_list = thunar_g_file_list_append (source_file_list, file);
              g_object_unref (file);
              g_free (filename);
            }
        }

      /* transform the target filename(s) into (a) GFile object(s) */
      for (n = 0; err == NULL && target_filenames[n] != NULL; ++n)
        {
          filename = g_filename_from_utf8 (target_filenames[n], -1, NULL, NULL, &err);
          if (filename != NULL)
            {
              file = g_file_new_for_commandline_arg (filename);
              target_file_list = thunar_g_file_list_append (target_file_list, file);
              g_object_unref (file);
              g_free (filename);
            }
        }

      /* switch back to the previous working directory */
      if (!exo_str_is_empty (working_directory))
        {
          new_working_dir = thunar_util_change_working_directory (old_working_dir);
          g_free (old_working_dir);
          g_free (new_working_dir);
        }

      if (err == NULL)
        {
          /* let the application process the filenames */
          application = thunar_application_get ();
          switch (transfer_mode)
            {
            case THUNAR_DBUS_TRANSFER_MODE_COPY_TO:
              thunar_application_copy_to (application, screen, 
                                          source_file_list, target_file_list, 
                                          NULL);
              break;
            case THUNAR_DBUS_TRANSFER_MODE_COPY_INTO:
              thunar_application_copy_into (application, screen, 
                                            source_file_list, target_file_list->data, 
                                            NULL);
              break;
            case THUNAR_DBUS_TRANSFER_MODE_MOVE_INTO:
              thunar_application_move_into (application, screen, 
                                            source_file_list, target_file_list->data, 
                                            NULL);
              break;
            case THUNAR_DBUS_TRANSFER_MODE_LINK_INTO:
              thunar_application_link_into (application, screen, 
                                            source_file_list, target_file_list->data, 
                                            NULL);
              break;
            }
          g_object_unref (application);
        }

      /* free the file lists */
      thunar_g_file_list_free (source_file_list);
      thunar_g_file_list_free (target_file_list);

      /* release the screen */
      g_object_unref (screen);
    }

  if (err != NULL)
    {
      g_propagate_error (error, err);
      return FALSE;
    }

  return TRUE;
}



static gboolean
thunar_dbus_service_copy_to (ThunarDBusService *dbus_service,
                             const gchar       *working_directory,
                             gchar            **source_filenames,
                             gchar            **target_filenames,
                             const gchar       *display,
                             const gchar       *startup_id,
                             GError           **error)
{
  return thunar_dbus_service_transfer_files (THUNAR_DBUS_TRANSFER_MODE_COPY_TO,
                                             working_directory,
                                             (const gchar * const *)source_filenames,
                                             (const gchar * const *)target_filenames,
                                             display,
                                             startup_id,
                                             error);
}



static gboolean
thunar_dbus_service_copy_into (ThunarDBusService *dbus_service,
                               const gchar       *working_directory,
                               gchar            **source_filenames,
                               const gchar       *target_filename,
                               const gchar       *display,
                               const gchar       *startup_id,
                               GError           **error)
{
  const gchar *target_filenames[2] = { target_filename, NULL };

  return thunar_dbus_service_transfer_files (THUNAR_DBUS_TRANSFER_MODE_COPY_INTO,
                                             working_directory,
                                             (const gchar * const *)source_filenames,
                                             target_filenames,
                                             display,
                                             startup_id,
                                             error);
}



static gboolean
thunar_dbus_service_move_into (ThunarDBusService *dbus_service,
                               const gchar       *working_directory,
                               gchar            **source_filenames,
                               const gchar       *target_filename,
                               const gchar       *display,
                               const gchar       *startup_id,
                               GError           **error)
{
  const gchar *target_filenames[2] = { target_filename, NULL };

  return thunar_dbus_service_transfer_files (THUNAR_DBUS_TRANSFER_MODE_MOVE_INTO,
                                             working_directory,
                                             (const gchar * const *)source_filenames,
                                             target_filenames,
                                             display,
                                             startup_id,
                                             error);
}



static gboolean
thunar_dbus_service_link_into (ThunarDBusService *dbus_service,
                               const gchar       *working_directory,
                               gchar            **source_filenames,
                               const gchar       *target_filename,
                               const gchar       *display,
                               const gchar       *startup_id,
                               GError           **error)
{
  const gchar *target_filenames[2] = { target_filename, NULL };

  return thunar_dbus_service_transfer_files (THUNAR_DBUS_TRANSFER_MODE_LINK_INTO,
                                             working_directory,
                                             (const gchar * const *)source_filenames,
                                             target_filenames,
                                             display,
                                             startup_id,
                                             error);
}


static gboolean
thunar_dbus_service_unlink_files (ThunarDBusService  *dbus_service,
                                  const gchar        *working_directory,
                                  gchar             **filenames,
                                  const gchar        *display,
                                  const gchar        *startup_id,
                                  GError            **error)
{
  ThunarApplication *application;
  ThunarFile        *thunar_file;
  GFile             *file;
  GdkScreen         *screen;
  GError            *err = NULL;
  GList             *file_list = NULL;
  gchar             *filename;
  gchar             *new_working_dir = NULL;
  gchar             *old_working_dir = NULL;
  guint              n;

  /* verify that atleast one filename is given */
  if (filenames == NULL || *filenames == NULL)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("At least one filename must be specified"));
      return FALSE;
    }

  /* try to open the screen for the display name */
  screen = thunar_gdk_screen_open (display, &err);
  if (screen != NULL)
    {
      /* change the working directory if necessary */
      if (!exo_str_is_empty (working_directory))
        old_working_dir = thunar_util_change_working_directory (working_directory);

      /* try to parse the specified filenames */
      for (n = 0; err == NULL && filenames[n] != NULL; ++n)
        {
          /* decode the filename (D-BUS uses UTF-8) */
          filename = g_filename_from_utf8 (filenames[n], -1, NULL, NULL, &err);
          if (filename != NULL)
            {
              /* determine the path for the filename */
              file = g_file_new_for_commandline_arg (filename);
              thunar_file = thunar_file_get (file, &err);

              if (thunar_file != NULL)
                file_list = g_list_append (file_list, thunar_file);

              g_object_unref (file);
            }

          /* cleanup */
          g_free (filename);
        }

      /* switch back to the previous working directory */
      if (!exo_str_is_empty (working_directory))
        {
          new_working_dir = thunar_util_change_working_directory (old_working_dir);
          g_free (old_working_dir);
          g_free (new_working_dir);
        }

      /* check if we succeeded */
      if (err == NULL && file_list != NULL)
        {
          /* tell the application to move the specified files to the trash */
          application = thunar_application_get ();
          thunar_application_unlink_files (application, screen, file_list, TRUE);
          g_object_unref (application);
        }

      /* cleanup */
      thunar_g_file_list_free (file_list);
      g_object_unref (screen);
    }

  if (err != NULL)
    {
      g_propagate_error (error, err);
      return FALSE;
    }

  return TRUE;
}



static gboolean
thunar_dbus_service_terminate (ThunarDBusService *dbus_service,
                               GError           **error)
{
  /* leave the Gtk main loop as soon as possible */
  gtk_main_quit ();

  /* we cannot fail */
  return TRUE;
}



gboolean
thunar_dbus_service_has_connection (ThunarDBusService *dbus_service)
{
  _thunar_return_val_if_fail (THUNAR_IS_DBUS_SERVICE (dbus_service), FALSE);
  return dbus_service->connection != NULL;
}

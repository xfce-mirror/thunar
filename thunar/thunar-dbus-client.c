/* vi:set et ai sw=2 sts=2 ts=2: */
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

#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus.h>

#include <thunar/thunar-dbus-client.h>
#include <thunar/thunar-private.h>



/**
 * thunar_dbus_client_bulk_rename:
 * @working_directory : the default working directory for the bulk rename dialog.
 * @filenames         : the list of files that should be displayed by default or
 *                      the empty list to start with an empty bulk rename dialog.
 * @standalone        : whether to run the bulk renamer in standalone mode.
 * @screen            : the #GdkScreen on which to display the dialog or %NULL to
 *                      use the default #GdkScreen.
 * @startup_id        : startup id to properly finish startup notification and set
 *                      the window timestamp or %NULL.
 * @error             : return location for errors or %NULL.
 *
 * Tries to invoke the BulkRename() method on a running Thunar instance, that is
 * registered with the current D-BUS session bus. Returns %TRUE if the method was
 * successfully invoked, else %FALSE.
 *
 * If %TRUE is returned, the current process may afterwards just terminate, as
 * all @filenames will be handled by the remote instance.
 *
 * Return value: %TRUE on success, else %FALSE.
 **/
gboolean
thunar_dbus_client_bulk_rename (const gchar *working_directory,
                                gchar      **filenames,
                                gboolean     standalone,
                                GdkScreen   *screen,
                                const gchar *startup_id,
                                GError     **error)
{
  DBusConnection *connection;
  DBusMessage    *message;
  DBusMessage    *result;
  DBusError       derror;
  gchar          *display_name;

  _thunar_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), FALSE);
  _thunar_return_val_if_fail (g_path_is_absolute (working_directory), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_return_val_if_fail (filenames != NULL, FALSE);

  /* initialize the DBusError struct */
  dbus_error_init (&derror);

  /* fallback to default screen if no other is specified */
  if (G_LIKELY (screen == NULL))
    screen = gdk_screen_get_default ();

  /* try to connect to the session bus */
  connection = dbus_bus_get (DBUS_BUS_SESSION, &derror);
  if (G_UNLIKELY (connection == NULL))
    {
      dbus_set_g_error (error, &derror);
      dbus_error_free (&derror);
      return FALSE;
    }

  /* determine the display name for the screen */
  display_name = gdk_screen_make_display_name (screen);

  /* dbus does not like null values */
  if (startup_id == NULL)
    startup_id = "";

  /* generate the BulkRename() method (disable activation!) */
  message = dbus_message_new_method_call ("org.xfce.Thunar", "/org/xfce/FileManager", "org.xfce.Thunar", "BulkRename");
  dbus_message_set_auto_start (message, FALSE);
  dbus_message_append_args (message,
                            DBUS_TYPE_STRING, &working_directory,
                            DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &filenames, g_strv_length (filenames),
                            DBUS_TYPE_BOOLEAN, &standalone,
                            DBUS_TYPE_STRING, &display_name,
                            DBUS_TYPE_STRING, &startup_id,
                            DBUS_TYPE_INVALID);

  /* release the display name */
  g_free (display_name);

  /* send the message and release our references on connection and message */
  dbus_error_init (&derror);
  result = dbus_connection_send_with_reply_and_block (connection, message, -1, &derror);
  dbus_message_unref (message);

  /* check if no reply was received */
  if (G_UNLIKELY (result == NULL))
    {
      dbus_set_g_error (error, &derror);
      dbus_error_free (&derror);
      return FALSE;
    }

  /* but maybe we received an error */
  if (dbus_message_get_type (result) == DBUS_MESSAGE_TYPE_ERROR)
    {
      dbus_set_error_from_message (&derror, result);
      dbus_set_g_error (error, &derror);
      dbus_message_unref (result);
      dbus_error_free (&derror);
      return FALSE;
    }

  /* let's asume that it worked */
  dbus_message_unref (result);
  return TRUE;
}



/**
 * thunar_dbus_client_launch_files:
 * @working_directory : the directory relative to which @filenames may be looked up.
 * @filenames         : the list of @filenames to launch.
 * @screen            : the #GdkScreen on which to launch the @filenames or %NULL
 *                      to use the default #GdkScreen.
 * @startup_id        : startup id to properly finish startup notification and set
 *                      the window timestamp or %NULL.
 * @error             : return location for errors or %NULL.
 *
 * Tries to invoke the LaunchFiles() method on a running Thunar instance, that is
 * registered with the current D-BUS session bus. Returns %TRUE if the method was
 * successfully invoked, else %FALSE.
 *
 * If %TRUE is returned, the current process may afterwards just terminate, as
 * all @filenames will be handled by the remote instance. Else, if %FALSE is
 * returned the current process should try to launch the @filenames itself.
 *
 * Return value: %TRUE on success, else %FALSE.
 **/
gboolean
thunar_dbus_client_launch_files (const gchar *working_directory,
                                 gchar      **filenames,
                                 GdkScreen   *screen,
                                 const gchar *startup_id,
                                 GError     **error)
{
  DBusConnection *connection;
  DBusMessage    *message;
  DBusMessage    *result;
  DBusError       derror;
  gchar          *display_name;

  _thunar_return_val_if_fail (g_path_is_absolute (working_directory), FALSE);
  _thunar_return_val_if_fail (filenames != NULL && *filenames != NULL, FALSE);
  _thunar_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* initialize the DBusError struct */
  dbus_error_init (&derror);

  /* fallback to default screen if no other is specified */
  if (G_LIKELY (screen == NULL))
    screen = gdk_screen_get_default ();

  /* try to connect to the session bus */
  connection = dbus_bus_get (DBUS_BUS_SESSION, &derror);
  if (G_UNLIKELY (connection == NULL))
    {
      dbus_set_g_error (error, &derror);
      dbus_error_free (&derror);
      return FALSE;
    }

  /* determine the display name for the screen */
  display_name = gdk_screen_make_display_name (screen);

  /* dbus does not like null values */
  if (startup_id == NULL)
    startup_id = "";

  /* generate the LaunchFiles() method (disable activation!) */
  message = dbus_message_new_method_call ("org.xfce.Thunar", "/org/xfce/FileManager", "org.xfce.FileManager", "LaunchFiles");
  dbus_message_set_auto_start (message, FALSE);
  dbus_message_append_args (message,
                            DBUS_TYPE_STRING, &working_directory,
                            DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &filenames, g_strv_length (filenames),
                            DBUS_TYPE_STRING, &display_name,
                            DBUS_TYPE_STRING, &startup_id,
                            DBUS_TYPE_INVALID);

  /* release the display name */
  g_free (display_name);

  /* send the message and release our references on connection and message */
  dbus_error_init (&derror);
  result = dbus_connection_send_with_reply_and_block (connection, message, -1, &derror);
  dbus_message_unref (message);

  /* check if no reply was received */
  if (G_UNLIKELY (result == NULL))
    {
      dbus_set_g_error (error, &derror);
      dbus_error_free (&derror);
      return FALSE;
    }

  /* but maybe we received an error */
  if (dbus_message_get_type (result) == DBUS_MESSAGE_TYPE_ERROR)
    {
      dbus_set_error_from_message (&derror, result);
      dbus_set_g_error (error, &derror);
      dbus_message_unref (result);
      dbus_error_free (&derror);
      return FALSE;
    }

  /* let's asume that it worked */
  dbus_message_unref (result);
  return TRUE;
}



/**
 * thunar_dbus_client_terminate:
 * @error : Return location for errors or %NULL.
 *
 * Tells a running Thunar instance, connected to the D-BUS
 * session bus, to terminate immediately.
 *
 * Return value: %TRUE if any instance was terminated, else
 *               %FALSE and @error is set.
 **/
gboolean
thunar_dbus_client_terminate (GError **error)
{
  DBusConnection *connection;
  DBusMessage    *message;
  DBusMessage    *result;
  DBusError       derror;

  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* initialize the DBusError struct */
  dbus_error_init (&derror);

  /* try to connect to the session bus */
  connection = dbus_bus_get (DBUS_BUS_SESSION, &derror);
  if (G_UNLIKELY (connection == NULL))
    {
      dbus_set_g_error (error, &derror);
      dbus_error_free (&derror);
      return FALSE;
    }

  /* generate the LaunchFiles() method (disable activation!) */
  message = dbus_message_new_method_call ("org.xfce.Thunar", "/org/xfce/FileManager", "org.xfce.Thunar", "Terminate");
  dbus_message_set_auto_start (message, FALSE);

  /* send the message and release our references on connection and message */
  dbus_error_init (&derror);
  result = dbus_connection_send_with_reply_and_block (connection, message, -1, &derror);
  dbus_message_unref (message);

  /* check if no reply was received */
  if (G_UNLIKELY (result == NULL))
    {
      /* check if there was nothing to terminate */
      if (dbus_error_has_name (&derror, DBUS_ERROR_NAME_HAS_NO_OWNER))
        {
          dbus_error_free (&derror);
          return TRUE;
        }

      /* Looks like there was a real error */
      dbus_set_g_error (error, &derror);
      dbus_error_free (&derror);
      return FALSE;
    }

  /* but maybe we received an error */
  if (dbus_message_get_type (result) == DBUS_MESSAGE_TYPE_ERROR)
    {
      dbus_set_error_from_message (&derror, result);
      dbus_set_g_error (error, &derror);
      dbus_message_unref (result);
      dbus_error_free (&derror);
      return FALSE;
    }

  /* let's asume that it worked */
  dbus_message_unref (result);
  return TRUE;
}

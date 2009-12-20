/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#include <glib-object.h>

#include <thunar/thunar-browser.h>
#include <thunar/thunar-file.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-util.h>



typedef struct _PokeFileData   PokeFileData;
typedef struct _PokeVolumeData PokeVolumeData;

struct _PokeFileData
{
  ThunarBrowser            *browser;
  ThunarFile               *source;
  ThunarFile               *file;
  ThunarBrowserPokeFileFunc func;
  gpointer                  user_data;
};

struct _PokeVolumeData
{
  ThunarBrowser              *browser;
  GVolume                    *volume;
  ThunarFile                 *mount_point;
  ThunarBrowserPokeVolumeFunc func;
  gpointer                    user_data;
};



GType
thunar_browser_get_type (void)
{
  static volatile gsize type__volatile = 0;
  GType                 type;

  if (g_once_init_enter (&type__volatile))
    {
      type = g_type_register_static_simple (G_TYPE_INTERFACE,
                                            I_("ThunarBrowser"),
                                            sizeof (ThunarBrowserIface),
                                            NULL,
                                            0,
                                            NULL,
                                            0);

      g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);

      g_once_init_leave (&type__volatile, type);
    }

  return type__volatile;
}



static PokeFileData *
thunar_browser_poke_file_data_new (ThunarBrowser            *browser,
                                   ThunarFile               *source,
                                   ThunarFile               *file,
                                   ThunarBrowserPokeFileFunc func,
                                   gpointer                  user_data)
{
  PokeFileData *poke_data;

  _thunar_return_val_if_fail (THUNAR_IS_BROWSER (browser), NULL);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (source), NULL);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  poke_data = g_slice_new0 (PokeFileData);
  poke_data->browser = g_object_ref (browser);
  poke_data->source = g_object_ref (source);
  poke_data->file = g_object_ref (file);
  poke_data->func = func;
  poke_data->user_data = user_data;

  return poke_data;
}



static void
thunar_browser_poke_file_data_free (PokeFileData *poke_data)
{
  _thunar_return_if_fail (poke_data != NULL);
  _thunar_return_if_fail (THUNAR_IS_BROWSER (poke_data->browser));
  _thunar_return_if_fail (THUNAR_IS_FILE (poke_data->source));
  _thunar_return_if_fail (THUNAR_IS_FILE (poke_data->file));

  g_object_unref (poke_data->browser);
  g_object_unref (poke_data->source);
  g_object_unref (poke_data->file);

  g_slice_free (PokeFileData, poke_data);
}



static PokeVolumeData *
thunar_browser_poke_volume_data_new (ThunarBrowser              *browser,
                                     GVolume                    *volume,
                                     ThunarBrowserPokeVolumeFunc func,
                                     gpointer                    user_data)
{
  PokeVolumeData *poke_data;

  _thunar_return_val_if_fail (THUNAR_IS_BROWSER (browser), NULL);
  _thunar_return_val_if_fail (G_IS_VOLUME (volume), NULL);

  poke_data = g_slice_new0 (PokeVolumeData);
  poke_data->browser = g_object_ref (browser);
  poke_data->volume = g_object_ref (volume);
  poke_data->func = func;
  poke_data->user_data = user_data;

  return poke_data;
}



static void
thunar_browser_poke_volume_data_free (PokeVolumeData *poke_data)
{
  _thunar_return_if_fail (poke_data != NULL);
  _thunar_return_if_fail (THUNAR_IS_BROWSER (poke_data->browser));
  _thunar_return_if_fail (G_IS_VOLUME (poke_data->volume));

  g_object_unref (poke_data->browser);
  g_object_unref (poke_data->volume);

  g_slice_free (PokeVolumeData, poke_data);
}



static GMountOperation *
thunar_browser_mount_operation_new (gpointer parent)
{
  GMountOperation *mount_operation;
  GtkWindow       *window = NULL;
  GdkScreen       *screen = NULL;

  mount_operation = gtk_mount_operation_new (NULL);

  screen = thunar_util_parse_parent (parent, &window);
  gtk_mount_operation_set_screen (GTK_MOUNT_OPERATION (mount_operation), screen);
  gtk_mount_operation_set_parent (GTK_MOUNT_OPERATION (mount_operation), window);

  return mount_operation;
}



static void
thunar_browser_poke_mountable_finish (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  PokeFileData *poke_data = user_data;
  ThunarFile   *target = NULL;
  GError       *error = NULL;
  GFile        *location;

  _thunar_return_if_fail (G_IS_FILE (object));
  _thunar_return_if_fail (G_IS_ASYNC_RESULT (result));
  _thunar_return_if_fail (user_data != NULL);
  _thunar_return_if_fail (THUNAR_IS_BROWSER (poke_data->browser));
  _thunar_return_if_fail (THUNAR_IS_FILE (poke_data->file));

  if (!g_file_mount_mountable_finish (G_FILE (object), result, &error))
    {
      if (error->domain == G_IO_ERROR)
        {
          if (error->code == G_IO_ERROR_ALREADY_MOUNTED)
            g_clear_error (&error);
        }
    }

  if (error == NULL)
    {
      thunar_file_reload (poke_data->file);

      location = thunar_file_get_target_location (poke_data->file);
      target = thunar_file_get (location, &error);
      g_object_unref (location);
    }

  if (poke_data->func != NULL)
    {
      (poke_data->func) (poke_data->browser, poke_data->source, target, error, 
                         poke_data->user_data);
    }

  g_clear_error (&error);

  if (target != NULL)
    g_object_unref (target);

  thunar_browser_poke_file_data_free (poke_data);
}



static void
thunar_browser_poke_file_finish (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  PokeFileData *poke_data = user_data;
  GError       *error = NULL;

  _thunar_return_if_fail (G_IS_FILE (object));
  _thunar_return_if_fail (G_IS_ASYNC_RESULT (result));
  _thunar_return_if_fail (user_data != NULL);
  _thunar_return_if_fail (THUNAR_IS_BROWSER (poke_data->browser));
  _thunar_return_if_fail (THUNAR_IS_FILE (poke_data->file));

  if (!g_file_mount_enclosing_volume_finish (G_FILE (object), result, &error))
    {
      if (error->domain == G_IO_ERROR)
        {
          if (error->code == G_IO_ERROR_ALREADY_MOUNTED)
            g_clear_error (&error);
        }
    }

  if (error == NULL)
    thunar_file_reload (poke_data->file);

  if (poke_data->func != NULL)
    {
      if (error == NULL)
        {
          (poke_data->func) (poke_data->browser, poke_data->source, poke_data->file, 
                             NULL, poke_data->user_data);
        }
      else
        {
          (poke_data->func) (poke_data->browser, poke_data->source, NULL, error,
                             poke_data->user_data);
        }
    }

  g_clear_error (&error);

  thunar_browser_poke_file_data_free (poke_data);
}



static void
thunar_browser_poke_file_internal (ThunarBrowser            *browser,
                                   ThunarFile               *source,
                                   ThunarFile               *file,
                                   gpointer                  widget,
                                   ThunarBrowserPokeFileFunc func,
                                   gpointer                  user_data)
{
  GMountOperation *mount_operation;
  ThunarFile      *target;
  PokeFileData    *poke_data;
  GError          *error = NULL;
  GFile           *location;

  _thunar_return_if_fail (THUNAR_IS_BROWSER (browser));
  _thunar_return_if_fail (THUNAR_IS_FILE (source));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  if (thunar_file_get_kind (file) == G_FILE_TYPE_SHORTCUT)
    {
      location = thunar_file_get_target_location (file);
      target = thunar_file_get (location, &error);
      g_object_unref (location);

      if (target != NULL)
        {
          /* TODO in very rare occasions (shortcut X -> other shortcut -> shortcut X),
           * this can lead to endless recursion  */
          thunar_browser_poke_file_internal (browser, source, target, widget, 
                                             func, user_data);
        }
      else
        {
          if (func != NULL)
            func (browser, source, NULL, error, user_data);
        }

      g_clear_error (&error);

      if (target != NULL)
        g_object_unref (target);
    }
  else if (thunar_file_get_kind (file) == G_FILE_TYPE_MOUNTABLE)
    {
      if (thunar_file_is_mounted (file))
        {
          location = thunar_file_get_target_location (file);
          target = thunar_file_get (location, &error);
          g_object_unref (location);

          if (func != NULL)
            func (browser, source, target, error, user_data);

          g_clear_error (&error);

          if (target != NULL)
            g_object_unref (target);
        }
      else
        {
          poke_data = thunar_browser_poke_file_data_new (browser, source, file,
                                                         func, user_data);

          mount_operation = thunar_browser_mount_operation_new (widget);

          g_file_mount_mountable (thunar_file_get_file (file), 
                                  G_MOUNT_MOUNT_NONE, mount_operation, NULL,
                                  thunar_browser_poke_mountable_finish,
                                  poke_data);

          g_object_unref (mount_operation);
        }
    }
  else if (!thunar_file_is_mounted (file))
    {
      poke_data = thunar_browser_poke_file_data_new (browser, source, file,
                                                     func, user_data);

      mount_operation = thunar_browser_mount_operation_new (widget);

      g_file_mount_enclosing_volume (thunar_file_get_file (file),
                                     G_MOUNT_MOUNT_NONE, mount_operation, NULL,
                                     thunar_browser_poke_file_finish,
                                     poke_data);

      g_object_unref (mount_operation);
    }
  else
    {
      if (func != NULL)
        func (browser, source, file, NULL, user_data);
    }
}



/**
 * thunar_browser_poke_file:
 * @browser : a #ThunarBrowser.
 * @file      : a #ThunarFile.
 * @widget    : a #GtkWidget, a #GdkScreen or %NULL.
 * @func      : a #ThunarBrowserPokeFileFunc callback or %NULL.
 * @user_data : pointer to arbitrary user data or %NULL.
 *
 * Pokes a #ThunarFile to see what's behind it.
 *
 * If @file has the type %G_FILE_TYPE_SHORTCUT, it tries to load and mount 
 * the file that is referred to by the %G_FILE_ATTRIBUTE_STANDARD_TARGET_URI
 * of the @file.
 *
 * If @file has the type %G_FILE_TYPE_MOUNTABLE, it tries to mount the @file.
 *
 * In the other cases, if the enclosing volume of the @file is not mounted
 * yet, it tries to mount it.
 *
 * When finished or if an error occured, it calls @func with the provided
 * @user data. The #GError parameter to @func is set if, and only if there
 * was an error.
 **/
void
thunar_browser_poke_file (ThunarBrowser            *browser,
                          ThunarFile               *file,
                          gpointer                  widget,
                          ThunarBrowserPokeFileFunc func,
                          gpointer                  user_data)
{
  _thunar_return_if_fail (THUNAR_IS_BROWSER (browser));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  thunar_browser_poke_file_internal (browser, file, file, widget, func, user_data);
}



static void
thunar_browser_poke_volume_finish (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  PokeVolumeData *poke_data = user_data;
  ThunarFile     *file;
  GError         *error = NULL;
  GMount         *mount;
  GFile          *mount_point;

  _thunar_return_if_fail (G_IS_VOLUME (object));
  _thunar_return_if_fail (G_IS_ASYNC_RESULT (result));
  _thunar_return_if_fail (user_data != NULL);
  _thunar_return_if_fail (THUNAR_IS_BROWSER (poke_data->browser));
  _thunar_return_if_fail (G_IS_VOLUME (poke_data->volume));
  _thunar_return_if_fail (G_VOLUME (object) == poke_data->volume);

  if (!g_volume_mount_finish (G_VOLUME (object), result, &error))
    {
      if (error->domain == G_IO_ERROR)
        {
          if (error->code == G_IO_ERROR_ALREADY_MOUNTED)
            g_clear_error (&error);
        }
    }

  if (error == NULL)
    {
      mount = g_volume_get_mount (poke_data->volume);
      mount_point = g_mount_get_root (mount);

      file = thunar_file_get (mount_point, &error);

      g_object_unref (mount_point);
      g_object_unref (mount);

      if (poke_data->func != NULL)
        {
          (poke_data->func) (poke_data->browser, poke_data->volume, file, error, 
                             poke_data->user_data);
        }

      if (file != NULL)
        g_object_unref (file);
    }
  else
    {
      if (poke_data->func != NULL)
        {
          (poke_data->func) (poke_data->browser, poke_data->volume, NULL, error,
                             poke_data->user_data);
        }
    }

  thunar_browser_poke_volume_data_free (poke_data);
}



/**
 * thunar_browser_poke_volume:
 * @browser   : a #ThunarBrowser.
 * @volume    : a #GVolume.
 * @widget    : a #GtkWidget, a #GdkScreen or %NULL.
 * @func      : a #ThunarBrowserPokeVolumeFunc callback or %NULL.
 * @user_data : pointer to arbitrary user data or %NULL.
 *
 * This function checks if @volume is mounted or not. If it is, it loads
 * a #ThunarFile for the mount root and calls @func. If it is not mounted,
 * it first mounts the volume asynchronously and calls @func with the
 * #ThunarFile corresponding to the mount root when the mounting is finished.
 *
 * The #ThunarFile passed to @func will be %NULL if, and only if mounting
 * the @volume failed. The #GError passed to @func will be set if, and only if
 * mounting failed.
 **/
void
thunar_browser_poke_volume (ThunarBrowser              *browser,
                            GVolume                    *volume,
                            gpointer                    widget,
                            ThunarBrowserPokeVolumeFunc func,
                            gpointer                    user_data)
{
  GMountOperation *mount_operation;
  PokeVolumeData  *poke_data;
  ThunarFile      *file;
  GError          *error = NULL;
  GMount          *mount;
  GFile           *mount_point;

  _thunar_return_if_fail (THUNAR_IS_BROWSER (browser));
  _thunar_return_if_fail (G_IS_VOLUME (volume));

  if (thunar_g_volume_is_mounted (volume))
    {
      mount = g_volume_get_mount (volume);
      mount_point = g_mount_get_root (mount);

      file = thunar_file_get (mount_point, &error);

      g_object_unref (mount_point);
      g_object_unref (mount);

      if (func != NULL)
        func (browser, volume, file, error, user_data);

      g_clear_error (&error);

      if (file != NULL)
        g_object_unref (file);
    }
  else
    {
      poke_data = thunar_browser_poke_volume_data_new (browser, volume, func, user_data);

      mount_operation = thunar_browser_mount_operation_new (widget);

      g_volume_mount (volume, G_MOUNT_MOUNT_NONE, mount_operation, NULL, 
                      thunar_browser_poke_volume_finish, poke_data);

      g_object_unref (mount_operation);
    }
}

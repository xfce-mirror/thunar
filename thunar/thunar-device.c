/*-
 * Copyright (c) 2012 Nick Schermer <nick@xfce.org>
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

#ifdef HAVE_LIBNOTIFY
#include <thunar/thunar-notify.h>
#endif
#include <thunar/thunar-device.h>
#include <thunar/thunar-private.h>



enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_KIND
};



static void           thunar_device_finalize               (GObject                *object);
static void           thunar_device_get_property           (GObject                 *object,
                                                            guint                    prop_id,
                                                            GValue                  *value,
                                                            GParamSpec              *pspec);
static void           thunar_device_set_property           (GObject                 *object,
                                                            guint                    prop_id,
                                                            const GValue            *value,
                                                            GParamSpec              *pspec);



struct _ThunarDeviceClass
{
  GObjectClass __parent__;
};

struct _ThunarDevice
{
  GObject __parent__;

  /* a GVolume/GMount/GDrive */
  gpointer          device;

  ThunarDeviceKind  kind;
};

typedef struct
{
  ThunarDevice         *device;
  ThunarDeviceCallback  callback;
  gpointer              user_data;
}
ThunarDeviceOperation;



G_DEFINE_TYPE (ThunarDevice, thunar_device, G_TYPE_OBJECT)



static void
thunar_device_class_init (ThunarDeviceClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_device_finalize;
  gobject_class->get_property = thunar_device_get_property;
  gobject_class->set_property = thunar_device_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_DEVICE,
                                   g_param_spec_object ("device",
                                                        "device",
                                                        "device",
                                                        G_TYPE_OBJECT,
                                                        EXO_PARAM_READWRITE
                                                        | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class,
                                   PROP_KIND,
                                   g_param_spec_uint ("kind",
                                                      "kind",
                                                      "kind",
                                                      THUNAR_DEVICE_KIND_VOLUME,
                                                      THUNAR_DEVICE_KIND_MOUNT_REMOTE,
                                                      THUNAR_DEVICE_KIND_VOLUME,
                                                      EXO_PARAM_READWRITE
                                                      | G_PARAM_CONSTRUCT_ONLY));
}



static void
thunar_device_init (ThunarDevice *device)
{
  device->kind = THUNAR_DEVICE_KIND_VOLUME;
}



static void
thunar_device_finalize (GObject *object)
{
  ThunarDevice *device = THUNAR_DEVICE (object);

  if (device->device != NULL)
    g_object_unref (G_OBJECT (device->device));

  (*G_OBJECT_CLASS (thunar_device_parent_class)->finalize) (object);
}



static void
thunar_device_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  ThunarDevice *device = THUNAR_DEVICE (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, device->device);
      break;

    case PROP_KIND:
      g_value_set_uint (value, device->kind);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_device_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  ThunarDevice *device = THUNAR_DEVICE (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      device->device = g_value_dup_object (value);
      _thunar_assert (G_IS_VOLUME (device->device) || G_IS_MOUNT (device->device));
      break;

    case PROP_KIND:
      device->kind = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}




static ThunarDeviceOperation *
thunar_device_operation_new (ThunarDevice         *device,
                             ThunarDeviceCallback  callback,
                             gpointer              user_data)
{
  ThunarDeviceOperation *operation;

  operation = g_slice_new0 (ThunarDeviceOperation);
  operation->device = g_object_ref (device);
  operation->callback = callback;
  operation->user_data = user_data;

  return operation;
}



static void
thunar_device_operation_free (ThunarDeviceOperation *operation)
{
  g_object_unref (operation->device);
  g_slice_free (ThunarDeviceOperation, operation);
}



static void
thunar_device_mount_unmount_finish (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  ThunarDeviceOperation *operation = user_data;
  GError                *error = NULL;

  _thunar_return_if_fail (G_IS_MOUNT (object));
  _thunar_return_if_fail (G_IS_ASYNC_RESULT (result));

#ifdef HAVE_LIBNOTIFY
  thunar_notify_unmount_finish (G_MOUNT (object));
#endif

  /* finish the unmount */
  if (!g_mount_unmount_with_operation_finish (G_MOUNT (object), result, &error))
    {
      /* unset the error if a helper program has already interacted with the user */
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED))
        g_clear_error (&error);
    }

  /* callback */
  (operation->callback) (operation->device, error, operation->user_data);

  /* cleanup */
  if (error != NULL)
    g_error_free (error);
  thunar_device_operation_free (operation);
}



static void
thunar_device_mount_eject_finish (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  ThunarDeviceOperation *operation = user_data;
  GError                *error = NULL;

  _thunar_return_if_fail (G_IS_MOUNT (object));
  _thunar_return_if_fail (G_IS_ASYNC_RESULT (result));

#ifdef HAVE_LIBNOTIFY
  thunar_notify_unmount_finish (G_MOUNT (object));
#endif

  /* finish the eject */
  if (!g_mount_eject_with_operation_finish (G_MOUNT (object), result, &error))
    {
      /* unset the error if a helper program has already interacted with the user */
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED))
        g_clear_error (&error);
    }

  /* callback */
  (operation->callback) (operation->device, error, operation->user_data);

  /* cleanup */
  if (error != NULL)
    g_error_free (error);
  thunar_device_operation_free (operation);
}



static void
thunar_device_volume_mount_finished (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  ThunarDeviceOperation *operation = user_data;
  GError                *error = NULL;

  _thunar_return_if_fail (G_IS_VOLUME (object));
  _thunar_return_if_fail (G_IS_ASYNC_RESULT (result));
  
  /* finish the eject */
  if (!g_volume_eject_with_operation_finish (G_VOLUME (object), result, &error))
    {
      /* unset the error if a helper program has already interacted with the user */
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED)
          || g_error_matches (error, G_IO_ERROR, G_IO_ERROR_ALREADY_MOUNTED)
          || g_error_matches (error, G_IO_ERROR, G_IO_ERROR_PENDING))
        g_clear_error (&error);
    }

  /* callback */
  (operation->callback) (operation->device, error, operation->user_data);

  /* cleanup */
  if (error != NULL)
    g_error_free (error);
  thunar_device_operation_free (operation);
}



static void
thunar_device_volume_eject_finish (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  ThunarDeviceOperation *operation = user_data;
  GError                *error = NULL;

  _thunar_return_if_fail (G_IS_VOLUME (object));
  _thunar_return_if_fail (G_IS_ASYNC_RESULT (result));

#ifdef HAVE_LIBNOTIFY
  thunar_notify_eject_finish (G_VOLUME (object));
#endif

  /* finish the eject */
  if (!g_volume_eject_with_operation_finish (G_VOLUME (object), result, &error))
    {
      /* unset the error if a helper program has already interacted with the user */
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED))
        g_clear_error (&error);
    }

  /* callback */
  (operation->callback) (operation->device, error, operation->user_data);

  /* cleanup */
  if (error != NULL)
    g_error_free (error);
  thunar_device_operation_free (operation);
}



gchar *
thunar_device_get_name (const ThunarDevice *device)
{
  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), NULL);

  if (G_IS_VOLUME (device->device))
    return g_volume_get_name (device->device);
  else if (G_IS_MOUNT (device->device))
   return g_mount_get_name (device->device);
  else
    _thunar_assert_not_reached ();

  return NULL;
}



GIcon *
thunar_device_get_icon (const ThunarDevice *device)
{
  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), NULL);

  if (G_IS_VOLUME (device->device))
    return g_volume_get_icon (device->device);
  else if (G_IS_MOUNT (device->device))
   return g_mount_get_icon (device->device);
  else
    _thunar_assert_not_reached ();

  return NULL;
}



ThunarDeviceKind
thunar_device_get_kind (const ThunarDevice *device)
{
  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), THUNAR_DEVICE_KIND_VOLUME);
  return device->kind;
}



/**
 * thunar_device_can_eject:
 *
 * If the user should see the option to eject this device.
 **/
gboolean
thunar_device_can_eject (const ThunarDevice *device)
{
  gboolean  can_eject = FALSE;
  GMount   *volume_mount;

  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), FALSE);

  if (G_IS_VOLUME (device->device))
    {
      can_eject = g_volume_can_eject (device->device);

      if (!can_eject)
        {
          /* check if the mount can eject/unmount */
          volume_mount = g_volume_get_mount (device->device);
          if (volume_mount != NULL)
            {
              can_eject = g_mount_can_eject (volume_mount) || g_mount_can_unmount (volume_mount);
              g_object_unref (volume_mount);
            }
        }
    }
  else if (G_IS_MOUNT (device->device))
    {
      /* eject or unmount because thats for the user the same
       * because we prefer volumes over mounts as devices */
      can_eject = g_mount_can_eject (device->device) || g_mount_can_unmount (device->device);
    }
  else
    _thunar_assert_not_reached ();

  return can_eject;
}



/**
 * thunar_device_can_mount:
 *
 * If the user should see the option to mount this device.
 **/
gboolean
thunar_device_can_mount (const ThunarDevice *device)
{
  gboolean  can_mount = FALSE;
  GMount   *volume_mount;

  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), FALSE);

  if (G_IS_VOLUME (device->device))
    {
      /* only volumes without a mountpoint and mount capability */
      volume_mount = g_volume_get_mount (device->device);
      if (volume_mount == NULL)
        can_mount = g_volume_can_mount (device->device);
      else
        g_object_unref (volume_mount);
    }
  else if (G_IS_MOUNT (device->device))
    {
      /* a mount is already mounted... */
      can_mount = FALSE;
    }
  else
    _thunar_assert_not_reached ();

  return can_mount;
}



/**
 * thunar_device_can_unmount:
 *
 * If the user should see the option to unmount this device.
 **/
gboolean
thunar_device_can_unmount (const ThunarDevice *device)
{
  gboolean  can_unmount = FALSE;
  GMount   *volume_mount;

  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), FALSE);

  if (G_IS_VOLUME (device->device))
    {
      /* only volumes with a mountpoint and unmount capability */
      volume_mount = g_volume_get_mount (device->device);
      if (volume_mount != NULL)
        {
          can_unmount = g_mount_can_unmount (volume_mount);
          g_object_unref (volume_mount);
        }
    }
  else if (G_IS_MOUNT (device->device))
    {
      /* check if the mount can unmount */
      can_unmount = g_mount_can_unmount (device->device);
    }
  else
    _thunar_assert_not_reached ();

  return can_unmount;
}



gboolean
thunar_device_is_mounted (const ThunarDevice *device)
{
  gboolean  is_mounted = FALSE;
  GMount   *volume_mount;

  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), FALSE);
  
  if (G_IS_VOLUME (device->device))
    {
      /* a volume with a mount point is mounted */
      volume_mount = g_volume_get_mount (device->device);
      if (volume_mount != NULL)
        {
          is_mounted = TRUE;
          g_object_unref (volume_mount);
        }
    }
  else if (G_IS_MOUNT (device->device))
    {
      /* mounter are always mounted... */
      is_mounted = TRUE;
    }
  else
    _thunar_assert_not_reached ();

  return is_mounted;
}



GFile *
thunar_device_get_root (const ThunarDevice *device)
{
  GFile  *root = NULL;
  GMount *volume_mount;

  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), NULL);

  if (G_IS_VOLUME (device->device))
    {
      volume_mount = g_volume_get_mount (device->device);
      if (volume_mount != NULL)
        {
          root = g_mount_get_root (volume_mount);
          g_object_unref (volume_mount);
        }

      if (root == NULL)
        root = g_volume_get_activation_root (device->device);
    }
  else if (G_IS_MOUNT (device->device))
    {
      root = g_mount_get_root (device->device);
    }
  else
    _thunar_assert_not_reached ();

  return root;
}



const gchar *
thunar_device_get_sort_key (const ThunarDevice *device)
{
  _thunar_return_val_if_fail (THUNAR_IS_DEVICE (device), NULL);

  if (G_IS_VOLUME (device->device))
    return g_volume_get_sort_key (device->device);
  else if (G_IS_MOUNT (device->device))
    return g_mount_get_sort_key (device->device);
  else
    _thunar_assert_not_reached ();

  return NULL;
}



void
thunar_device_mount (ThunarDevice         *device,
                     GMountOperation      *mount_operation,
                     GCancellable         *cancellable,
                     ThunarDeviceCallback  callback,
                     gpointer              user_data)
{
  ThunarDeviceOperation *operation;
  
  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (G_IS_MOUNT_OPERATION (mount_operation));
  _thunar_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  _thunar_return_if_fail (callback != NULL);
  
  if (G_IS_VOLUME (device->device))
    {
      operation = thunar_device_operation_new (device, callback, user_data);
      g_volume_mount (G_VOLUME (device->device),
                      G_MOUNT_MOUNT_NONE,
                      mount_operation,
                      cancellable,
                      thunar_device_volume_mount_finished,
                      operation);
    }
}



/**
 * thunar_device_unmount:
 *
 * Unmount a #ThunarDevice. Don't try to eject.
 **/
void
thunar_device_unmount (ThunarDevice         *device,
                       GMountOperation      *mount_operation,
                       GCancellable         *cancellable,
                       ThunarDeviceCallback  callback,
                       gpointer              user_data)
{
  ThunarDeviceOperation *operation;
  GMount                *mount;

  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (G_IS_MOUNT_OPERATION (mount_operation));
  _thunar_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  _thunar_return_if_fail (callback != NULL);

  /* get the mount from the volume or use existing mount */
  if (G_IS_VOLUME (device->device))
    mount = g_volume_get_mount (device->device);
  else if (G_IS_MOUNT (device->device))
    mount = g_object_ref (device->device);
  else
    mount = NULL;

  if (G_LIKELY (mount != NULL))
    {
      /* only handle mounts that can be unmounted here */
      if (g_mount_can_unmount (mount))
        {
#ifdef HAVE_LIBNOTIFY
          thunar_notify_unmount (mount);
#endif

          /* try unmounting the mount */
          operation = thunar_device_operation_new (device, callback, user_data);
          g_mount_unmount_with_operation (mount,
                                          G_MOUNT_UNMOUNT_NONE,
                                          mount_operation,
                                          cancellable,
                                          thunar_device_mount_unmount_finish,
                                          operation);
        }

      g_object_unref (G_OBJECT (mount));
    }
}



/**
 * thunar_device_unmount:
 *
 * Try to eject a #ThunarDevice, fall-back to unmounting
 **/
void
thunar_device_eject (ThunarDevice         *device,
                     GMountOperation      *mount_operation,
                     GCancellable         *cancellable,
                     ThunarDeviceCallback  callback,
                     gpointer              user_data)
{
  ThunarDeviceOperation *operation;
  GMount                *mount = NULL;
  GVolume               *volume;

  _thunar_return_if_fail (THUNAR_IS_DEVICE (device));
  _thunar_return_if_fail (G_IS_MOUNT_OPERATION (mount_operation));
  _thunar_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  _thunar_return_if_fail (callback != NULL);


  if (G_IS_VOLUME (device->device))
    {
      volume = device->device;

      if (g_volume_can_eject (volume))
        {
#ifdef HAVE_LIBNOTIFY
          thunar_notify_eject (volume);
#endif

          /* try ejecting the volume */
          operation = thunar_device_operation_new (device, callback, user_data);
          g_volume_eject_with_operation (volume,
                                         G_MOUNT_UNMOUNT_NONE,
                                         mount_operation,
                                         cancellable,
                                         thunar_device_volume_eject_finish,
                                         operation);

          /* done */
          return;
        }
      else
        {
          /* get the mount and fall-through */
          mount = g_volume_get_mount (volume);
        }
    }
  else if (G_IS_MOUNT (device->device))
    {
      /* set the mount and fall-through */
      mount = g_object_ref (device->device);
    }

  /* handle mounts */
  if (mount != NULL)
    {
      /* distinguish between ejectable and unmountable mounts */
      if (g_mount_can_eject (mount))
        {
#ifdef HAVE_LIBNOTIFY
          thunar_notify_unmount (mount);
#endif

          /* try ejecting the mount */
          operation = thunar_device_operation_new (device, callback, user_data);
          g_mount_eject_with_operation (mount,
                                        G_MOUNT_UNMOUNT_NONE,
                                        mount_operation,
                                        cancellable,
                                        thunar_device_mount_eject_finish,
                                        operation);
        }
      else if (g_mount_can_unmount (mount))
        {
#ifdef HAVE_LIBNOTIFY
          thunar_notify_unmount (mount);
#endif

          /* try unmounting the mount */
          operation = thunar_device_operation_new (device, callback, user_data);
          g_mount_unmount_with_operation (mount,
                                          G_MOUNT_UNMOUNT_NONE,
                                          mount_operation,
                                          cancellable,
                                          thunar_device_mount_unmount_finish,
                                          operation);
        }

      g_object_unref (G_OBJECT (mount));
    }
}

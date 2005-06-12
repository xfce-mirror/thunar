/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <exo/exo.h>

#include <thunar-vfs/thunar-vfs-enum-types.h>
#include <thunar-vfs/thunar-vfs-volume.h>
#include <thunar-vfs/thunar-vfs-volume-bsd.h>



static void thunar_vfs_volume_class_init (gpointer klass);



GType
thunar_vfs_volume_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsVolumeIface),
        NULL,
        NULL,
        (GClassInitFunc) thunar_vfs_volume_class_init,
        NULL,
        NULL,
        0,
        0,
        NULL,
      };

      type = g_type_register_static (G_TYPE_INTERFACE,
                                     "ThunarVfsVolume",
                                     &info, 0);

      g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
    }

  return type;
}



static void
thunar_vfs_volume_class_init (gpointer klass)
{
  /**
   * ThunarVfsVolume:kind:
   *
   * The kind of the volume (e.g. cdrom, floppy or harddisk).
   **/
  g_object_interface_install_property (klass,
                                       g_param_spec_enum ("kind",
                                                          _("Kind"),
                                                          _("The kind of the volume"),
                                                          THUNAR_VFS_TYPE_VFS_VOLUME_KIND,
                                                          THUNAR_VFS_VOLUME_KIND_UNKNOWN,
                                                          G_PARAM_READABLE));

  /**
   * ThunarVfsVolume:mount-point:
   *
   * The path in the file system where this volume is mounted
   * or will be mounted.
   **/
  g_object_interface_install_property (klass,
                                       g_param_spec_object ("mount-point",
                                                            _("Mount point"),
                                                            _("The mount point of the volume"),
                                                            THUNAR_VFS_TYPE_URI,
                                                            G_PARAM_READABLE));

  /**
   * ThunarVfsVolume:mount-status:
   *
   * Indicates whether or not, this volume is currently mounted
   * by the kernel.
   **/
  g_object_interface_install_property (klass,
                                       g_param_spec_boolean ("mount-status",
                                                             _("Mount status"),
                                                             _("The mount status of the volume"),
                                                             FALSE,
                                                             G_PARAM_READABLE));
}



/**
 * thunar_vfs_volume_get_kind:
 * @volume : a #ThunarVfsVolume instance.
 *
 * Returns the kind of drive/device representd by @volume.
 *
 * Return value: the kind of @volume.
 **/
ThunarVfsVolumeKind
thunar_vfs_volume_get_kind (ThunarVfsVolume *volume)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), THUNAR_VFS_VOLUME_KIND_UNKNOWN);
  return THUNAR_VFS_VOLUME_GET_IFACE (volume)->get_kind (volume);
}



/**
 * thunar_vfs_volume_get_mount_point:
 * @volume : a #ThunarVfsVolume instance.
 *
 * Return value: the URI which identifies the path where
 *               the volume will be mounted (or is already
 *               mounted).
 **/
ThunarVfsURI*
thunar_vfs_volume_get_mount_point (ThunarVfsVolume *volume)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), NULL);
  return THUNAR_VFS_VOLUME_GET_IFACE (volume)->get_mount_point (volume);
}



/**
 * thunar_vfs_volume_get_mount_status:
 * @volume : a #ThunarVfsVolume instance.
 *
 * Determines whether the @volume is currently mounted
 * or not.
 *
 * Return value: %TRUE if @volume is mounted, else %FALSE.
 **/
gboolean
thunar_vfs_volume_get_mount_status (ThunarVfsVolume *volume)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), FALSE);
  return THUNAR_VFS_VOLUME_GET_IFACE (volume)->get_mount_status (volume);
}




enum
{
  VOLUMES_ADDED,
  VOLUMES_REMOVED,
  LAST_SIGNAL,
};



static void thunar_vfs_volume_manager_base_init  (gpointer klass);



static guint manager_signals[LAST_SIGNAL];



GType
thunar_vfs_volume_manager_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsVolumeManagerIface),
        (GBaseInitFunc) thunar_vfs_volume_manager_base_init,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        0,
        NULL,
      };

      type = g_type_register_static (G_TYPE_INTERFACE,
                                     "ThunarVfsVolumeManager",
                                     &info, 0);

      g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
    }

  return type;
}



static void
thunar_vfs_volume_manager_base_init (gpointer klass)
{
  static gboolean initialized = FALSE;

  if (G_UNLIKELY (!initialized))
    {
      /**
       * ThunarVfsVolumeManager::volumes-added:
       * @manager : a #ThunarVfsVolumeManager instance.
       * @volumes : a list of #ThunarVfsVolume<!---->s.
       *
       * Invoked by the @manager whenever new volumes have been
       * attached to the system or the administrator changes the
       * /etc/fstab file, or some other condition, depending
       * on the manager implementation.
       *
       * Note that the implementation should not invoke this
       * method when a volume is mounted, as that's a completely
       * different condition!
       **/
      manager_signals[VOLUMES_ADDED] =
        g_signal_new ("volumes-added",
                      G_TYPE_FROM_INTERFACE (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (ThunarVfsVolumeManagerIface, volumes_added),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);

      /**
       * ThunarVfsVolumeManager::volumes-removed:
       * @manager : a #ThunarVfsVolume instance.
       * @volumes : a list of #ThunarVfsVolume<!---->s.
       *
       * Invoked whenever the @manager notices that @volumes have
       * been detached from the system.
       **/
      manager_signals[VOLUMES_REMOVED] =
        g_signal_new ("volumes-removed",
                      G_TYPE_FROM_INTERFACE (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (ThunarVfsVolumeManagerIface, volumes_removed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);

      initialized = TRUE;
    }
}



/**
 * thunar_vfs_volume_manager_get_default:
 * 
 * Returns the default, shared #ThunarVfsVolumeManager instance
 * for this system. This function automatically determines, which
 * implementation of #ThunarVfsVolumeManager should be used for
 * the target system and returns an instance of that class, which
 * is shared among all modules using the volume manager facility.
 *
 * Call #g_object_unref() on the returned object when you are
 * done with it.
 *
 * Return value: the shared #ThunarVfsVolumeManager instance.
 **/
ThunarVfsVolumeManager*
thunar_vfs_volume_manager_get_default (void)
{
  static ThunarVfsVolumeManager *manager = NULL;

  if (G_UNLIKELY (manager == NULL))
    {
      manager = g_object_new (THUNAR_VFS_TYPE_VOLUME_MANAGER_BSD, NULL);
      g_object_add_weak_pointer (G_OBJECT (manager), (gpointer) &manager);
    }
  else
    {
      g_object_ref (G_OBJECT (manager));
    }

  return manager;
}



/**
 * thunar_vfs_volume_manager_get_volumes:
 * @manager : a #ThunarVfsVolumeManager instance.
 *
 * Returns all #ThunarVfsVolume<!---->s currently known for
 * @manager. The returned list is owned by @manager and should
 * therefore considered constant in the caller.
 *
 * Return value: the list of volumes known for @manager.
 **/
GList*
thunar_vfs_volume_manager_get_volumes (ThunarVfsVolumeManager *manager)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER (manager), NULL);
  return THUNAR_VFS_VOLUME_MANAGER_GET_IFACE (manager)->get_volumes (manager);
}



/**
 * thunar_vfs_volume_manager_volumes_added:
 * @manager : a #ThunarVfsVolumeManager instance.
 * @volumes : a list of #ThunarVfsVolume<!---->s.
 *
 * Emits the "volumes-added" signal on @manager using the
 * given @volumes.
 *
 * This method should only be used by classes implementing
 * the #ThunarVfsVolumeManager interface.
 **/
void
thunar_vfs_volume_manager_volumes_added (ThunarVfsVolumeManager *manager,
                                         GList                  *volumes)
{
  g_return_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER (manager));
  g_return_if_fail (g_list_length (volumes) > 0);
  g_signal_emit (G_OBJECT (manager), manager_signals[VOLUMES_ADDED], 0, volumes);
}



/**
 * thunar_vfs_volume_manager_volumes_removed:
 * @manager : a #ThunarVfsVolumeManager instance.
 * @volumes : a list of #ThunarVfsVolume<!---->s.
 *
 * Emits the "volumes-removed" signal on @manager using
 * the given @volumes.
 *
 * This method should only be used by classes implementing
 * the #ThunarVfsVolumeManager interface.
 **/
void
thunar_vfs_volume_manager_volumes_removed (ThunarVfsVolumeManager *manager,
                                           GList                  *volumes)
{
  g_return_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER (manager));
  g_return_if_fail (g_list_length (volumes) > 0);
  g_signal_emit (G_OBJECT (manager), manager_signals[VOLUMES_REMOVED], 0, volumes);
}



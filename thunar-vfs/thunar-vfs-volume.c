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



enum
{
  THUNAR_VFS_VOLUME_CHANGED,
  THUNAR_VFS_VOLUME_LAST_SIGNAL,
};



static void thunar_vfs_volume_base_init  (gpointer klass);



static guint volume_signals[THUNAR_VFS_VOLUME_CHANGED];



GType
thunar_vfs_volume_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsVolumeIface),
        (GBaseInitFunc) thunar_vfs_volume_base_init,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        0,
        NULL,
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
thunar_vfs_volume_base_init (gpointer klass)
{
  static gboolean initialized = FALSE;

  if (G_UNLIKELY (!initialized))
    {
      /**
       * ThunarVfsVolume::changed:
       * @volume : the #ThunarVfsVolume instance.
       *
       * Emitted whenever the state of @volume changed.
       **/
      volume_signals[THUNAR_VFS_VOLUME_CHANGED] =
        g_signal_new ("changed",
                      G_TYPE_FROM_INTERFACE (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (ThunarVfsVolumeIface, changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

      initialized = TRUE;
    }
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
 * thunar_vfs_volume_get_name;
 * @volume : a #ThunarVfsVolume instance.
 *
 * Returns the name of the @volume. This is usually the
 * name of the device or the label of the medium, if a
 * medium is present.
 *
 * Return value: the name of @volume.
 **/
const gchar*
thunar_vfs_volume_get_name (ThunarVfsVolume *volume)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), NULL);
  return THUNAR_VFS_VOLUME_GET_IFACE (volume)->get_name (volume);
}



/**
 * thunar_vfs_volume_get_status:
 * @volume : a #ThunarVfsVolume instance.
 *
 * Determines the current status of the @volume, e.g. whether
 * or not the @volume is currently mounted, or whether a
 * medium is present.
 *
 * Return value: the status for @volume.
 **/
ThunarVfsVolumeStatus
thunar_vfs_volume_get_status (ThunarVfsVolume *volume)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), 0);
  return THUNAR_VFS_VOLUME_GET_IFACE (volume)->get_status (volume);
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
 * thunar_vfs_volume_is_mounted:
 * @volume : a #ThunarVfsVolume instance.
 *
 * Determines whether @volume is currently mounted into the
 * filesystem hierarchy.
 *
 * Return value: %TRUE if @volume is mounted, else %FALSE.
 **/
gboolean
thunar_vfs_volume_is_mounted (ThunarVfsVolume *volume)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), FALSE);
  return THUNAR_VFS_VOLUME_GET_IFACE (volume)->get_status (volume) & THUNAR_VFS_VOLUME_STATUS_MOUNTED;
}



/**
 * thunar_vfs_volume_is_present:
 * @volume : a #ThunarVfsVolume instance.
 *
 * Determines whether a medium is currently inserted for
 * @volume, e.g. for a CD-ROM drive, this will be %TRUE
 * only if a disc is present in the slot.
 *
 * Return value: %TRUE if @volume is present, else %FALSE.
 **/
gboolean
thunar_vfs_volume_is_present (ThunarVfsVolume *volume)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), FALSE);
  return THUNAR_VFS_VOLUME_GET_IFACE (volume)->get_status (volume) & THUNAR_VFS_VOLUME_STATUS_PRESENT;
}



/**
 * thunar_vfs_volume_is_removable:
 * @volume : a #ThunarVfsVolume instance.
 *
 * Determines whether @volume is a removable device, for example
 * a CD-ROM or a floppy drive.
 *
 * Return value: %TRUE if @volume is a removable device, else %FALSE.
 **/
gboolean
thunar_vfs_volume_is_removable (ThunarVfsVolume *volume)
{
  ThunarVfsVolumeKind kind;

  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), FALSE);

  kind = thunar_vfs_volume_get_kind (volume);

  switch (kind)
    {
    case THUNAR_VFS_VOLUME_KIND_CDROM:
    case THUNAR_VFS_VOLUME_KIND_FLOPPY:
      return TRUE;

    case THUNAR_VFS_VOLUME_KIND_UNKNOWN:
    case THUNAR_VFS_VOLUME_KIND_HARDDISK:
      return FALSE;

    default:
      g_assert_not_reached ();
      return FALSE;
    }
}



/**
 * thunar_vfs_volume_lookup_icon:
 * @volume     : a #ThunarVfsVolume instance.
 * @icon_theme : a #GtkIconTheme instance.
 * @size       : the size of the icon in pixels.
 * @flags      : the icon theme lookup flags.
 *
 * Tries to find a suitable icon for @volume in the given @icon_theme. If
 * no suitable icon can be found, %NULL will be returned instead.
 *
 * Call #gtk_icon_info_free() on the returned icon info object when you
 * are done with it.
 *
 * Return value: a #GtkIconInfo or %NULL.
 **/
GtkIconInfo*
thunar_vfs_volume_lookup_icon (ThunarVfsVolume   *volume,
                               GtkIconTheme      *icon_theme,
                               gint               size,
                               GtkIconLookupFlags flags)
{
  ThunarVfsVolumeKind kind;
  GtkIconInfo        *icon_info;

  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), NULL);
  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);
  g_return_val_if_fail (size > 0, NULL);

  kind = thunar_vfs_volume_get_kind (volume);
  switch (kind)
    {
    case THUNAR_VFS_VOLUME_KIND_CDROM:
      icon_info = gtk_icon_theme_lookup_icon (icon_theme, "gnome-dev-cdrom", size, flags);
      if (icon_info != NULL)
        return icon_info;
      break;

    case THUNAR_VFS_VOLUME_KIND_FLOPPY:
      icon_info = gtk_icon_theme_lookup_icon (icon_theme, "gnome-dev-floppy", size, flags);
      if (icon_info != NULL)
        return icon_info;
      break;

    default:
      break;
    }

  return gtk_icon_theme_lookup_icon (icon_theme, "gnome-fs-blockdev", size, flags);
}



/**
 * thunar_vfs_volume_changed:
 * @volume : a #ThunarVfsVolume instance.
 *
 * Emits the "changed" signal on @volume. This function should
 * only be used by implementations of the #ThunarVfsVolume
 * interface.
 **/
void
thunar_vfs_volume_changed (ThunarVfsVolume *volume)
{
  g_return_if_fail (THUNAR_VFS_IS_VOLUME (volume));
  g_signal_emit (G_OBJECT (volume), volume_signals[THUNAR_VFS_VOLUME_CHANGED], 0);
}




enum
{
  THUNAR_VFS_VOLUME_MANAGER_VOLUMES_ADDED,
  THUNAR_VFS_VOLUME_MANAGER_VOLUMES_REMOVED,
  THUNAR_VFS_VOLUME_MANAGER_LAST_SIGNAL,
};



static void thunar_vfs_volume_manager_base_init  (gpointer klass);



static guint manager_signals[THUNAR_VFS_VOLUME_MANAGER_LAST_SIGNAL];



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
      manager_signals[THUNAR_VFS_VOLUME_MANAGER_VOLUMES_ADDED] =
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
      manager_signals[THUNAR_VFS_VOLUME_MANAGER_VOLUMES_REMOVED] =
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
  extern GType _thunar_vfs_volume_manager_impl_get_type (void);
  static ThunarVfsVolumeManager *manager = NULL;

  if (G_UNLIKELY (manager == NULL))
    {
      manager = g_object_new (_thunar_vfs_volume_manager_impl_get_type (), NULL);
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
  g_signal_emit (G_OBJECT (manager), manager_signals[THUNAR_VFS_VOLUME_MANAGER_VOLUMES_ADDED], 0, volumes);
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
  g_signal_emit (G_OBJECT (manager), manager_signals[THUNAR_VFS_VOLUME_MANAGER_VOLUMES_REMOVED], 0, volumes);
}



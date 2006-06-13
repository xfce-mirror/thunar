/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#include <thunar-vfs/thunar-vfs-enum-types.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-volume-freebsd.h>
#include <thunar-vfs/thunar-vfs-volume-hal.h>
#include <thunar-vfs/thunar-vfs-volume-none.h>
#include <thunar-vfs/thunar-vfs-volume-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>



/* determine the default thunar vfs volume manager type */
#if defined(THUNAR_VFS_VOLUME_IMPL_FREEBSD)
#define THUNAR_VFS_TYPE_VOLUME_MANAGER_IMPL THUNAR_VFS_TYPE_VOLUME_MANAGER_FREEBSD
#elif defined(THUNAR_VFS_VOLUME_IMPL_HAL)
#define THUNAR_VFS_TYPE_VOLUME_MANAGER_IMPL THUNAR_VFS_TYPE_VOLUME_MANAGER_HAL
#elif defined(THUNAR_VFS_VOLUME_IMPL_NONE)
#define THUNAR_VFS_TYPE_VOLUME_MANAGER_IMPL THUNAR_VFS_TYPE_VOLUME_MANAGER_NONE
#else
#error "Add your volume manager implemenation here!"
#endif



/* Signal identifiers */
enum
{
  VOLUMES_ADDED,
  VOLUMES_REMOVED,
  VOLUME_MOUNTED,
  VOLUME_PRE_UNMOUNT,
  VOLUME_UNMOUNTED,
  LAST_SIGNAL,
};



static void             thunar_vfs_volume_manager_class_init              (ThunarVfsVolumeManagerClass  *klass);
static void             thunar_vfs_volume_manager_finalize                (GObject                      *object);
static ThunarVfsVolume *thunar_vfs_volume_manager_real_get_volume_by_info (ThunarVfsVolumeManager       *manager, 
                                                                           const ThunarVfsInfo          *info);
static void             thunar_vfs_volume_manager_volume_mounted          (ThunarVfsVolumeManager       *manager,
                                                                           ThunarVfsVolume              *volume);
static void             thunar_vfs_volume_manager_volume_pre_unmount      (ThunarVfsVolumeManager       *manager,
                                                                           ThunarVfsVolume              *volume);
static void             thunar_vfs_volume_manager_volume_unmounted        (ThunarVfsVolumeManager       *manager,
                                                                           ThunarVfsVolume              *volume);



static GObjectClass *thunar_vfs_volume_manager_parent_class;
static guint         manager_signals[LAST_SIGNAL];



GType
thunar_vfs_volume_manager_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = _thunar_vfs_g_type_register_simple (G_TYPE_OBJECT,
                                                 "ThunarVfsVolumeManager",
                                                 sizeof (ThunarVfsVolumeManagerClass),
                                                 thunar_vfs_volume_manager_class_init,
                                                 sizeof (ThunarVfsVolumeManager),
                                                 NULL,
                                                 G_TYPE_FLAG_ABSTRACT);
    }

  return type;
}



static void
thunar_vfs_volume_manager_class_init (ThunarVfsVolumeManagerClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_vfs_volume_manager_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_volume_manager_finalize;

  /* we provide a fallback implementation */
  klass->get_volume_by_info = thunar_vfs_volume_manager_real_get_volume_by_info;

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
    g_signal_new (I_("volumes-added"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarVfsVolumeManagerClass, volumes_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * ThunarVfsVolumeManager::volumes-removed:
   * @manager : a #ThunarVfsVolumeManager instance.
   * @volumes : a list of #ThunarVfsVolume<!---->s.
   *
   * Invoked whenever the @manager notices that @volumes have
   * been detached from the system.
   **/
  manager_signals[VOLUMES_REMOVED] =
    g_signal_new (I_("volumes-removed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarVfsVolumeManagerClass, volumes_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * ThunarVfsVolumeManager::volume-mounted:
   * @manager : a #ThunarVfsVolumeManager instance.
   * @volume  : a #ThunarVfsVolume instance.
   *
   * Emitted by @manager right after the @volume was
   * successfully mounted.
   **/
  manager_signals[VOLUME_MOUNTED] =
    g_signal_new (I_("volume-mounted"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarVfsVolumeManagerClass, volume_mounted),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, THUNAR_VFS_TYPE_VOLUME);

  /**
   * ThunarVfsVolumeManager::volume-pre-unmount:
   * @manager : a #ThunarVfsVolumeManager instance.
   * @volume  : a #ThunarVfsVolume instance.
   *
   * Emitted by @manager right before an attempt is
   * made to unmount @volume. Applications can connect
   * to this signal to perform cleanups, like changing
   * to a different folder if a folder on the @volume
   * is currently displayed.
   **/
  manager_signals[VOLUME_PRE_UNMOUNT] =
    g_signal_new (I_("volume-pre-unmount"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarVfsVolumeManagerClass, volume_pre_unmount),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, THUNAR_VFS_TYPE_VOLUME);

  /**
   * ThunarVfsVolumeManager::volume-unmounted:
   * @manager : a #ThunarVfsVolumeManager instance.
   * @volume  : a #ThunarVfsVolume instance.
   *
   * Emitted by @manager right after the @volume was
   * successfully unmounted.
   **/
  manager_signals[VOLUME_UNMOUNTED] =
    g_signal_new (I_("volume-unmounted"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarVfsVolumeManagerClass, volume_unmounted),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, THUNAR_VFS_TYPE_VOLUME);
}



static void
thunar_vfs_volume_manager_finalize (GObject *object)
{
  ThunarVfsVolumeManager *manager = THUNAR_VFS_VOLUME_MANAGER (object);
  ThunarVfsVolume        *volume;

  /* drop all managed volumes */
  while (manager->volumes != NULL)
    {
      /* grab the first volume */
      volume = THUNAR_VFS_VOLUME (manager->volumes->data);

      /* verify that we're the only one keeping a reference on it */
      if (G_UNLIKELY (G_OBJECT (volume)->ref_count > 1))
        {
          g_warning ("Attempt detected to finalize the ThunarVfsVolumeManager, while "
                     "there are still ThunarVfsVolume instances with a reference count "
                     "of 2 or more. This usually presents a bug in the application.");
        }

      /* disconnect from the volume */
      thunar_vfs_volume_manager_remove (manager, volume);
    }

  (*G_OBJECT_CLASS (thunar_vfs_volume_manager_parent_class)->finalize) (object);
}



static ThunarVfsVolume*
thunar_vfs_volume_manager_real_get_volume_by_info (ThunarVfsVolumeManager *manager, 
                                                   const ThunarVfsInfo    *info)
{
  ThunarVfsVolume *best_volume = NULL;
  ThunarVfsPath   *best_path = NULL;
  ThunarVfsPath   *path;
  GList           *lp;

  /* otherwise try to find it the hard way */
  for (lp = manager->volumes; lp != NULL; lp = lp->next)
    {
      /* check if the volume is mounted */
      if (!thunar_vfs_volume_is_mounted (lp->data))
        continue;

      /* check if the mount point is an ancestor of the info path */
      path = thunar_vfs_volume_get_mount_point (lp->data);
      if (path == NULL || (!thunar_vfs_path_equal (info->path, path) && !thunar_vfs_path_is_ancestor (info->path, path)))
        continue;

      /* possible match, check if better than previous match */
      if (best_volume == NULL || thunar_vfs_path_equal (path, best_path) || thunar_vfs_path_is_ancestor (path, best_path))
        {
          best_volume = lp->data;
          best_path = path;
        }
    }

  return best_volume;
}



static void
thunar_vfs_volume_manager_volume_mounted (ThunarVfsVolumeManager *manager,
                                          ThunarVfsVolume        *volume)
{
  g_return_if_fail (THUNAR_VFS_IS_VOLUME (volume));
  g_return_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER (manager));
  g_return_if_fail (g_list_find (manager->volumes, volume) != NULL);
  g_signal_emit (G_OBJECT (manager), manager_signals[VOLUME_MOUNTED], 0, volume);
}



static void
thunar_vfs_volume_manager_volume_pre_unmount (ThunarVfsVolumeManager *manager,
                                              ThunarVfsVolume        *volume)
{
  g_return_if_fail (THUNAR_VFS_IS_VOLUME (volume));
  g_return_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER (manager));
  g_return_if_fail (g_list_find (manager->volumes, volume) != NULL);
  g_signal_emit (G_OBJECT (manager), manager_signals[VOLUME_PRE_UNMOUNT], 0, volume);
}



static void
thunar_vfs_volume_manager_volume_unmounted (ThunarVfsVolumeManager *manager,
                                            ThunarVfsVolume        *volume)
{
  g_return_if_fail (THUNAR_VFS_IS_VOLUME (volume));
  g_return_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER (manager));
  g_return_if_fail (g_list_find (manager->volumes, volume) != NULL);
  g_signal_emit (G_OBJECT (manager), manager_signals[VOLUME_UNMOUNTED], 0, volume);
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
 * Call g_object_unref() on the returned object when you are
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
      manager = g_object_new (THUNAR_VFS_TYPE_VOLUME_MANAGER_IMPL, NULL);
      g_object_add_weak_pointer (G_OBJECT (manager), (gpointer) &manager);
    }
  else
    {
      g_object_ref (G_OBJECT (manager));
    }

  return manager;
}



/**
 * thunar_vfs_volume_manager_get_volume_by_info:
 * @manager : a #ThunarVfsVolumeManager instance.
 * @info    : a #ThunarVfsInfo.
 *
 * Tries to lookup the #ThunarVfsVolume on which @info is
 * located. If @manager doesn't know a #ThunarVfsVolume
 * for @info, %NULL will be returned.
 *
 * The returned #ThunarVfsVolume (if any) is owned by
 * @manager and must not be freed by the caller.
 *
 * Return value: the #ThunarVfsVolume, on which @info is
 *               located or %NULL.
 **/
ThunarVfsVolume*
thunar_vfs_volume_manager_get_volume_by_info (ThunarVfsVolumeManager *manager,
                                              const ThunarVfsInfo    *info)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER (manager), NULL);
  g_return_val_if_fail (info != NULL, NULL);
  return (*THUNAR_VFS_VOLUME_MANAGER_GET_CLASS (manager)->get_volume_by_info) (manager, info);
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
  return manager->volumes;
}



/**
 * thunar_vfs_volume_manager_add:
 * @manager : a #ThunarVfsVolumeManager instance.
 * @volume  : a #ThunarVfsVolume.
 *
 * Adds @volume to the list of #ThunarVfsVolume<!---->s managed
 * by the @manager, and emits the "volumes-added" signal on
 * @manager using the given @volume.
 *
 * It is an error to call this method with a @volume that is
 * already managed by @manager.
 *
 * This method should only be used by #ThunarVfsVolumeManager
 * implementations.
 **/
void
thunar_vfs_volume_manager_add (ThunarVfsVolumeManager *manager,
                               ThunarVfsVolume        *volume)
{
  GList list;

  g_return_if_fail (THUNAR_VFS_IS_VOLUME (volume));
  g_return_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER (manager));
  g_return_if_fail (g_list_find (manager->volumes, volume) == NULL);

  /* add the volume to the list of managed volumes */
  manager->volumes = g_list_prepend (manager->volumes, volume);
  g_object_ref (G_OBJECT (volume));

  /* connect the required signals */
  g_signal_connect_swapped (G_OBJECT (volume), "mounted", G_CALLBACK (thunar_vfs_volume_manager_volume_mounted), manager);
  g_signal_connect_swapped (G_OBJECT (volume), "pre-unmount", G_CALLBACK (thunar_vfs_volume_manager_volume_pre_unmount), manager);
  g_signal_connect_swapped (G_OBJECT (volume), "unmounted", G_CALLBACK (thunar_vfs_volume_manager_volume_unmounted), manager);

  /* fake a volume list */
  list.data = volume;
  list.next = NULL;
  list.prev = NULL;

  /* emit "volumes-added" with the faked list */
  g_signal_emit (G_OBJECT (manager), manager_signals[VOLUMES_ADDED], 0, &list);
}



/**
 * thunar_vfs_volume_manager_remove:
 * @manager : a #ThunarVfsVolumeManager instance.
 * @volume  : a #ThunarVfsVolume.
 *
 * Removes @volume from the list of #ThunarVfsVolume<!---->s managed
 * by the @manager, and emits the "volumes-removed" signal on
 * @manager using the given @volume.
 *
 * It is an error to call this method with @volume that is not
 * currently managed by @manager.
 *
 * This method should only be used by #ThunarVfsVolumeManager
 * implementations.
 **/
void
thunar_vfs_volume_manager_remove (ThunarVfsVolumeManager *manager,
                                  ThunarVfsVolume        *volume)
{
  GList list;

  g_return_if_fail (THUNAR_VFS_IS_VOLUME (volume));
  g_return_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER (manager));
  g_return_if_fail (g_list_find (manager->volumes, volume) != NULL);

  /* remove the volume from our list */
  manager->volumes = g_list_remove (manager->volumes, volume);

  /* disconnect our signal handlers */
  g_signal_handlers_disconnect_by_func (G_OBJECT (volume), thunar_vfs_volume_manager_volume_mounted, manager);
  g_signal_handlers_disconnect_by_func (G_OBJECT (volume), thunar_vfs_volume_manager_volume_pre_unmount, manager);
  g_signal_handlers_disconnect_by_func (G_OBJECT (volume), thunar_vfs_volume_manager_volume_unmounted, manager);

  /* fake a volume list */
  list.data = volume;
  list.next = NULL;
  list.prev = NULL;

  /* emit "volumes-removed" with the faked list */
  g_signal_emit (G_OBJECT (manager), manager_signals[VOLUMES_REMOVED], 0, &list);

  /* drop our reference on the volume */
  g_object_unref (G_OBJECT (volume));
}



#define __THUNAR_VFS_VOLUME_MANAGER_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>

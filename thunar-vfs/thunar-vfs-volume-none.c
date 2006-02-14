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

#include <thunar-vfs/thunar-vfs-volume-none.h>
#include <thunar-vfs/thunar-vfs-alias.h>



static void             thunar_vfs_volume_manager_none_class_init         (ThunarVfsVolumeManagerNoneClass *klass);
static void             thunar_vfs_volume_manager_none_manager_init       (ThunarVfsVolumeManagerIface     *iface);
static void             thunar_vfs_volume_manager_none_init               (ThunarVfsVolumeManagerNone      *manager_none);
static ThunarVfsVolume *thunar_vfs_volume_manager_none_get_volume_by_info (ThunarVfsVolumeManager          *manager,
                                                                           const ThunarVfsInfo             *info);
static GList           *thunar_vfs_volume_manager_none_get_volumes        (ThunarVfsVolumeManager          *manager);



struct _ThunarVfsVolumeManagerNoneClass
{
  GObjectClass __parent__;
};

struct _ThunarVfsVolumeManagerNone
{
  GObject __parent__;
};



G_DEFINE_TYPE_WITH_CODE (ThunarVfsVolumeManagerNone,
                         thunar_vfs_volume_manager_none,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (THUNAR_VFS_TYPE_VOLUME_MANAGER,
                                                thunar_vfs_volume_manager_none_manager_init));


static void
thunar_vfs_volume_manager_none_class_init (ThunarVfsVolumeManagerNoneClass *klass)
{
}



static void
thunar_vfs_volume_manager_none_manager_init (ThunarVfsVolumeManagerIface *iface)
{
  iface->get_volume_by_info = thunar_vfs_volume_manager_none_get_volume_by_info;
  iface->get_volumes = thunar_vfs_volume_manager_none_get_volumes;
}



static void
thunar_vfs_volume_manager_none_init (ThunarVfsVolumeManagerNone *manager_none)
{
}



static ThunarVfsVolume*
thunar_vfs_volume_manager_none_get_volume_by_info (ThunarVfsVolumeManager *manager,
                                                   const ThunarVfsInfo    *info)
{
  return NULL;
}



static GList*
thunar_vfs_volume_manager_none_get_volumes (ThunarVfsVolumeManager *manager)
{
  return NULL;
}





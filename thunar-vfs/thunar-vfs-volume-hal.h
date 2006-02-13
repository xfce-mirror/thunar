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

#ifndef __THUNAR_VFS_VOLUME_HAL_H__
#define __THUNAR_VFS_VOLUME_HAL_H__

#include <thunar-vfs/thunar-vfs-volume.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsVolumeHalClass ThunarVfsVolumeHalClass;
typedef struct _ThunarVfsVolumeHal      ThunarVfsVolumeHal;

#define THUNAR_VFS_TYPE_VOLUME_HAL            (thunar_vfs_volume_hal_get_type ())
#define THUNAR_VFS_VOLUME_HAL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_VOLUME_HAL, ThunarVfsVolumeHal))
#define THUNAR_VFS_VOLUME_HAL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_VOLUME_HAL, ThunarVfsVolumeHalClass))
#define THUNAR_VFS_IS_VOLUME_HAL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_VOLUME_HAL))
#define THUNAR_VFS_IS_VOLUME_HAL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_VOLUME_HAL))
#define THUNAR_VFS_VOLUME_HAL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_VOLUME_HAL, ThunarVfsVolumeHalClass))

GType thunar_vfs_volume_hal_get_type (void) G_GNUC_CONST G_GNUC_INTERNAL;


typedef struct _ThunarVfsVolumeManagerHalClass ThunarVfsVolumeManagerHalClass;
typedef struct _ThunarVfsVolumeManagerHal      ThunarVfsVolumeManagerHal;

#define THUNAR_VFS_TYPE_VOLUME_MANAGER_HAL            (thunar_vfs_volume_manager_hal_get_type ())
#define THUNAR_VFS_VOLUME_MANAGER_HAL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_VOLUME_MANAGER_HAL, ThunarVfsVolumeManagerHal))
#define THUNAR_VFS_VOLUME_MANAGER_HAL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_VOLUME_MANAGER_HAL, ThunarVfsVolumeManagerHalClass))
#define THUNAR_VFS_IS_VOLUME_MANAGER_HAL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_VOLUME_MANAGER_HAL))
#define THUNAR_VFS_IS_VOLUME_MANAGER_HAL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_VOLUME_MANAGER_HAL))
#define THUNAR_VFS_VOLUME_MANAGER_HAL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_VOLUME_MANAGER_HAL, ThunarVfsVolumeManagerHalClass))

GType thunar_vfs_volume_manager_hal_get_type (void) G_GNUC_CONST G_GNUC_INTERNAL;

G_END_DECLS;

#endif /* !__THUNAR_VFS_VOLUME_HAL_H__ */

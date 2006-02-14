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

#ifndef __THUNAR_VFS_VOLUME_NONE_H__
#define __THUNAR_VFS_VOLUME_NONE_H__

#include <thunar-vfs/thunar-vfs-volume.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsVolumeNoneClass ThunarVfsVolumeNoneClass;
typedef struct _ThunarVfsVolumeNone      ThunarVfsVolumeNone;

#define THUNAR_VFS_TYPE_VOLUME_NONE            (thunar_vfs_volume_none_get_type ())
#define THUNAR_VFS_VOLUME_NONE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_VOLUME_NONE, ThunarVfsVolumeNONE))
#define THUNAR_VFS_VOLUME_NONE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_VOLUME_NONE, ThunarVfsVolumeNONEClass))
#define THUNAR_VFS_IS_VOLUME_NONE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_VOLUME_NONE))
#define THUNAR_VFS_IS_VOLUME_NONE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_VOLUME_NONE))
#define THUNAR_VFS_VOLUME_NONE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_VOLUME_NONE, ThunarVfsVolumeNONEClass))

GType thunar_vfs_volume_none_get_type (void) G_GNUC_CONST;


typedef struct _ThunarVfsVolumeManagerNoneClass ThunarVfsVolumeManagerNoneClass;
typedef struct _ThunarVfsVolumeManagerNone      ThunarVfsVolumeManagerNone;

#define THUNAR_VFS_TYPE_VOLUME_MANAGER_NONE            (thunar_vfs_volume_manager_none_get_type ())
#define THUNAR_VFS_VOLUME_MANAGER_NONE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_VOLUME_MANAGER_NONE, ThunarVfsVolumeManagerNONE))
#define THUNAR_VFS_VOLUME_MANAGER_NONE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_VOLUME_MANAGER_NONE, ThunarVfsVolumeManagerNONEClass))
#define THUNAR_VFS_IS_VOLUME_MANAGER_NONE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_VOLUME_MANAGER_NONE))
#define THUNAR_VFS_IS_VOLUME_MANAGER_NONE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_VOLUME_MANAGER_NONE))
#define THUNAR_VFS_VOLUME_MANAGER_NONE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_VOLUME_MANAGER_NONE, ThunarVfsVolumeManagerNONEClass))

GType thunar_vfs_volume_manager_none_get_type (void) G_GNUC_CONST;

G_END_DECLS;

#endif /* !__THUNAR_VFS_VOLUME_NONE_H__ */

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

#ifndef __THUNAR_VFS_VOLUME_SYSV_H__
#define __THUNAR_VFS_VOLUME_SYSV_H__

#include <thunar-vfs/thunar-vfs-volume.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsVolumeSysVClass ThunarVfsVolumeSysVClass;
typedef struct _ThunarVfsVolumeSysV      ThunarVfsVolumeSysV;

#define THUNAR_VFS_TYPE_VOLUME_SYSV            (thunar_vfs_volume_sysv_get_type ())
#define THUNAR_VFS_VOLUME_SYSV(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_VOLUME_SYSV, ThunarVfsVolumeSYSV))
#define THUNAR_VFS_VOLUME_SYSV_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_VOLUME_SYSV, ThunarVfsVolumeSYSVClass))
#define THUNAR_VFS_IS_VOLUME_SYSV(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_VOLUME_SYSV))
#define THUNAR_VFS_IS_VOLUME_SYSV_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_VOLUME_SYSV))
#define THUNAR_VFS_VOLUME_SYSV_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_VOLUME_SYSV, ThunarVfsVolumeSYSVClass))

GType thunar_vfs_volume_sysv_get_type (void) G_GNUC_CONST;


typedef struct _ThunarVfsVolumeManagerSysVClass ThunarVfsVolumeManagerSysVClass;
typedef struct _ThunarVfsVolumeManagerSysV      ThunarVfsVolumeManagerSysV;

#define THUNAR_VFS_TYPE_VOLUME_MANAGER_SYSV            (thunar_vfs_volume_manager_sysv_get_type ())
#define THUNAR_VFS_VOLUME_MANAGER_SYSV(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_VOLUME_MANAGER_SYSV, ThunarVfsVolumeManagerSYSV))
#define THUNAR_VFS_VOLUME_MANAGER_SYSV_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_VOLUME_MANAGER_SYSV, ThunarVfsVolumeManagerSYSVClass))
#define THUNAR_VFS_IS_VOLUME_MANAGER_SYSV(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_VOLUME_MANAGER_SYSV))
#define THUNAR_VFS_IS_VOLUME_MANAGER_SYSV_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_VOLUME_MANAGER_SYSV))
#define THUNAR_VFS_VOLUME_MANAGER_SYSV_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_VOLUME_MANAGER_SYSV, ThunarVfsVolumeManagerSYSVClass))

GType thunar_vfs_volume_manager_sysv_get_type (void) G_GNUC_CONST;

G_END_DECLS;

#endif /* !__THUNAR_VFS_VOLUME_SYSV_H__ */

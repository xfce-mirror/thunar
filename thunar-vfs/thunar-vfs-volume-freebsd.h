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

#ifndef __THUNAR_VFS_VOLUME_FREEBSD_H__
#define __THUNAR_VFS_VOLUME_FREEBSD_H__

#include <thunar-vfs/thunar-vfs-volume.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsVolumeFreeBSDClass ThunarVfsVolumeFreeBSDClass;
typedef struct _ThunarVfsVolumeFreeBSD      ThunarVfsVolumeFreeBSD;

#define THUNAR_VFS_TYPE_VOLUME_FREEBSD            (thunar_vfs_volume_freebsd_get_type ())
#define THUNAR_VFS_VOLUME_FREEBSD(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_VOLUME_FREEBSD, ThunarVfsVolumeFreeBSD))
#define THUNAR_VFS_VOLUME_FREEBSD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_VOLUME_FREEBSD, ThunarVfsVolumeFreeBSDClass))
#define THUNAR_VFS_IS_VOLUME_FREEBSD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_VOLUME_FREEBSD))
#define THUNAR_VFS_IS_VOLUME_FREEBSD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_VOLUME_FREEBSD))
#define THUNAR_VFS_VOLUME_FREEBSD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_VOLUME_FREEBSD, ThunarVfsVolumeFreeBSDClass))

GType thunar_vfs_volume_freebsd_get_type (void) G_GNUC_CONST G_GNUC_INTERNAL;


typedef struct _ThunarVfsVolumeManagerFreeBSDClass ThunarVfsVolumeManagerFreeBSDClass;
typedef struct _ThunarVfsVolumeManagerFreeBSD      ThunarVfsVolumeManagerFreeBSD;

#define THUNAR_VFS_TYPE_VOLUME_MANAGER_FREEBSD            (thunar_vfs_volume_manager_freebsd_get_type ())
#define THUNAR_VFS_VOLUME_MANAGER_FREEBSD(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_VOLUME_MANAGER_FREEBSD, ThunarVfsVolumeManagerFreeBSD))
#define THUNAR_VFS_VOLUME_MANAGER_FREEBSD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_VOLUME_MANAGER_FREEBSD, ThunarVfsVolumeManagerFreeBSDClass))
#define THUNAR_VFS_IS_VOLUME_MANAGER_FREEBSD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_VOLUME_MANAGER_FREEBSD))
#define THUNAR_VFS_IS_VOLUME_MANAGER_FREEBSD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_VOLUME_MANAGER_FREEBSD))
#define THUNAR_VFS_VOLUME_MANAGER_FREEBSD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_VOLUME_MANAGER_FREEBSD, ThunarVfsVolumeManagerFreeBSDClass))

GType thunar_vfs_volume_manager_freebsd_get_type (void) G_GNUC_CONST G_GNUC_INTERNAL;

G_END_DECLS;

#endif /* !__THUNAR_VFS_VOLUME_FREEBSD_H__ */

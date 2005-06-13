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

#ifndef __THUNAR_VFS_VOLUME_H__
#define __THUNAR_VFS_VOLUME_H__

#include <gtk/gtk.h>

#include <thunar-vfs/thunar-vfs-uri.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsVolumeIface ThunarVfsVolumeIface;
typedef struct _ThunarVfsVolume      ThunarVfsVolume;

#define THUNAR_VFS_TYPE_VOLUME            (thunar_vfs_volume_get_type ())
#define THUNAR_VFS_VOLUME(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_VOLUME, ThunarVfsVolume))
#define THUNAR_VFS_IS_VOLUME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_VOLUME))
#define THUNAR_VFS_VOLUME_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), THUNAR_VFS_TYPE_VOLUME, ThunarVfsVolumeIface))

/**
 * ThunarVfsVolumeKind:
 * @THUNAR_VFS_VOLUME_KIND_UNKNOWN  : Unknown volume.
 * @THUNAR_VFS_VOLUME_KIND_CDROM    : CD/DVD drives.
 * @THUNAR_VFS_VOLUME_KIND_FLOPPY   : Floppy drives.
 * @THUNAR_VFS_VOLUME_KIND_HARDDISK : Hard disk drives.
 *
 * Describes the type of a VFS volume.
 **/
typedef enum
{
  THUNAR_VFS_VOLUME_KIND_UNKNOWN,
  THUNAR_VFS_VOLUME_KIND_CDROM,
  THUNAR_VFS_VOLUME_KIND_FLOPPY,
  THUNAR_VFS_VOLUME_KIND_HARDDISK,
} ThunarVfsVolumeKind;

/**
 * ThunarVfsVolumeStatus:
 * @THUNAR_VFS_VOLUME_STATUS_PRESENT : Whether or not a medium is present.
 * @THUNAR_VFS_VOLUME_STATUS_MOUNTED : Whether or not the media is currently mounted.
 *
 * Describes the current status of a VFS volume.
 **/
typedef enum /*< flags >*/
{
  THUNAR_VFS_VOLUME_STATUS_MOUNTED = 1 << 0,
  THUNAR_VFS_VOLUME_STATUS_PRESENT = 1 << 1,
} ThunarVfsVolumeStatus;

struct _ThunarVfsVolumeIface
{
  GTypeInterface __parent__;

  /* methods */
  ThunarVfsVolumeKind   (*get_kind)         (ThunarVfsVolume *volume);
  const gchar          *(*get_name)         (ThunarVfsVolume *volume);
  ThunarVfsVolumeStatus (*get_status)       (ThunarVfsVolume *volume);
  ThunarVfsURI         *(*get_mount_point)  (ThunarVfsVolume *volume);

  /* signals */
  void (*changed) (ThunarVfsVolume *volume);
};

GType                 thunar_vfs_volume_get_type          (void) G_GNUC_CONST;

ThunarVfsVolumeKind   thunar_vfs_volume_get_kind          (ThunarVfsVolume   *volume);
const gchar          *thunar_vfs_volume_get_name          (ThunarVfsVolume   *volume);
ThunarVfsVolumeStatus thunar_vfs_volume_get_status        (ThunarVfsVolume   *volume);
ThunarVfsURI         *thunar_vfs_volume_get_mount_point   (ThunarVfsVolume   *volume);

gboolean              thunar_vfs_volume_is_mounted        (ThunarVfsVolume   *volume);
gboolean              thunar_vfs_volume_is_present        (ThunarVfsVolume   *volume);
gboolean              thunar_vfs_volume_is_removable      (ThunarVfsVolume   *volume);

GtkIconInfo          *thunar_vfs_volume_lookup_icon       (ThunarVfsVolume   *volume,
                                                           GtkIconTheme      *icon_theme,
                                                           gint               size,
                                                           GtkIconLookupFlags flags);

void                  thunar_vfs_volume_changed           (ThunarVfsVolume   *volume);


typedef struct _ThunarVfsVolumeManagerIface ThunarVfsVolumeManagerIface;
typedef struct _ThunarVfsVolumeManager      ThunarVfsVolumeManager;

#define THUNAR_VFS_TYPE_VOLUME_MANAGER            (thunar_vfs_volume_manager_get_type ())
#define THUNAR_VFS_VOLUME_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_VOLUME_MANAGER, ThunarVfsVolumeManager))
#define THUNAR_VFS_IS_VOLUME_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_VOLUME_MANAGER))
#define THUNAR_VFS_VOLUME_MANAGER_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), THUNAR_VFS_TYPE_VOLUME_MANAGER, ThunarVfsVolumeManagerIface))

struct _ThunarVfsVolumeManagerIface
{
  GTypeInterface __parent__;

  /* methods */
  GList *(*get_volumes) (ThunarVfsVolumeManager *manager);

  /* signals */
  void (*volumes_added)   (ThunarVfsVolumeManager *manager,
                           GList                  *volumes);
  void (*volumes_removed) (ThunarVfsVolumeManager *manager,
                           GList                  *volumes);
};

GType                   thunar_vfs_volume_manager_get_type        (void) G_GNUC_CONST;

ThunarVfsVolumeManager *thunar_vfs_volume_manager_get_default     (void);

GList                  *thunar_vfs_volume_manager_get_volumes     (ThunarVfsVolumeManager *manager);

void                    thunar_vfs_volume_manager_volumes_added   (ThunarVfsVolumeManager *manager,
                                                                   GList                  *volumes);
void                    thunar_vfs_volume_manager_volumes_removed (ThunarVfsVolumeManager *manager,
                                                                   GList                  *volumes);

G_END_DECLS;

#endif /* !__THUNAR_VFS_VOLUME_H__ */

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

#ifndef __THUNAR_VFS_VOLUME_PRIVATE_H__
#define __THUNAR_VFS_VOLUME_PRIVATE_H__

#include <thunar-vfs/thunar-vfs-volume.h>

G_BEGIN_DECLS;

struct _ThunarVfsVolumeClass
{
  /*< private >*/
  GObjectClass __parent__;

  /*< public >*/

  /* methods */
  ThunarVfsVolumeKind   (*get_kind)         (ThunarVfsVolume   *volume);
  const gchar          *(*get_name)         (ThunarVfsVolume   *volume);
  ThunarVfsVolumeStatus (*get_status)       (ThunarVfsVolume   *volume);
  ThunarVfsPath        *(*get_mount_point)  (ThunarVfsVolume   *volume);
  const gchar          *(*lookup_icon_name) (ThunarVfsVolume   *volume,
                                             GtkIconTheme      *icon_theme);

  gboolean              (*eject)            (ThunarVfsVolume   *volume,
                                             GtkWidget         *window,
                                             GError           **error);
  gboolean              (*mount)            (ThunarVfsVolume   *volume,
                                             GtkWidget         *window,
                                             GError           **error);
  gboolean              (*unmount)          (ThunarVfsVolume   *volume,
                                             GtkWidget         *window,
                                             GError           **error);

  /* signals */
  void                  (*changed)          (ThunarVfsVolume   *volume);
  void                  (*mounted)          (ThunarVfsVolume   *volume);
  void                  (*pre_unmount)      (ThunarVfsVolume   *volume);
  void                  (*unmounted)        (ThunarVfsVolume   *volume);
};

struct _ThunarVfsVolume
{
  /*< private >*/
  GObject __parent__;
};

void thunar_vfs_volume_changed (ThunarVfsVolume *volume) G_GNUC_INTERNAL;


struct _ThunarVfsVolumeManagerClass
{
  /*< private >*/
  GObjectClass __parent__;

  /*< public >*/

  /* methods */
  ThunarVfsVolume *(*get_volume_by_info)  (ThunarVfsVolumeManager *manager,
                                           const ThunarVfsInfo    *info);

  /* signals */
  void             (*volumes_added)       (ThunarVfsVolumeManager *manager,
                                           GList                  *volumes);
  void             (*volumes_removed)     (ThunarVfsVolumeManager *manager,
                                           GList                  *volumes);
  void             (*volume_mounted)      (ThunarVfsVolumeManager *manager,
                                           ThunarVfsVolume        *volume);
  void             (*volume_pre_unmount)  (ThunarVfsVolumeManager *manager,
                                           ThunarVfsVolume        *volume);
  void             (*volume_unmounted)    (ThunarVfsVolumeManager *manager,
                                           ThunarVfsVolume        *volume);
};

struct _ThunarVfsVolumeManager
{
  /*< private >*/
  GObject __parent__;

  /*< public >*/

  /* the list of available volumes */
  GList  *volumes;
};

void thunar_vfs_volume_manager_add    (ThunarVfsVolumeManager *manager,
                                       ThunarVfsVolume        *volume) G_GNUC_INTERNAL;
void thunar_vfs_volume_manager_remove (ThunarVfsVolumeManager *manager,
                                       ThunarVfsVolume        *volume) G_GNUC_INTERNAL;

G_END_DECLS;

#endif /* !__THUNAR_VFS_VOLUME_PRIVATE_H__ */

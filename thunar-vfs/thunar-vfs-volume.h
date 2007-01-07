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

#ifndef __THUNAR_VFS_VOLUME_H__
#define __THUNAR_VFS_VOLUME_H__

#include <gtk/gtk.h>

#include <thunar-vfs/thunar-vfs-info.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsVolumeClass ThunarVfsVolumeClass;
typedef struct _ThunarVfsVolume      ThunarVfsVolume;

#define THUNAR_VFS_TYPE_VOLUME            (thunar_vfs_volume_get_type ())
#define THUNAR_VFS_VOLUME(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_VOLUME, ThunarVfsVolume))
#define THUNAR_VFS_VOLUME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_VOLUME, ThunarVfsVolumeClass))
#define THUNAR_VFS_IS_VOLUME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_VOLUME))
#define THUNAR_VFS_IS_VOLUME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_VOLUME))
#define THUNAR_VFS_VOLUME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_VOLUME, ThunarVfsVolumeClass))

/**
 * ThunarVfsVolumeKind:
 * @THUNAR_VFS_VOLUME_KIND_UNKNOWN        : Unknown volume.
 * @THUNAR_VFS_VOLUME_KIND_CDROM          : CD-ROMs.
 * @THUNAR_VFS_VOLUME_KIND_CDR            : CD-Rs.
 * @THUNAR_VFS_VOLUME_KIND_CDRW           : CD-RWs.
 * @THUNAR_VFS_VOLUME_KIND_DVDROM         : DVD-ROMs.
 * @THUNAR_VFS_VOLUME_KIND_DVDRAM         : DVD-RAMs.
 * @THUNAR_VFS_VOLUME_KIND_DVDR           : DVD-Rs.
 * @THUNAR_VFS_VOLUME_KIND_DVDRW          : DVD-RWs.
 * @THUNAR_VFS_VOLUME_KIND_DVDPLUSR       : DVD+Rs.
 * @THUNAR_VFS_VOLUME_KIND_DVDPLUSRW      : DVD+RWs.
 * @THUNAR_VFS_VOLUME_KIND_FLOPPY         : Floppy drives.
 * @THUNAR_VFS_VOLUME_KIND_HARDDISK       : Hard disk drives.
 * @THUNAR_VFS_VOLUME_KIND_USBSTICK       : USB sticks.
 * @THUNAR_VFS_VOLUME_KIND_AUDIO_PLAYER   : Portable audio players (i.e. iPod).
 * @THUNAR_VFS_VOLUME_KIND_AUDIO_CD       : Audio CDs.
 * @THUNAR_VFS_VOLUME_KIND_MEMORY_CARD    : Memory cards.
 * @THUNAR_VFS_VOLUME_KIND_REMOVABLE_DISK : Other removable disks.
 *
 * Describes the type of a VFS volume.
 **/
typedef enum
{
  THUNAR_VFS_VOLUME_KIND_UNKNOWN,
  THUNAR_VFS_VOLUME_KIND_CDROM,
  THUNAR_VFS_VOLUME_KIND_CDR,
  THUNAR_VFS_VOLUME_KIND_CDRW,
  THUNAR_VFS_VOLUME_KIND_DVDROM,
  THUNAR_VFS_VOLUME_KIND_DVDRAM,
  THUNAR_VFS_VOLUME_KIND_DVDR,
  THUNAR_VFS_VOLUME_KIND_DVDRW,
  THUNAR_VFS_VOLUME_KIND_DVDPLUSR,
  THUNAR_VFS_VOLUME_KIND_DVDPLUSRW,
  THUNAR_VFS_VOLUME_KIND_FLOPPY,
  THUNAR_VFS_VOLUME_KIND_HARDDISK,
  THUNAR_VFS_VOLUME_KIND_USBSTICK,
  THUNAR_VFS_VOLUME_KIND_AUDIO_PLAYER,
  THUNAR_VFS_VOLUME_KIND_AUDIO_CD,
  THUNAR_VFS_VOLUME_KIND_MEMORY_CARD,
  THUNAR_VFS_VOLUME_KIND_REMOVABLE_DISK,
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

GType                 thunar_vfs_volume_get_type          (void) G_GNUC_CONST;

ThunarVfsVolumeKind   thunar_vfs_volume_get_kind          (ThunarVfsVolume   *volume);
const gchar          *thunar_vfs_volume_get_name          (ThunarVfsVolume   *volume);
ThunarVfsVolumeStatus thunar_vfs_volume_get_status        (ThunarVfsVolume   *volume);
ThunarVfsPath        *thunar_vfs_volume_get_mount_point   (ThunarVfsVolume   *volume);

gboolean              thunar_vfs_volume_is_disc           (ThunarVfsVolume   *volume);
gboolean              thunar_vfs_volume_is_mounted        (ThunarVfsVolume   *volume);
gboolean              thunar_vfs_volume_is_present        (ThunarVfsVolume   *volume);
gboolean              thunar_vfs_volume_is_ejectable      (ThunarVfsVolume   *volume);
gboolean              thunar_vfs_volume_is_removable      (ThunarVfsVolume   *volume);

const gchar          *thunar_vfs_volume_lookup_icon_name  (ThunarVfsVolume   *volume,
                                                           GtkIconTheme      *icon_theme);

gboolean              thunar_vfs_volume_eject             (ThunarVfsVolume   *volume,
                                                           GtkWidget         *window,
                                                           GError           **error);
gboolean              thunar_vfs_volume_mount             (ThunarVfsVolume   *volume,
                                                           GtkWidget         *window,
                                                           GError           **error);
gboolean              thunar_vfs_volume_unmount           (ThunarVfsVolume   *volume,
                                                           GtkWidget         *window,
                                                           GError           **error);


typedef struct _ThunarVfsVolumeManagerClass ThunarVfsVolumeManagerClass;
typedef struct _ThunarVfsVolumeManager      ThunarVfsVolumeManager;

#define THUNAR_VFS_TYPE_VOLUME_MANAGER            (thunar_vfs_volume_manager_get_type ())
#define THUNAR_VFS_VOLUME_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_VOLUME_MANAGER, ThunarVfsVolumeManager))
#define THUNAR_VFS_VOLUME_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_VOLUME_MANAGER, ThunarVfsVolumeManagerClass))
#define THUNAR_VFS_IS_VOLUME_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_VOLUME_MANAGER))
#define THUNAR_VFS_IS_VOLUME_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_VOLUME_MANAGER))
#define THUNAR_VFS_VOLUME_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_VOLUME_MANAGER, ThunarVfsVolumeManagerClass))

GType                   thunar_vfs_volume_manager_get_type            (void) G_GNUC_CONST;

ThunarVfsVolumeManager *thunar_vfs_volume_manager_get_default         (void);

ThunarVfsVolume        *thunar_vfs_volume_manager_get_volume_by_info  (ThunarVfsVolumeManager *manager,
                                                                       const ThunarVfsInfo    *info);
GList                  *thunar_vfs_volume_manager_get_volumes         (ThunarVfsVolumeManager *manager);

G_END_DECLS;

#endif /* !__THUNAR_VFS_VOLUME_H__ */

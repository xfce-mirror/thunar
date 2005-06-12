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
 *
 * Based on ideas and code I wrote for xffm in 2003, as part of the
 * fstab plugin.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif

#ifdef HAVE_FSTAB_H
#include <fstab.h>
#endif

#include <exo/exo.h>

#include <thunar-vfs/thunar-vfs-volume-bsd.h>



enum
{
  PROP_0,
  PROP_KIND,
  PROP_MOUNT_POINT,
  PROP_MOUNT_STATUS,
};



static void                thunar_vfs_volume_bsd_class_init       (ThunarVfsVolumeBSDClass *klass);
static void                thunar_vfs_volume_bsd_volume_init      (ThunarVfsVolumeIface    *iface);
static void                thunar_vfs_volume_bsd_init             (ThunarVfsVolumeBSD      *volume_bsd);
static void                thunar_vfs_volume_bsd_finalize         (GObject                 *object);
static void                thunar_vfs_volume_bsd_get_property     (GObject                 *object, 
                                                                   guint                    prop_id,
                                                                   GValue                  *value,
                                                                   GParamSpec              *pspec);
static ThunarVfsVolumeKind thunar_vfs_volume_bsd_get_kind         (ThunarVfsVolume         *volume);
static ThunarVfsURI       *thunar_vfs_volume_bsd_get_mount_point  (ThunarVfsVolume         *volume);
static gboolean            thunar_vfs_volume_bsd_get_mount_status (ThunarVfsVolume         *volume);
static ThunarVfsVolumeBSD *thunar_vfs_volume_bsd_new              (const gchar             *device_path,
                                                                   const gchar             *mount_path);



struct _ThunarVfsVolumeBSDClass
{
  GObjectClass __parent__;
};

struct _ThunarVfsVolumeBSD
{
  GObject __parent__;

  gchar              *device_path;
  struct statfs       info;
  ThunarVfsVolumeKind kind;
  ThunarVfsURI       *mount_point;
  gboolean            mount_status;
};



G_DEFINE_TYPE_WITH_CODE (ThunarVfsVolumeBSD,
                         thunar_vfs_volume_bsd,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (THUNAR_VFS_TYPE_VOLUME,
                                                thunar_vfs_volume_bsd_volume_init));



static void
thunar_vfs_volume_bsd_class_init (ThunarVfsVolumeBSDClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_volume_bsd_finalize;
  gobject_class->get_property = thunar_vfs_volume_bsd_get_property;

  g_object_class_override_property (gobject_class,
                                    PROP_KIND,
                                    "kind");

  g_object_class_override_property (gobject_class,
                                    PROP_MOUNT_POINT,
                                    "mount-point");

  g_object_class_override_property (gobject_class,
                                    PROP_MOUNT_STATUS,
                                    "mount-status");
}



static void
thunar_vfs_volume_bsd_volume_init (ThunarVfsVolumeIface *iface)
{
  iface->get_kind = thunar_vfs_volume_bsd_get_kind;
  iface->get_mount_point = thunar_vfs_volume_bsd_get_mount_point;
  iface->get_mount_status = thunar_vfs_volume_bsd_get_mount_status;
}



static void
thunar_vfs_volume_bsd_init (ThunarVfsVolumeBSD *volume_bsd)
{
}



static void
thunar_vfs_volume_bsd_finalize (GObject *object)
{
  ThunarVfsVolumeBSD *volume_bsd = THUNAR_VFS_VOLUME_BSD (object);

  g_return_if_fail (THUNAR_VFS_IS_VOLUME_BSD (volume_bsd));

  if (G_LIKELY (volume_bsd->mount_point != NULL))
    g_object_unref (G_OBJECT (volume_bsd->mount_point));
  g_free (volume_bsd->device_path);

  G_OBJECT_CLASS (thunar_vfs_volume_bsd_parent_class)->finalize (object);
}



static void
thunar_vfs_volume_bsd_get_property (GObject    *object, 
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ThunarVfsVolume *volume = THUNAR_VFS_VOLUME (object);

  switch (prop_id)
    {
    case PROP_KIND:
      g_value_set_enum (value, thunar_vfs_volume_get_kind (volume));
      break;

    case PROP_MOUNT_POINT:
      g_value_set_object (value, thunar_vfs_volume_get_mount_point (volume));
      break;

    case PROP_MOUNT_STATUS:
      g_value_set_boolean (value, thunar_vfs_volume_get_mount_status (volume));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static ThunarVfsVolumeKind
thunar_vfs_volume_bsd_get_kind (ThunarVfsVolume *volume)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME_BSD (volume), THUNAR_VFS_VOLUME_KIND_UNKNOWN);
  return THUNAR_VFS_VOLUME_BSD (volume)->kind;
}



static ThunarVfsURI*
thunar_vfs_volume_bsd_get_mount_point (ThunarVfsVolume *volume)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME_BSD (volume), NULL);
  return THUNAR_VFS_VOLUME_BSD (volume)->mount_point;
}



static gboolean
thunar_vfs_volume_bsd_get_mount_status (ThunarVfsVolume *volume)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME_BSD (volume), FALSE);
  return THUNAR_VFS_VOLUME_BSD (volume)->mount_status;
}



static ThunarVfsVolumeBSD*
thunar_vfs_volume_bsd_new (const gchar *device_path,
                           const gchar *mount_path)
{
  ThunarVfsVolumeBSD *volume_bsd;

  g_return_val_if_fail (device_path != NULL, NULL);
  g_return_val_if_fail (mount_path != NULL, NULL);

  /* allocate the volume object */
  volume_bsd = g_object_new (THUNAR_VFS_TYPE_VOLUME_BSD, NULL);
  volume_bsd->device_path = g_strdup (device_path);
  volume_bsd->kind = THUNAR_VFS_VOLUME_KIND_UNKNOWN;
  volume_bsd->mount_point = thunar_vfs_uri_new_for_path (mount_path);

  /* query the filesystem information for the mount point */
  if (statfs (mount_path, &volume_bsd->info) < 0)
    {
      g_object_unref (G_OBJECT (volume_bsd));
      return NULL;
    }

  /* check if the volume is currently mounted */
  volume_bsd->mount_status = exo_str_is_equal (volume_bsd->info.f_mntfromname,
                                               volume_bsd->device_path);

  return volume_bsd;
}




static void   thunar_vfs_volume_manager_bsd_class_init    (ThunarVfsVolumeManagerBSDClass *klass);
static void   thunar_vfs_volume_manager_bsd_manager_init  (ThunarVfsVolumeManagerIface    *iface);
static void   thunar_vfs_volume_manager_bsd_init          (ThunarVfsVolumeManagerBSD      *manager_bsd);
static void   thunar_vfs_volume_manager_bsd_finalize      (GObject                        *object);
static GList *thunar_vfs_volume_manager_bsd_get_volumes   (ThunarVfsVolumeManager         *manager);



struct _ThunarVfsVolumeManagerBSDClass
{
  GObjectClass __parent__;
};

struct _ThunarVfsVolumeManagerBSD
{
  GObject __parent__;
  GList  *volumes;
};



G_DEFINE_TYPE_WITH_CODE (ThunarVfsVolumeManagerBSD,
                         thunar_vfs_volume_manager_bsd,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (THUNAR_VFS_TYPE_VOLUME_MANAGER,
                                                thunar_vfs_volume_manager_bsd_manager_init));



static void
thunar_vfs_volume_manager_bsd_class_init (ThunarVfsVolumeManagerBSDClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_volume_manager_bsd_finalize;
}



static void
thunar_vfs_volume_manager_bsd_manager_init (ThunarVfsVolumeManagerIface *iface)
{
  iface->get_volumes = thunar_vfs_volume_manager_bsd_get_volumes;
}



static void
thunar_vfs_volume_manager_bsd_init (ThunarVfsVolumeManagerBSD *manager_bsd)
{
  ThunarVfsVolumeBSD *volume_bsd;
  struct fstab       *fs;

  /* load the fstab database */
  setfsent ();

  /* process all fstab entries and generate volume objects */
  for (;;)
    {
      /* query the next fstab entry */
      fs = getfsent ();
      if (G_UNLIKELY (fs == NULL))
        break;

      /* we only care for file systems */
      if (!exo_str_is_equal (fs->fs_type, FSTAB_RW)
          && !exo_str_is_equal (fs->fs_type, FSTAB_RQ)
          && !exo_str_is_equal (fs->fs_type, FSTAB_RO))
        continue;

      volume_bsd = thunar_vfs_volume_bsd_new (fs->fs_spec, fs->fs_file);
      if (G_LIKELY (volume_bsd != NULL))
        manager_bsd->volumes = g_list_append (manager_bsd->volumes, volume_bsd);
    }

  /* unload the fstab database */
  endfsent ();
}



static void
thunar_vfs_volume_manager_bsd_finalize (GObject *object)
{
  ThunarVfsVolumeManagerBSD *manager_bsd = THUNAR_VFS_VOLUME_MANAGER_BSD (object);
  GList                     *lp;

  g_return_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER (manager_bsd));

  for (lp = manager_bsd->volumes; lp != NULL; lp = lp->next)
    g_object_unref (G_OBJECT (lp->data));
  g_list_free (manager_bsd->volumes);

  G_OBJECT_CLASS (thunar_vfs_volume_manager_bsd_parent_class)->finalize (object);
}



static GList*
thunar_vfs_volume_manager_bsd_get_volumes (ThunarVfsVolumeManager *manager)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER_BSD (manager), NULL);
  return THUNAR_VFS_VOLUME_MANAGER_BSD (manager)->volumes;
}



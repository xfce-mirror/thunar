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

#ifdef HAVE_SYS_CDIO_H
#include <sys/cdio.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_FSTAB_H
#include <fstab.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-exec.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-volume-freebsd.h>
#include <thunar-vfs/thunar-vfs-volume-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>



static void                     thunar_vfs_volume_freebsd_class_init       (ThunarVfsVolumeFreeBSDClass *klass);
static void                     thunar_vfs_volume_freebsd_finalize         (GObject                     *object);
static ThunarVfsVolumeKind      thunar_vfs_volume_freebsd_get_kind         (ThunarVfsVolume             *volume);
static const gchar             *thunar_vfs_volume_freebsd_get_name         (ThunarVfsVolume             *volume);
static ThunarVfsVolumeStatus    thunar_vfs_volume_freebsd_get_status       (ThunarVfsVolume             *volume);
static ThunarVfsPath           *thunar_vfs_volume_freebsd_get_mount_point  (ThunarVfsVolume             *volume);
static gboolean                 thunar_vfs_volume_freebsd_eject            (ThunarVfsVolume             *volume,
                                                                            GtkWidget                   *window,
                                                                            GError                     **error);
static gboolean                 thunar_vfs_volume_freebsd_mount            (ThunarVfsVolume             *volume,
                                                                            GtkWidget                   *window,
                                                                            GError                     **error);
static gboolean                 thunar_vfs_volume_freebsd_unmount          (ThunarVfsVolume             *volume,
                                                                            GtkWidget                   *window,
                                                                            GError                     **error);
static gboolean                 thunar_vfs_volume_freebsd_update           (gpointer                     user_data);
static ThunarVfsVolumeFreeBSD  *thunar_vfs_volume_freebsd_new              (const gchar                 *device_path,
                                                                            const gchar                 *mount_path);



struct _ThunarVfsVolumeFreeBSDClass
{
  ThunarVfsVolumeClass __parent__;
};

struct _ThunarVfsVolumeFreeBSD
{
  ThunarVfsVolume       __parent__;

  gchar                *device_path;
  const gchar          *device_name;
  ThunarVfsFileDevice   device_id;

  gchar                *label;

  struct statfs         info;
  ThunarVfsPath        *mount_point;

  ThunarVfsVolumeKind   kind;
  ThunarVfsVolumeStatus status;

  gint                  update_timer_id;
};



static GObjectClass *thunar_vfs_volume_freebsd_parent_class;



GType
thunar_vfs_volume_freebsd_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = _thunar_vfs_g_type_register_simple (THUNAR_VFS_TYPE_VOLUME,
                                                 "ThunarVfsVolumeFreeBSD",
                                                 sizeof (ThunarVfsVolumeFreeBSDClass),
                                                 thunar_vfs_volume_freebsd_class_init,
                                                 sizeof (ThunarVfsVolumeFreeBSD),
                                                 NULL,
                                                 0);
    }

  return type;
}



static void
thunar_vfs_volume_freebsd_class_init (ThunarVfsVolumeFreeBSDClass *klass)
{
  ThunarVfsVolumeClass *thunarvfs_volume_class;
  GObjectClass         *gobject_class;

  /* determine the parent type class */
  thunar_vfs_volume_freebsd_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_volume_freebsd_finalize;

  thunarvfs_volume_class = THUNAR_VFS_VOLUME_CLASS (klass);
  thunarvfs_volume_class->get_kind = thunar_vfs_volume_freebsd_get_kind;
  thunarvfs_volume_class->get_name = thunar_vfs_volume_freebsd_get_name;
  thunarvfs_volume_class->get_status = thunar_vfs_volume_freebsd_get_status;
  thunarvfs_volume_class->get_mount_point = thunar_vfs_volume_freebsd_get_mount_point;
  thunarvfs_volume_class->eject = thunar_vfs_volume_freebsd_eject;
  thunarvfs_volume_class->mount = thunar_vfs_volume_freebsd_mount;
  thunarvfs_volume_class->unmount = thunar_vfs_volume_freebsd_unmount;
}



static void
thunar_vfs_volume_freebsd_finalize (GObject *object)
{
  ThunarVfsVolumeFreeBSD *volume_freebsd = THUNAR_VFS_VOLUME_FREEBSD (object);

  if (G_LIKELY (volume_freebsd->update_timer_id > 0))
    g_source_remove (volume_freebsd->update_timer_id);

  if (G_LIKELY (volume_freebsd->mount_point != NULL))
    thunar_vfs_path_unref (volume_freebsd->mount_point);

  g_free (volume_freebsd->device_path);
  g_free (volume_freebsd->label);

  (*G_OBJECT_CLASS (thunar_vfs_volume_freebsd_parent_class)->finalize) (object);
}



static ThunarVfsVolumeKind
thunar_vfs_volume_freebsd_get_kind (ThunarVfsVolume *volume)
{
  return THUNAR_VFS_VOLUME_FREEBSD (volume)->kind;
}



static const gchar*
thunar_vfs_volume_freebsd_get_name (ThunarVfsVolume *volume)
{
  ThunarVfsVolumeFreeBSD *volume_freebsd = THUNAR_VFS_VOLUME_FREEBSD (volume);
  return (volume_freebsd->label != NULL) ? volume_freebsd->label : volume_freebsd->device_name;
}



static ThunarVfsVolumeStatus
thunar_vfs_volume_freebsd_get_status (ThunarVfsVolume *volume)
{
  return THUNAR_VFS_VOLUME_FREEBSD (volume)->status;
}



static ThunarVfsPath*
thunar_vfs_volume_freebsd_get_mount_point (ThunarVfsVolume *volume)
{
  return THUNAR_VFS_VOLUME_FREEBSD (volume)->mount_point;
}



static gboolean
thunar_vfs_volume_freebsd_eject (ThunarVfsVolume *volume,
                                 GtkWidget       *window,
                                 GError         **error)
{
  ThunarVfsVolumeFreeBSD *volume_freebsd = THUNAR_VFS_VOLUME_FREEBSD (volume);
  gboolean                result;
  gchar                  *quoted;

  /* execute the eject command */
  quoted = g_shell_quote (volume_freebsd->device_path);
  result = thunar_vfs_exec_sync ("eject %s", error, quoted);
  g_free (quoted);

  /* update volume state if successfull */
  if (G_LIKELY (result))
    thunar_vfs_volume_freebsd_update (volume_freebsd);

  return result;
}



static gboolean
thunar_vfs_volume_freebsd_mount (ThunarVfsVolume *volume,
                                 GtkWidget       *window,
                                 GError         **error)
{
  ThunarVfsVolumeFreeBSD *volume_freebsd = THUNAR_VFS_VOLUME_FREEBSD (volume);
  gboolean                result;
  gchar                   mount_point[THUNAR_VFS_PATH_MAXSTRLEN];
  gchar                  *quoted;

  /* determine the absolute path to the mount point */
  if (thunar_vfs_path_to_string (volume_freebsd->mount_point, mount_point, sizeof (mount_point), error) < 0)
    return FALSE;

  /* execute the mount command */
  quoted = g_shell_quote (mount_point);
  result = thunar_vfs_exec_sync ("mount %s", error, quoted);
  g_free (quoted);

  /* update volume state if successfull */
  if (G_LIKELY (result))
    thunar_vfs_volume_freebsd_update (volume_freebsd);

  return result;
}



static gboolean
thunar_vfs_volume_freebsd_unmount (ThunarVfsVolume *volume,
                                   GtkWidget       *window,
                                   GError         **error)
{
  ThunarVfsVolumeFreeBSD *volume_freebsd = THUNAR_VFS_VOLUME_FREEBSD (volume);
  gboolean                result;
  gchar                   mount_point[THUNAR_VFS_PATH_MAXSTRLEN];
  gchar                  *quoted;

  /* determine the absolute path to the mount point */
  if (thunar_vfs_path_to_string (volume_freebsd->mount_point, mount_point, sizeof (mount_point), error) < 0)
    return FALSE;

  /* execute the umount command */
  quoted = g_shell_quote (mount_point);
  result = thunar_vfs_exec_sync ("umount %s", error, quoted);
  g_free (quoted);

  /* update volume state if successfull */
  if (G_LIKELY (result))
    thunar_vfs_volume_freebsd_update (volume_freebsd);

  return result;
}



static gboolean
thunar_vfs_volume_freebsd_update (gpointer user_data)
{
  ThunarVfsVolumeFreeBSD *volume_freebsd = THUNAR_VFS_VOLUME_FREEBSD (user_data);
  ThunarVfsVolumeStatus   status = 0;
  struct ioc_toc_header   ith;
  struct stat             sb;
  gchar                  *label;
  gchar                   buffer[2048];
  int                     fd;

  if (volume_freebsd->kind == THUNAR_VFS_VOLUME_KIND_CDROM)
    {
      /* try to read the table of contents from the CD-ROM,
       * which will only succeed if a disc is present for
       * the drive.
       */
      fd = open (volume_freebsd->device_path, O_RDONLY);
      if (fd >= 0)
        {
          if (ioctl (fd, CDIOREADTOCHEADER, &ith) >= 0)
            {
              status |= THUNAR_VFS_VOLUME_STATUS_PRESENT;

              /* read the label of the disc */
              if (volume_freebsd->label == NULL && (volume_freebsd->status & THUNAR_VFS_VOLUME_STATUS_PRESENT) == 0)
                {
                  /* skip to sector 16 and read it */
                  if (lseek (fd, 16 * 2048, SEEK_SET) >= 0 && read (fd, buffer, sizeof (buffer)) >= 0)
                    {
                      /* offset 40 contains the volume identifier */
                      label = buffer + 40;
                      label[32] = '\0';
                      g_strchomp (label);
                      if (G_LIKELY (*label != '\0'))
                        volume_freebsd->label = g_strdup (label);
                    }
                }
            }

          close (fd);
        }
    }

  /* determine the absolute path to the mount point */
  if (thunar_vfs_path_to_string (volume_freebsd->mount_point, buffer, sizeof (buffer), NULL) > 0)
    {
      /* query the file system information for the mount point */
      if (statfs (buffer, &volume_freebsd->info) >= 0)
        {
          /* if the device is mounted, it means that a medium is present */
          if (exo_str_is_equal (volume_freebsd->info.f_mntfromname, volume_freebsd->device_path))
            status |= THUNAR_VFS_VOLUME_STATUS_MOUNTED | THUNAR_VFS_VOLUME_STATUS_PRESENT;
        }

      /* free the volume label if no disc is present */
      if ((status & THUNAR_VFS_VOLUME_STATUS_PRESENT) == 0)
        {
          g_free (volume_freebsd->label);
          volume_freebsd->label = NULL;
        }

      /* determine the device id if mounted */
      if ((status & THUNAR_VFS_VOLUME_STATUS_MOUNTED) != 0)
        {
          if (stat (buffer, &sb) < 0)
            volume_freebsd->device_id = (ThunarVfsFileDevice) -1;
          else
            volume_freebsd->device_id = sb.st_dev;
        }
    }

  /* update the status if necessary */
  if (status != volume_freebsd->status)
    {
      volume_freebsd->status = status;
      thunar_vfs_volume_changed (THUNAR_VFS_VOLUME (volume_freebsd));
    }

  return TRUE;
}



static ThunarVfsVolumeFreeBSD*
thunar_vfs_volume_freebsd_new (const gchar *device_path,
                               const gchar *mount_path)
{
  ThunarVfsVolumeFreeBSD *volume_freebsd;
  const gchar            *p;

  g_return_val_if_fail (device_path != NULL, NULL);
  g_return_val_if_fail (mount_path != NULL, NULL);

  /* allocate the volume object */
  volume_freebsd = g_object_new (THUNAR_VFS_TYPE_VOLUME_FREEBSD, NULL);
  volume_freebsd->device_path = g_strdup (device_path);
  volume_freebsd->mount_point = thunar_vfs_path_new (mount_path, NULL);

  /* determine the device name */
  for (p = volume_freebsd->device_name = volume_freebsd->device_path; *p != '\0'; ++p)
    if (p[0] == '/' && (p[1] != '/' && p[1] != '\0'))
      volume_freebsd->device_name = p + 1;

  /* determine the kind of the volume */
  p = volume_freebsd->device_name;
  if (p[0] == 'c' && p[1] == 'd' && g_ascii_isdigit (p[2]))
    volume_freebsd->kind = THUNAR_VFS_VOLUME_KIND_CDROM;
  else if (p[0] == 'f' && p[1] == 'd' && g_ascii_isdigit (p[2]))
    volume_freebsd->kind = THUNAR_VFS_VOLUME_KIND_FLOPPY;
  else if ((p[0] == 'a' && p[1] == 'd' && g_ascii_isdigit (p[2]))
        || (p[0] == 'd' && p[1] == 'a' && g_ascii_isdigit (p[2])))
    volume_freebsd->kind = THUNAR_VFS_VOLUME_KIND_HARDDISK;
  else
    volume_freebsd->kind = THUNAR_VFS_VOLUME_KIND_UNKNOWN;

  /* determine up-to-date status */
  thunar_vfs_volume_freebsd_update (volume_freebsd);

  /* start the update timer */
  volume_freebsd->update_timer_id = g_timeout_add (1000, thunar_vfs_volume_freebsd_update, volume_freebsd);

  return volume_freebsd;
}




static void             thunar_vfs_volume_manager_freebsd_class_init         (ThunarVfsVolumeManagerFreeBSDClass *klass);
static void             thunar_vfs_volume_manager_freebsd_init               (ThunarVfsVolumeManagerFreeBSD      *manager_freebsd);
static ThunarVfsVolume *thunar_vfs_volume_manager_freebsd_get_volume_by_info (ThunarVfsVolumeManager             *manager,
                                                                              const ThunarVfsInfo                *info);



struct _ThunarVfsVolumeManagerFreeBSDClass
{
  ThunarVfsVolumeManagerClass __parent__;
};

struct _ThunarVfsVolumeManagerFreeBSD
{
  ThunarVfsVolumeManager __parent__;
};



GType
thunar_vfs_volume_manager_freebsd_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = _thunar_vfs_g_type_register_simple (THUNAR_VFS_TYPE_VOLUME_MANAGER,
                                                 "ThunarVfsVolumeManagerFreeBSD",
                                                 sizeof (ThunarVfsVolumeManagerFreeBSDClass),
                                                 thunar_vfs_volume_manager_freebsd_class_init,
                                                 sizeof (ThunarVfsVolumeManagerFreeBSD),
                                                 thunar_vfs_volume_manager_freebsd_init,
                                                 0);
    }

  return type;
}



static void
thunar_vfs_volume_manager_freebsd_class_init (ThunarVfsVolumeManagerFreeBSDClass *klass)
{
  ThunarVfsVolumeManagerClass *thunarvfs_volume_manager_class;

  thunarvfs_volume_manager_class = THUNAR_VFS_VOLUME_MANAGER_CLASS (klass);
  thunarvfs_volume_manager_class->get_volume_by_info = thunar_vfs_volume_manager_freebsd_get_volume_by_info;
}



static void
thunar_vfs_volume_manager_freebsd_init (ThunarVfsVolumeManagerFreeBSD *manager_freebsd)
{
  ThunarVfsVolumeFreeBSD *volume_freebsd;
  struct fstab           *fs;

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

      volume_freebsd = thunar_vfs_volume_freebsd_new (fs->fs_spec, fs->fs_file);
      if (G_LIKELY (volume_freebsd != NULL))
        {
          thunar_vfs_volume_manager_add (THUNAR_VFS_VOLUME_MANAGER (manager_freebsd), THUNAR_VFS_VOLUME (volume_freebsd));
          g_object_unref (G_OBJECT (volume_freebsd));
        }
    }

  /* unload the fstab database */
  endfsent ();
}



static ThunarVfsVolume*
thunar_vfs_volume_manager_freebsd_get_volume_by_info (ThunarVfsVolumeManager *manager,
                                                      const ThunarVfsInfo    *info)
{
  ThunarVfsVolumeFreeBSD *volume_freebsd = NULL;
  GList                  *lp;

  for (lp = manager->volumes; lp != NULL; lp = lp->next)
    {
      volume_freebsd = THUNAR_VFS_VOLUME_FREEBSD (lp->data);
      if ((volume_freebsd->status & THUNAR_VFS_VOLUME_STATUS_MOUNTED) != 0 && volume_freebsd->device_id == info->device)
        return THUNAR_VFS_VOLUME (volume_freebsd);
    }

  return NULL;
}



#define __THUNAR_VFS_VOLUME_FREEBSD_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>

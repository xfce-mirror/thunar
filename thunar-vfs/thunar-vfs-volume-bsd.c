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

#include <exo/exo.h>

#include <thunar-vfs/thunar-vfs-volume-bsd.h>
#include <thunar-vfs/thunar-vfs-alias.h>



static void                  thunar_vfs_volume_bsd_class_init       (ThunarVfsVolumeBSDClass *klass);
static void                  thunar_vfs_volume_bsd_volume_init      (ThunarVfsVolumeIface    *iface);
static void                  thunar_vfs_volume_bsd_init             (ThunarVfsVolumeBSD      *volume_bsd);
static void                  thunar_vfs_volume_bsd_finalize         (GObject                 *object);
static ThunarVfsVolumeKind   thunar_vfs_volume_bsd_get_kind         (ThunarVfsVolume         *volume);
static const gchar          *thunar_vfs_volume_bsd_get_name         (ThunarVfsVolume         *volume);
static ThunarVfsVolumeStatus thunar_vfs_volume_bsd_get_status       (ThunarVfsVolume         *volume);
static ThunarVfsPath        *thunar_vfs_volume_bsd_get_mount_point  (ThunarVfsVolume         *volume);
static gboolean              thunar_vfs_volume_bsd_get_free_space   (ThunarVfsVolume         *volume,
                                                                     ThunarVfsFileSize       *free_space_return);
static gboolean              thunar_vfs_volume_bsd_eject            (ThunarVfsVolume         *volume,
                                                                     GtkWidget               *window,
                                                                     GError                 **error);
static gboolean              thunar_vfs_volume_bsd_mount            (ThunarVfsVolume         *volume,
                                                                     GtkWidget               *window,
                                                                     GError                 **error);
static gboolean              thunar_vfs_volume_bsd_unmount          (ThunarVfsVolume         *volume,
                                                                     GtkWidget               *window,
                                                                     GError                 **error);
static gboolean              thunar_vfs_volume_bsd_update           (gpointer                 user_data);
static ThunarVfsVolumeBSD   *thunar_vfs_volume_bsd_new              (const gchar             *device_path,
                                                                     const gchar             *mount_path);



struct _ThunarVfsVolumeBSDClass
{
  GObjectClass __parent__;
};

struct _ThunarVfsVolumeBSD
{
  GObject __parent__;

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
}



static void
thunar_vfs_volume_bsd_volume_init (ThunarVfsVolumeIface *iface)
{
  iface->get_kind = thunar_vfs_volume_bsd_get_kind;
  iface->get_name = thunar_vfs_volume_bsd_get_name;
  iface->get_status = thunar_vfs_volume_bsd_get_status;
  iface->get_mount_point = thunar_vfs_volume_bsd_get_mount_point;
  iface->get_free_space = thunar_vfs_volume_bsd_get_free_space;
  iface->eject = thunar_vfs_volume_bsd_eject;
  iface->mount = thunar_vfs_volume_bsd_mount;
  iface->unmount = thunar_vfs_volume_bsd_unmount;
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

  if (G_LIKELY (volume_bsd->update_timer_id >= 0))
    g_source_remove (volume_bsd->update_timer_id);

  if (G_LIKELY (volume_bsd->mount_point != NULL))
    thunar_vfs_path_unref (volume_bsd->mount_point);

  g_free (volume_bsd->device_path);
  g_free (volume_bsd->label);

  G_OBJECT_CLASS (thunar_vfs_volume_bsd_parent_class)->finalize (object);
}



static ThunarVfsVolumeKind
thunar_vfs_volume_bsd_get_kind (ThunarVfsVolume *volume)
{
  return THUNAR_VFS_VOLUME_BSD (volume)->kind;
}



static const gchar*
thunar_vfs_volume_bsd_get_name (ThunarVfsVolume *volume)
{
  ThunarVfsVolumeBSD *volume_bsd = THUNAR_VFS_VOLUME_BSD (volume);

  return (volume_bsd->label != NULL) ? volume_bsd->label : volume_bsd->device_name;
}



static ThunarVfsVolumeStatus
thunar_vfs_volume_bsd_get_status (ThunarVfsVolume *volume)
{
  return THUNAR_VFS_VOLUME_BSD (volume)->status;
}



static ThunarVfsPath*
thunar_vfs_volume_bsd_get_mount_point (ThunarVfsVolume *volume)
{
  return THUNAR_VFS_VOLUME_BSD (volume)->mount_point;
}



static gboolean
thunar_vfs_volume_bsd_get_free_space (ThunarVfsVolume   *volume,
                                      ThunarVfsFileSize *free_space_return)
{
  ThunarVfsVolumeBSD *volume_bsd = THUNAR_VFS_VOLUME_BSD (volume);
  *free_space_return = volume_bsd->info.f_bavail * volume_bsd->info.f_bsize;
  return TRUE;
}



static gboolean
thunar_vfs_volume_bsd_eject (ThunarVfsVolume *volume,
                             GtkWidget       *window,
                             GError         **error)
{
  ThunarVfsVolumeBSD *volume_bsd = THUNAR_VFS_VOLUME_BSD (volume);
  gboolean            result;
  gchar              *standard_error;
  gchar              *command_line;
  gchar              *quoted;
  gint                exit_status;

  /* generate the command line for the eject command */
  quoted = g_shell_quote (volume_bsd->device_path);
  command_line = g_strconcat ("eject ", quoted, NULL);
  g_free (quoted);

  /* execute the eject command */
  result = g_spawn_command_line_sync (command_line, NULL, &standard_error, &exit_status, error);
  if (G_LIKELY (result))
    {
      /* check if the command failed */
      if (G_UNLIKELY (exit_status != 0))
        {
          /* drop additional whitespace from the stderr output */
          g_strstrip (standard_error);

          /* generate an error from the standard_error content */
          if (G_LIKELY (g_str_has_prefix (standard_error, "eject: ")))
            {
              /* strip off the "eject: " prefix */
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s", standard_error + 7);
            }
          else
            {
              /* use the error message as-is, not nice, but hey, better than nothing */
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s", standard_error);
            }

          /* and yes, we failed */
          result = FALSE;
        }

      /* release the stderr output */
      g_free (standard_error);
    }

  /* cleanup */
  g_free (command_line);

  /* update volume state if successfull */
  if (G_LIKELY (result))
    thunar_vfs_volume_bsd_update (volume_bsd);

  return result;
}



static gboolean
thunar_vfs_volume_bsd_mount (ThunarVfsVolume *volume,
                             GtkWidget       *window,
                             GError         **error)
{
  ThunarVfsVolumeBSD *volume_bsd = THUNAR_VFS_VOLUME_BSD (volume);
  gboolean            result;
  GString            *message;
  gchar               mount_point[THUNAR_VFS_PATH_MAXSTRLEN];
  gchar             **standard_error_parts;
  gchar              *standard_error;
  gchar              *command_line;
  gchar              *quoted;
  gint                exit_status;
  gint                n_parts;
  gint                i;

  /* determine the absolute path to the mount point */
  if (thunar_vfs_path_to_string (volume_bsd->mount_point, mount_point, sizeof (mount_point), error) < 0)
    return FALSE;

  /* generate the command line for the mount command */
  quoted = g_shell_quote (mount_point);
  command_line = g_strconcat ("mount ", quoted, NULL);
  g_free (quoted);

  /* execute the mount command */
  result = g_spawn_command_line_sync (command_line, NULL, &standard_error, &exit_status, error);
  if (G_LIKELY (result))
    {
      /* check if the command failed */
      if (G_UNLIKELY (exit_status != 0))
        {
          /* drop additional whitespace from the stderr output */
          g_strstrip (standard_error);

          /* the error message will usually look like
           * "cd9660: /dev/cd1: Operation not permitted",
           * so let's apply some voodoo magic here to
           * make it look better.
           */
          standard_error_parts = g_strsplit (standard_error, ":", -1);
          if (G_UNLIKELY (standard_error_parts[0] == NULL))
            {
              /* no useful information, *narf* */
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("Unknown error"));
            }
          else
            {
              /* determine the last part of the error */
              for (n_parts = 0; standard_error_parts[n_parts + 1] != NULL; ++n_parts)
                ;

              /* construct a new error message */
              message = g_string_new (g_strstrip (standard_error_parts[n_parts]));

              /* append any additional information in () */
              if (G_LIKELY (n_parts > 0))
                {
                  g_string_append (message, " (");
                  for (i = 0; i < n_parts; ++i)
                    {
                      if (G_UNLIKELY (i > 0))
                        g_string_append (message, ", ");
                      g_string_append (message, g_strstrip (standard_error_parts[i]));
                    }
                  g_string_append (message, ")");
                }

              /* use the generated message for the error */
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s", message->str);
              g_string_free (message, TRUE);
            }

          /* release the standard error parts */
          g_strfreev (standard_error_parts);

          /* and yes, we failed */
          result = FALSE;
        }

      /* release the stderr output */
      g_free (standard_error);
    }

  /* cleanup */
  g_free (command_line);

  /* update volume state if successfull */
  if (G_LIKELY (result))
    thunar_vfs_volume_bsd_update (volume_bsd);

  return result;
}



static gboolean
thunar_vfs_volume_bsd_unmount (ThunarVfsVolume *volume,
                               GtkWidget       *window,
                               GError         **error)
{
  ThunarVfsVolumeBSD *volume_bsd = THUNAR_VFS_VOLUME_BSD (volume);
  gboolean            result;
  gchar               mount_point[THUNAR_VFS_PATH_MAXSTRLEN];
  gchar              *standard_error;
  gchar              *command_line;
  gchar              *quoted;
  gint                exit_status;

  /* determine the absolute path to the mount point */
  if (thunar_vfs_path_to_string (volume_bsd->mount_point, mount_point, sizeof (mount_point), error) < 0)
    return FALSE;

  /* generate the command line for the umount command */
  quoted = g_shell_quote (mount_point);
  command_line = g_strconcat ("umount ", quoted, NULL);
  g_free (quoted);

  /* execute the umount command */
  result = g_spawn_command_line_sync (command_line, NULL, &standard_error, &exit_status, error);
  if (G_LIKELY (result))
    {
      /* check if the command failed */
      if (G_UNLIKELY (exit_status != 0))
        {
          /* drop additional whitespace from the stderr output */
          g_strstrip (standard_error);

          /* generate an error from the standard_error content */
          if (G_LIKELY (g_str_has_prefix (standard_error, "umount: ")))
            {
              /* strip off the "umount: " prefix */
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s", standard_error + 8);
            }
          else
            {
              /* use the error message as-is, not nice, but hey, better than nothing */
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s", standard_error);
            }


          /* and yes, we failed */
          result = FALSE;
        }

      /* release the stderr output */
      g_free (standard_error);
    }

  /* cleanup */
  g_free (command_line);

  /* update volume state if successfull */
  if (G_LIKELY (result))
    thunar_vfs_volume_bsd_update (volume_bsd);

  return result;
}



static gboolean
thunar_vfs_volume_bsd_update (gpointer user_data)
{
  ThunarVfsVolumeStatus status = 0;
  struct ioc_toc_header ith;
  ThunarVfsVolumeBSD   *volume_bsd = THUNAR_VFS_VOLUME_BSD (user_data);
  struct stat           sb;
  gchar                *label;
  gchar                 buffer[2048];
  int                   fd;

  if (volume_bsd->kind == THUNAR_VFS_VOLUME_KIND_CDROM)
    {
      /* try to read the table of contents from the CD-ROM,
       * which will only succeed if a disc is present for
       * the drive.
       */
      fd = open (volume_bsd->device_path, O_RDONLY);
      if (fd >= 0)
        {
          if (ioctl (fd, CDIOREADTOCHEADER, &ith) >= 0)
            {
              status |= THUNAR_VFS_VOLUME_STATUS_PRESENT;

              /* read the label of the disc */
              if (volume_bsd->label == NULL && (volume_bsd->status & THUNAR_VFS_VOLUME_STATUS_PRESENT) == 0)
                {
                  /* skip to sector 16 and read it */
                  if (lseek (fd, 16 * 2048, SEEK_SET) >= 0 && read (fd, buffer, sizeof (buffer)) >= 0)
                    {
                      /* offset 40 contains the volume identifier */
                      label = buffer + 40;
                      label[32] = '\0';
                      g_strchomp (label);
                      if (G_LIKELY (*label != '\0'))
                        volume_bsd->label = g_strdup (label);
                    }
                }
            }

          close (fd);
        }
    }

  /* determine the absolute path to the mount point */
  if (thunar_vfs_path_to_string (volume_bsd->mount_point, buffer, sizeof (buffer), NULL) > 0)
    {
      /* query the file system information for the mount point */
      if (statfs (buffer, &volume_bsd->info) >= 0)
        {
          /* if the device is mounted, it means that a medium is present */
          if (exo_str_is_equal (volume_bsd->info.f_mntfromname, volume_bsd->device_path))
            status |= THUNAR_VFS_VOLUME_STATUS_MOUNTED | THUNAR_VFS_VOLUME_STATUS_PRESENT;
        }

      /* free the volume label if no disc is present */
      if ((status & THUNAR_VFS_VOLUME_STATUS_PRESENT) == 0)
        {
          g_free (volume_bsd->label);
          volume_bsd->label = NULL;
        }

      /* determine the device id if mounted */
      if ((status & THUNAR_VFS_VOLUME_STATUS_MOUNTED) != 0)
        {
          if (stat (buffer, &sb) < 0)
            volume_bsd->device_id = (ThunarVfsFileDevice) -1;
          else
            volume_bsd->device_id = sb.st_dev;
        }
    }

  /* update the status if necessary */
  if (status != volume_bsd->status)
    {
      volume_bsd->status = status;
      thunar_vfs_volume_changed (THUNAR_VFS_VOLUME (volume_bsd));
    }

  return TRUE;
}



static ThunarVfsVolumeBSD*
thunar_vfs_volume_bsd_new (const gchar *device_path,
                           const gchar *mount_path)
{
  ThunarVfsVolumeBSD *volume_bsd;
  const gchar        *p;

  g_return_val_if_fail (device_path != NULL, NULL);
  g_return_val_if_fail (mount_path != NULL, NULL);

  /* allocate the volume object */
  volume_bsd = g_object_new (THUNAR_VFS_TYPE_VOLUME_BSD, NULL);
  volume_bsd->device_path = g_strdup (device_path);
  volume_bsd->mount_point = thunar_vfs_path_new (mount_path, NULL);

  /* determine the device name */
  for (p = volume_bsd->device_name = volume_bsd->device_path; *p != '\0'; ++p)
    if (p[0] == '/' && (p[1] != '/' && p[1] != '\0'))
      volume_bsd->device_name = p + 1;

  /* determine the kind of the volume */
  p = volume_bsd->device_name;
  if (p[0] == 'c' && p[1] == 'd' && g_ascii_isdigit (p[2]))
    volume_bsd->kind = THUNAR_VFS_VOLUME_KIND_CDROM;
  else if (p[0] == 'f' && p[1] == 'd' && g_ascii_isdigit (p[2]))
    volume_bsd->kind = THUNAR_VFS_VOLUME_KIND_FLOPPY;
  else if ((p[0] == 'a' && p[1] == 'd' && g_ascii_isdigit (p[2]))
        || (p[0] == 'd' && p[1] == 'a' && g_ascii_isdigit (p[2])))
    volume_bsd->kind = THUNAR_VFS_VOLUME_KIND_HARDDISK;
  else
    volume_bsd->kind = THUNAR_VFS_VOLUME_KIND_UNKNOWN;

  /* determine up-to-date status */
  thunar_vfs_volume_bsd_update (volume_bsd);

  /* start the update timer */
  volume_bsd->update_timer_id = g_timeout_add (1000, thunar_vfs_volume_bsd_update, volume_bsd);

  return volume_bsd;
}




static void             thunar_vfs_volume_manager_bsd_class_init         (ThunarVfsVolumeManagerBSDClass *klass);
static void             thunar_vfs_volume_manager_bsd_manager_init       (ThunarVfsVolumeManagerIface    *iface);
static void             thunar_vfs_volume_manager_bsd_init               (ThunarVfsVolumeManagerBSD      *manager_bsd);
static void             thunar_vfs_volume_manager_bsd_finalize           (GObject                        *object);
static ThunarVfsVolume *thunar_vfs_volume_manager_bsd_get_volume_by_info (ThunarVfsVolumeManager         *manager,
                                                                          const ThunarVfsInfo            *info);
static GList           *thunar_vfs_volume_manager_bsd_get_volumes        (ThunarVfsVolumeManager         *manager);



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
  iface->get_volume_by_info = thunar_vfs_volume_manager_bsd_get_volume_by_info;
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



static ThunarVfsVolume*
thunar_vfs_volume_manager_bsd_get_volume_by_info (ThunarVfsVolumeManager *manager,
                                                  const ThunarVfsInfo    *info)
{
  ThunarVfsVolumeManagerBSD *manager_bsd = THUNAR_VFS_VOLUME_MANAGER_BSD (manager);
  ThunarVfsVolumeBSD        *volume_bsd = NULL;
  GList                     *lp;

  for (lp = manager_bsd->volumes; lp != NULL; lp = lp->next)
    {
      volume_bsd = THUNAR_VFS_VOLUME_BSD (lp->data);
      if ((volume_bsd->status & THUNAR_VFS_VOLUME_STATUS_MOUNTED) != 0 && volume_bsd->device_id == info->device)
        return THUNAR_VFS_VOLUME (volume_bsd);
    }

  return NULL;
}



static GList*
thunar_vfs_volume_manager_bsd_get_volumes (ThunarVfsVolumeManager *manager)
{
  return THUNAR_VFS_VOLUME_MANAGER_BSD (manager)->volumes;
}



GType
_thunar_vfs_volume_manager_impl_get_type (void)
{
  return THUNAR_VFS_TYPE_VOLUME_MANAGER_BSD;
}



